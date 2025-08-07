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

QAudioFormat CRtmpPublisher::initAudioFormat(const AVConfig& config)
{
    QAudioFormat audioFormat;
    audioFormat.setSampleRate(config.audioCfg.audio_sample_rate);
    audioFormat.setChannelCount(config.audioCfg.audio_channel_count);
    audioFormat.setSampleSize(16);
    audioFormat.setSampleType(QAudioFormat::SignedInt);
    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setCodec("audio/pcm");

    return audioFormat;
}

bool CRtmpPublisher::initialize(const AVConfig& config)
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
    if (!rtmpPush_->connect(config_.path.c_str()))
    {
        qCritical() << "Failed to initialize rtmpPush.";
        cleanup();
        return false;
    }

    // ------------------------- 视频编码器初始化 -------------------------
    videoEncoder_.reset(new CVideoEncoder{});
    if (!videoEncoder_->initialize(config_.videoCfg))
    {
        qCritical() << "Failed to initialize Video Encoder.";
        cleanup();
        return false;
    }

    // ------------------------- 录音设备初始化 -------------------------
    audioCapturer_.reset(new CAudioCapturer{});
    QAudioFormat audioFormat = initAudioFormat(config);
    if (!audioCapturer_->initialize(audioFormat))
    {
        qCritical() << "Failed to initialize Audio Capturer.";
        cleanup();
        return false;
    }

    // ------------------------- 音频编码器初始化 -------------------------
    audioEncoder_.reset(new CAudioEncoder{});
    const QAudioFormat& finalAudioFormat = audioCapturer_->getAudioFormat();
    if (!audioEncoder_->initialize(
        finalAudioFormat.sampleRate(),
        finalAudioFormat.channelCount(),
        config_.audioCfg.audio_bitrate)
        )
    {
        qCritical() << "Failed to initialize Audio Encoder.";
        cleanup();
        return false;
    }

    // ------------------------- 检查是否已连接 -------------------------
    if (!rtmpPush_->isConnected())
    {
        qCritical() << "Failed to write muxer header.";
        cleanup();
        return false;
    }

    qInfo() << "Recorder Controller initialized successfully.";
    return true;
}

void CRtmpPublisher::startPush()
{
    if (isRecording_) return;

    audioCapturer_->start(); // 开始录音，填充音频缓冲区
    isRecording_ = true;
    qInfo() << "Recording started.";
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
        rtmpPush_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 清空音频编码器中缓存 -------------------------
    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) {
        rtmpPush_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 关闭rtmpPush -------------------------
    rtmpPush_->disconnect();
    cleanup(); // 清理所有资源
    qInfo() << "Recording stopped.";
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
        rtmpPush_->writePacket(pkt);
        //av_packet_unref(pkt);
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
            rtmpPush_->writePacket(pkt);
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }

    return true;
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