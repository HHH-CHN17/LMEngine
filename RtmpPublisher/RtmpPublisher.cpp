#include "RtmpPublisher.h"
#include <QDebug>
#include <qguiapplication.h>
#include <algorithm>

#ifdef DEBUG
static void ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vargs)
{
    // �����־�������ǿ��Ժ���һЩ������ϸ����Ϣ
    if (level > av_log_get_level())
        return;

    // ����һ���㹻��Ļ��������洢��ʽ�������־��Ϣ
    char message[1024];

    // ʹ�� vsnprintf ��ȫ�ظ�ʽ����־����
    // FFmpeg ���ݵ� fmt ��ʽ�ַ���ͨ���Ѿ��������� "[libx264 @ ...]" ��������������Ϣ
    vsnprintf(message, sizeof(message), fmt, vargs);

    // ȥ����Ϣĩβ����Ļ��з�����ΪqDebug���Զ����
    size_t len = strlen(message);
    if (len > 0 && message[len - 1] == '\n') {
        message[len - 1] = '\0';
    }

    // ����FFmpeg����־����ѡ��ʹ��Qt�Ĳ�ͬ�����
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
            return it; // ������ʼ��Ŀ�ʼλ��
        }

        it = std::search(begin, end, std::begin(startCode3), std::end(startCode3));
        if (it != end) {
            return it; // ������ʼ��Ŀ�ʼλ��
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

    // ------------------------- rtmpPush��ʼ�� -------------------------
    rtmpPush_.reset(new CRtmpPush{});
    if (!rtmpPush_->connect(config_.path_.c_str()))
    {
        qCritical() << "Failed to initialize rtmpPush.";
        cleanup();
        return false;
    }

    // ------------------------- ��Ƶ��������ʼ�� -------------------------
    videoEncoder_.reset(new CVideoEncoder{});
    if (!videoEncoder_->initialize(config_.videoCodecCfg_))
    {
        qCritical() << "Failed to initialize Video Encoder.";
        cleanup();
        return false;
    }
    // �˴�����timebase���ڱ����packetʱ���Զ���ʱ���ת��Ϊ�˴����õ�ʱ���
    videoEncoder_->setTimeBase({ 1, 1000 });

    // ------------------------- ¼���豸��ʼ�� -------------------------
    audioCapturer_.reset(new CAudioCapturer{});
    QAudioFormat audioFormat = initAudioFormat(config.audioFmt_);
    if (!audioCapturer_->initialize(audioFormat, config.audioFmt_))
    {
        qCritical() << "Failed to initialize Audio Capturer.";
        cleanup();
        return false;
    }

    // ------------------------- ��Ƶ��������ʼ�� -------------------------
    audioEncoder_.reset(new CAudioEncoder{});
    //const QAudioFormat& finalAudioFormat = audioCapturer_->getAudioFormat();
    if (!audioEncoder_->initialize(config.audioCodecCfg_, config.audioFmt_))
    {
        qCritical() << "Failed to initialize Audio Encoder.";
        cleanup();
        return false;
    }
    audioEncoder_->setTimeBase({ 1, 1000 });

    // ------------------------- ����Ƿ������� -------------------------
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

    audioCapturer_->start(); // ��ʼ¼���������Ƶ������

    // ------------------------- ������ý������֮ǰ����Ҫ����H.264��AAC��������Ϣ -------------------------

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

    // ------------------------- ��Ƶ���� -------------------------
    // 1. ��Ƶ����ֱ�ӱ���
    QVector<AVPacket*> videoPackets = videoEncoder_->encode(rgbData);
    for (AVPacket* pkt : videoPackets)
    {
        // ������Ϊͬ�����̣�����Ϊ�첽����ʱ��Ҫ���
        bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        // �Ƴ�startCode
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

    // ------------------------- ��Ƶ���� -------------------------
    // �������һ֡��Ƶ��Ҫ�����ֽ�
    const int audioBytesPerFrame = audioEncoder_->getBytesPerFrame();
    //qDebug() << "audioBytesPerFrame: " << audioBytesPerFrame;
    // ѭ�����������ڻ������л��۵�������Ƶ֡
    while (true)
    {
        QByteArray pcmChunk;
        {
            pcmChunk = audioCapturer_->readChunk(audioBytesPerFrame);
            if (pcmChunk.isEmpty())
            {
                //qDebug() << "need more pcm data to encode";
                break; // ��Ƶ���ݲ���һ֡
            }
        }

        // 2. ��Ƶ��Ҫ�Ȼ�ȡPCM���ٱ���
        QVector<AVPacket*> audioPackets = audioEncoder_->encode(reinterpret_cast<const uint8_t*>(pcmChunk.constData()));
        for (AVPacket* pkt : audioPackets)
        {
            /*qDebug() << "Muxer: Writing packet for stream index" << pkt->stream_index
                << "size:" << pkt->size
                << "pts:" << pkt->pts
                << "dts:" << pkt->dts;*/
        	// ������Ϊͬ�����̣�����Ϊ�첽����ʱ��Ҫ���
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

    audioCapturer_->stop(); // ֹͣ¼��

    qInfo() << "Flushing encoders...";

    // ------------------------- �����Ƶ���������� -------------------------
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    for (AVPacket* pkt : videoPackets) {
        // ������Ϊͬ�����̣�����Ϊ�첽����ʱ��Ҫ���
        bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        rtmpPush_->sendVideo(pkt->data, pkt->size, pkt->dts, isKeyFrame);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- �����Ƶ�������л��� -------------------------
    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) {
        // ������Ϊͬ�����̣�����Ϊ�첽����ʱ��Ҫ���
        bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
        rtmpPush_->sendAudio(pkt->data, pkt->size, pkt->dts, isKeyFrame);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- �ر�rtmpPush -------------------------
    rtmpPush_->disconnect();
    cleanup(); // ����������Դ
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

    // 1. ��֤���������ĺ� extradata ����Ч��
    if (!codecCtx || !codecCtx->extradata || codecCtx->extradata_size <= 0 ||
        !(codecCtx->flags & AV_CODEC_FLAG_GLOBAL_HEADER) ||
        codecCtx->codec_id != AV_CODEC_ID_H264)
    {
        qWarning() << "getH264Config: Invalid video encoder context or extradata is not available/valid.";
        return false;
    }

    sps.clear();
    pps.clear();

    // �� extradata ������ vector �У�����ʹ�õ�������������
    const std::vector<uint8_t> vecExtraData(codecCtx->extradata, codecCtx->extradata + codecCtx->extradata_size);
    auto itExDataCurr = vecExtraData.cbegin();
    auto itExDataEnd = vecExtraData.cend();

    // 2. ѭ������ extradata�����Ҳ���������е� NAL ��Ԫ
    while (true) {
        // ���ҵ�ǰλ�õ� NAL ��Ԫ��ʼ��
        auto itNalBegin = find_h264_start_code(itExDataCurr, itExDataEnd);
        if (itNalBegin == itExDataEnd)
            break;
        
        size_t start_code_len = (itNalBegin + 3 < itExDataEnd && itNalBegin[3] == 0x01) ? 4 : 3;
        auto itDataBegin = itNalBegin + start_code_len;
        auto itNalEnd = find_h264_start_code(itDataBegin, itExDataEnd); // ��ǰ NAL ��Ԫ�Ľ���λ�ü���һ��NAL��Ԫ��ʼ���λ�û�����itExDataEnd

        // ��ȡ��ǰ NAL ��Ԫ������
        std::vector<uint8_t> vecNalData(itDataBegin, itNalEnd);

        if (!vecNalData.empty()) 
        {
            // ��ȡ NAL ��Ԫ���� (NAL Header �ĵ� 5 λ)
            uint8_t nalType = vecNalData[0] & 0x1F;

            // 3. ���� NAL ��Ԫ���ͽ��з���
            switch (nalType) {
            case 0x07: // SPS
                if (sps.empty()) // ֻ�����һ���ҵ��� SPS
                    sps = vecNalData;
                else
                    qWarning() << "getH264Config: Found multiple SPS NAL units, using the first one.";
                break;
            case 0x08: // PPS
                if (pps.empty()) // ֻ�����һ���ҵ��� PPS
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

        // ����Ѿ�ͬʱ�ҵ��� SPS �� PPS���Ϳ�����ǰ��������
        if (!sps.empty() && !pps.empty())
            break;

        // ����һ�� NAL ��Ԫ����ʼλ�ü�������
        itExDataCurr = itNalEnd;
    }

    // 4. ���ռ���Ƿ�ɹ���ȡ
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
    // ���� AAC��extradata ͨ������ AudioSpecificConfig
    // ��Ҳ�����ǰ��� MP4 Audio Decoder Specific Info �Ľṹ
    // �����ֱ�ӵľ��� extradata ����
    asc.assign(codecCtx->extradata, codecCtx->extradata + codecCtx->extradata_size);
    return true;

}