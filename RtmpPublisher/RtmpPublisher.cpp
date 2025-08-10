#include "RtmpPublisher.h"
#include <QDebug>
#include <qguiapplication.h>

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
        rtmpPush_->sendVideo(pkt->data, pkt->size, pkt->dts, isKeyFrame);
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

	if (!codecCtx || !codecCtx->extradata || codecCtx->extradata_size <= 0 ||
		!(codecCtx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) ||
		codecCtx->codec_id != AV_CODEC_ID_H264) 
    {
			return false;

    }
	 const uint8_t* extradata = codecCtx->extradata;
	 int extradata_size = codecCtx->extradata_size;

    if (extradata_size < 8) { // 至少需要 AVCDecoderConfigurationRecord 的基本头部
        qCritical() << "Extradata too small.";
        return false;
    }

    // 检查是否是 AVCDecoderConfigurationRecord 格式 (通常第一个字节是版本号 1)
    if (extradata[0] != 1) {
        qCritical() << "Extradata is not in AVCDecoderConfigurationRecord format.";
        return false;
    }

    // 解析 AVCDecoderConfigurationRecord
    // 布局参考 ISO/IEC 14496-15
    // offset 0: configurationVersion (1 byte)
    // offset 1: AVCProfileIndication (1 byte)
    // offset 2: profile_compatibility (1 byte)
    // offset 3: AVCLevelIndication (1 byte)
    // offset 4: lengthSizeMinusOne (2 bits) + reserved (6 bits)
    // offset 5: numOfSequenceParameterSets (5 bits) + reserved (3 bits)
    int offset = 5;
    uint8_t num_sps = extradata[offset] & 0x1F;
    offset++;

    sps.clear();
    pps.clear();

    // 提取 SPS
    for (int i = 0; i < num_sps; ++i) 
    {
        if (offset + 2 > extradata_size) 
        {
            qCritical() << "Invalid SPS length in extradata.";
            return false;
        }
        uint16_t sps_len = (extradata[offset] << 8) | extradata[offset + 1];
        offset += 2;
        if (offset + sps_len > extradata_size) 
        {
            qCritical() << "SPS data exceeds extradata size.";
            return false;
        }
        // 通常只处理第一个 SPS
        if (sps.empty() && sps_len > 0) 
        {
            sps.assign(extradata + offset, extradata + offset + sps_len);
            // std::cout << "Extracted SPS, length: " << sps_len;
        }
        offset += sps_len;
    }

    // 提取 PPS
    if (offset + 1 > extradata_size) 
    {
        qCritical() << "Invalid PPS count in extradata.";
        return false;
    }
    uint8_t num_pps = extradata[offset];
    offset++;

    for (int i = 0; i < num_pps; ++i) 
    {
        if (offset + 2 > extradata_size) 
        {
            qCritical() << "Invalid PPS length in extradata.";
            return false;
        }
        uint16_t pps_len = (extradata[offset] << 8) | extradata[offset + 1];
        offset += 2;
        if (offset + pps_len > extradata_size) 
        {
            qCritical() << "PPS data exceeds extradata size.";
            return false;
        }
        // 通常只处理第一个 PPS
        if (pps.empty() && pps_len > 0)
        {
            pps.assign(extradata + offset, extradata + offset + pps_len);
            // std::cout << "Extracted PPS, length: " << pps_len;
        }
        offset += pps_len;
    }

	qDebug() << "Extracted SPS size:" << sps.size() << ", PPS size:" << pps.size();

    return !sps.empty() && !pps.empty();
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