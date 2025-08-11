#include "RtmpPublisher.h"
#include <QDebug>
#include <qguiapplication.h>
#include <algorithm>

#ifdef DEBUG
static void ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vargs)
{
    // 检查日志级别，我们可以忽略一些过于详细的信息
    if (level > av_log_get_level())
        return;

    // 创建一个足够大的缓冲区来存储格式化后的日志消息
    char message[1024];

    // 使用 vsnprintf 安全地格式化日志内容
    // FFmpeg 传递的 fmt 格式字符串通常已经包含了像 "[libx264 @ ...]" 这样的上下文信息
    vsnprintf(message, sizeof(message), fmt, vargs);

    // 去掉消息末尾多余的换行符，因为qDebug会自动添加
    size_t len = strlen(message);
    if (len > 0 && message[len - 1] == '\n') {
        message[len - 1] = '\0';
    }

    // 根据FFmpeg的日志级别，选择使用Qt的不同输出流
    switch (level) {
    case AV_LOG_PANIC:
    case AV_LOG_FATAL:
    case AV_LOG_ERROR:
        qCritical() << "FFmpeg:" << message;
        break;
    case AV_LOG_WARNING:
        qWarning() << "FFmpeg:" << message;
        break;
    case AV_LOG_INFO:
        qInfo() << "FFmpeg:" << message;
        break;
    case AV_LOG_VERBOSE:
    case AV_LOG_DEBUG:
    case AV_LOG_TRACE:
        qDebug() << "FFmpeg:" << message;
        break;
    default:
        qDebug() << "FFmpeg (Unknown Level):" << message;
        break;
    }
}
#endif // DEBUG 

namespace
{
    std::vector<uint8_t>::const_iterator
        find_h264_start_code(
            std::vector<uint8_t>::const_iterator& begin,
            std::vector<uint8_t>::const_iterator& end)
    {

        const uint8_t startCode3[] = { 0x00, 0x00, 0x01 };
        const uint8_t startCode4[] = { 0x00, 0x00, 0x00, 0x01 };

        auto it = std::search(begin, end, std::begin(startCode4), std::end(startCode4));
        if (it != end) {
            return it; // 返回起始码的开始位置
        }

        it = std::search(begin, end, std::begin(startCode3), std::end(startCode3));
        if (it != end) {
            return it; // 返回起始码的开始位置
        }

        return end;
    }

}

static void avCheckRet(const char* operate, int ret)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    qCritical() << operate << " failed: " << err_buf << " (error code: " << ret << ")";
}


CRtmpPublisher::CRtmpPublisher(QObject* parent)
    : QObject(parent)
{
    av_register_all();
    avcodec_register_all();
}

CRtmpPublisher::~CRtmpPublisher()
{
    if (isRecording_)
    {
        stopPush();
    }
}

CRtmpPublisher* CRtmpPublisher::GetInstance()
{
    static CRtmpPublisher objAVRecorder{};

    return &objAVRecorder;
}

QAudioFormat CRtmpPublisher::initAudioFormat(const AudioFormat& fmt)
{
    QAudioFormat audioFormat;
    audioFormat.setSampleRate(fmt.sample_rate_);
    audioFormat.setChannelCount(fmt.channels_);
    audioFormat.setSampleSize(fmt.sample_size_);
    audioFormat.setSampleType(fmt.sample_fmt_);
    audioFormat.setByteOrder(fmt.byte_order_);
    audioFormat.setCodec(fmt.codec_);

    return audioFormat;
}

bool CRtmpPublisher::initialize(AVConfig& config)
{
    if (isRecording_)
    {
        qWarning() << "Controller is busy. Please stop pushing first.";
        return false;
    }

    cleanup();
    config_ = config;

    // ------------------------- rtmpPush初始化 -------------------------
    rtmpPush_.reset(new CRtmpPush{});
    if (!rtmpPush_->connect(config_.path_.c_str()))
    {
        qCritical() << "Failed to initialize rtmpPush.";
        cleanup();
        return false;
    }

    // ------------------------- 视频编码器初始化 -------------------------
    videoEncoder_.reset(new CVideoEncoder{});
    if (!videoEncoder_->initialize(config_.videoCodecCfg_))
    {
        qCritical() << "Failed to initialize Video Encoder.";
        cleanup();
        return false;
    }
    // 此处设置timebase后，在编码成packet时会自动将时间基转换为此处设置的时间基
    videoEncoder_->setTimeBase({ 1, 1000 });

    // ------------------------- 录音设备初始化 -------------------------
    audioCapturer_.reset(new CAudioCapturer{});
    QAudioFormat audioFormat = initAudioFormat(config.audioFmt_);
    if (!audioCapturer_->initialize(audioFormat, config.audioFmt_))
    {
        qCritical() << "Failed to initialize Audio Capturer.";
        cleanup();
        return false;
    }

    // ------------------------- 音频编码器初始化 -------------------------
    audioEncoder_.reset(new CAudioEncoder{});
    //const QAudioFormat& finalAudioFormat = audioCapturer_->getAudioFormat();
    if (!audioEncoder_->initialize(config.audioCodecCfg_, config.audioFmt_))
    {
        qCritical() << "Failed to initialize Audio Encoder.";
        cleanup();
        return false;
    }
    audioEncoder_->setTimeBase({ 1, 1000 });

    // ------------------------- 检查是否已连接 -------------------------
    if (!rtmpPush_->isConnected())
    {
        qCritical() << "Failed to connect rtmp server.";
        cleanup();
        return false;
    }

    qInfo() << "RtmpPublisher initialized successfully.";
    return true;
}

void CRtmpPublisher::startPush()
{
    if (isRecording_) return;

    audioCapturer_->start(); // 开始录音，填充音频缓冲区

    // ------------------------- 发送流媒体数据之前，需要发送H.264和AAC的配置信息 -------------------------

    std::vector<uint8_t> sps{}, pps{}, asc{};
    if (!getH264Config(sps, pps))
    {
        qCritical() << "Failed to get H.264 config.";
        cleanup();
        return;
	}

    if (!getAacConfig(asc))
    {
        qCritical() << "Failed to get AAC config.";
        cleanup();
        return;
    }

    rtmpPush_->setAVConfig(
        sps.data(), sps.size(), 
        pps.data(), pps.size(), 
        asc.data(), asc.size()
    );

    isRecording_ = true;
    qInfo() << "Recording started.";
}

bool CRtmpPublisher::pushing(const unsigned char* rgbData)
{
    if (!isRecording_) return false;
    if (!rgbData) return false;

    // ------------------------- 视频编码 -------------------------
    // 1. 视频可以直接编码
    QVector<AVPacket*> videoPackets = videoEncoder_->encode(rgbData);
    for (AVPacket* pkt : videoPackets)
    {
        // 该流程为同步流程，随后改为异步流程时需要深拷贝
        bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        // 移除startCode
        uint8_t* data = pkt->data;
        assert(pkt->size >= 4);
        int startCodeLen = 0;
        if ((data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01))
        {
            startCodeLen = 3;
        }
        else if ((data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01))
        {
            startCodeLen = 4;
        }

        rtmpPush_->sendVideo(pkt->data + startCodeLen, pkt->size - startCodeLen, pkt->dts, isKeyFrame);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 音频编码 -------------------------
    // 计算编码一帧音频需要多少字节
    const int audioBytesPerFrame = audioEncoder_->getBytesPerFrame();
    //qDebug() << "audioBytesPerFrame: " << audioBytesPerFrame;
    // 循环处理所有在缓冲区中积累的完整音频帧
    while (true)
    {
        QByteArray pcmChunk;
        {
            pcmChunk = audioCapturer_->readChunk(audioBytesPerFrame);
            if (pcmChunk.isEmpty())
            {
                //qDebug() << "need more pcm data to encode";
                break; // 音频数据不够一帧
            }
        }

        // 2. 音频需要先获取PCM，再编码
        QVector<AVPacket*> audioPackets = audioEncoder_->encode(reinterpret_cast<const uint8_t*>(pcmChunk.constData()));
        for (AVPacket* pkt : audioPackets)
        {
            /*qDebug() << "Muxer: Writing packet for stream index" << pkt->stream_index
                << "size:" << pkt->size
                << "pts:" << pkt->pts
                << "dts:" << pkt->dts;*/
        	// 该流程为同步流程，随后改为异步流程时需要深拷贝
            rtmpPush_->sendAudio(pkt->data, pkt->size, pkt->dts, false);
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }

    return true;
}

void CRtmpPublisher::stopPush()
{
    if (!isRecording_) return;

    isRecording_ = false;
    qInfo() << "Stopping pushing...";

    audioCapturer_->stop(); // 停止录音

    qInfo() << "Flushing encoders...";

    // ------------------------- 清空视频编码器缓存 -------------------------
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    for (AVPacket* pkt : videoPackets) {
        // 该流程为同步流程，随后改为异步流程时需要深拷贝
        bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        rtmpPush_->sendVideo(pkt->data, pkt->size, pkt->dts, isKeyFrame);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 清空音频编码器中缓存 -------------------------
    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) {
        // 该流程为同步流程，随后改为异步流程时需要深拷贝
        bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        rtmpPush_->sendAudio(pkt->data, pkt->size, pkt->dts, isKeyFrame);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 关闭rtmpPush -------------------------
    rtmpPush_->disconnect();
    cleanup(); // 清理所有资源
    qInfo() << "Recording stopped.";
}


bool CRtmpPublisher::isRecording() const
{
    return isRecording_;
}

void CRtmpPublisher::cleanup()
{
    rtmpPush_.reset();
    videoEncoder_.reset();
    audioEncoder_.reset();
    audioCapturer_.reset();
}

bool CRtmpPublisher::getH264Config(std::vector<uint8_t>& sps, std::vector<uint8_t>& pps)
{
    const AVCodecContext* codecCtx = videoEncoder_->getCodecContext();

    // 1. 验证输入上下文和 extradata 的有效性
    if (!codecCtx || !codecCtx->extradata || codecCtx->extradata_size <= 0 ||
        !(codecCtx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) ||
        codecCtx->codec_id != AV_CODEC_ID_H264)
    {
        qWarning() << "getH264Config: Invalid video encoder context or extradata is not available/valid.";
        return false;
    }

    sps.clear();
    pps.clear();

    // 将 extradata 拷贝到 vector 中，便于使用迭代器进行搜索
    const std::vector<uint8_t> vecExtraData(codecCtx->extradata, codecCtx->extradata + codecCtx->extradata_size);
    auto itExDataCurr = vecExtraData.cbegin();
    auto itExDataEnd = vecExtraData.cend();

    // 2. 循环遍历 extradata，查找并分离出所有的 NAL 单元
    while (true) {
        // 查找当前位置的 NAL 单元起始码
        auto itNalBegin = find_h264_start_code(itExDataCurr, itExDataEnd);
        if (itNalBegin == itExDataEnd)
            break;
        
        size_t start_code_len = (itNalBegin + 3 < itExDataEnd && itNalBegin[3] == 0x01) ? 4 : 3;
        auto itDataBegin = itNalBegin + start_code_len;
        auto itNalEnd = find_h264_start_code(itDataBegin, itExDataEnd); // 当前 NAL 单元的结束位置即下一个NAL单元起始码的位置或者是itExDataEnd

        // 提取当前 NAL 单元的数据
        std::vector<uint8_t> vecNalData(itDataBegin, itNalEnd);

        if (!vecNalData.empty()) 
        {
            // 获取 NAL 单元类型 (NAL Header 的低 5 位)
            uint8_t nalType = vecNalData[0] & 0x1F;

            // 3. 根据 NAL 单元类型进行分配
            switch (nalType) {
            case 0x07: // SPS
                if (sps.empty()) // 只保存第一个找到的 SPS
                    sps = vecNalData;
                else
                    qWarning() << "getH264Config: Found multiple SPS NAL units, using the first one.";
                break;
            case 0x08: // PPS
                if (pps.empty()) // 只保存第一个找到的 PPS
                    pps = vecNalData;
                else
                    qWarning() << "getH264Config: Found multiple PPS NAL units, using the first one.";
                break;
            case 0x06: // SEI
                qDebug() << "getH264Config: Found and skipped SEI NAL unit.";
                break;
            default:
                qDebug() << "getH264Config: Found and skipped NAL unit of type" << nalType;
                break;
            }
        }

        // 如果已经同时找到了 SPS 和 PPS，就可以提前结束搜索
        if (!sps.empty() && !pps.empty())
            break;

        // 从下一个 NAL 单元的起始位置继续搜索
        itExDataCurr = itNalEnd;
    }

    // 4. 最终检查是否成功提取
    if (sps.empty() || pps.empty()) {
        qCritical() << "getH264Config: Failed to find both SPS and PPS in extradata.";
        return false;
    }

    qDebug() << "Successfully extracted SPS (size:" << sps.size() << ") and PPS (size:" << pps.size() << ")";
    return true;
}

bool CRtmpPublisher::getAacConfig(std::vector<uint8_t>& asc) {
	const AVCodecContext* codecCtx = audioEncoder_->getCodecContext();
    if (!codecCtx || !codecCtx->extradata || codecCtx->extradata_size <= 0 ||
        !(codecCtx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) ||
        codecCtx->codec_id != AV_CODEC_ID_AAC) 
    {
        return false;
    }
    // 对于 AAC，extradata 通常就是 AudioSpecificConfig
    // 但也可能是包含 MP4 Audio Decoder Specific Info 的结构
    // 最常见和直接的就是 extradata 本身
    asc.assign(codecCtx->extradata, codecCtx->extradata + codecCtx->extradata_size);
    return true;

}