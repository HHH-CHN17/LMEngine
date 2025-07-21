#include "AVRecorder.h"
#include <QDebug>
#include <qguiapplication.h>

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

static void avCheckRet(const char* operate, int ret)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    qCritical() << operate << " failed: " << err_buf << " (error code: " << ret << ")";
}


CAVRecorder::CAVRecorder(QObject* parent)
    : QObject(parent)
{
    av_register_all();
    avcodec_register_all();
}

CAVRecorder::~CAVRecorder()
{
    if (isRecording_) 
    {
        stopRecording();
    }
}

CAVRecorder* CAVRecorder::GetInstance()
{
    static CAVRecorder objAVRecorder{};

    return &objAVRecorder;
}

QAudioFormat CAVRecorder::initAudioFormat(const AVConfig& config)
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

bool CAVRecorder::initialize(const AVConfig& config)
{
    if (isRecording_) 
    {
        qWarning() << "Controller is busy. Please stop recording first.";
        return false;
    }

    cleanup();
    config_ = config;

    // ------------------------- muxer��ʼ�� -------------------------
    muxer_.reset(new CMuxer{});
    if (!muxer_->initialize(config_.filePath.c_str())) 
    {
        qCritical() << "Failed to initialize Muxer.";
        cleanup();
        return false;
    }

    // ------------------------- ��Ƶ��������ʼ�� -------------------------
    videoEncoder_.reset(new CVideoEncoder{});
    if (!videoEncoder_->initialize(config_.videoCfg)) 
    {
        qCritical() << "Failed to initialize Video Encoder.";
        cleanup();
        return false;
    }
    AVStream* videoStream = muxer_->addStream(videoEncoder_->getCodecContext());
    if (!videoStream)
    {
        qCritical() << "Failed to Add Video Stream.";
        return false;
    }
    videoEncoder_->setStream(videoStream);

    // ------------------------- ¼���豸��ʼ�� -------------------------
    audioCapturer_.reset(new CAudioCapturer{});
    QAudioFormat audioFormat = initAudioFormat(config);
    if (!audioCapturer_->initialize(audioFormat)) 
    {
        qCritical() << "Failed to initialize Audio Capturer.";
        cleanup();
        return false;
    }

    // ------------------------- ��Ƶ��������ʼ�� -------------------------
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
    AVStream* audioStream = muxer_->addStream(audioEncoder_->getCodecContext());
    if (!audioStream)
    {
        qCritical() << "Failed to Add Audio Stream.";
        return false;
    }
    audioEncoder_->setStream(audioStream);

    // ------------------------- д���ļ�ͷ -------------------------
    if (!muxer_->writeHeader()) 
    {
        qCritical() << "Failed to write muxer header.";
        cleanup();
        return false;
    }

    qInfo() << "Recorder Controller initialized successfully.";
    return true;
}

void CAVRecorder::startRecording()
{
    if (isRecording_) return;

    audioCapturer_->start(); // ��ʼ¼���������Ƶ������
    isRecording_ = true;
    qInfo() << "Recording started.";
}

void CAVRecorder::stopRecording()
{
    if (!isRecording_) return;

    isRecording_ = false;
    qInfo() << "Stopping recording...";

    audioCapturer_->stop(); // ֹͣ¼��

    qInfo() << "Flushing encoders...";

    // ------------------------- �����Ƶ���������� -------------------------
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    for (AVPacket* pkt : videoPackets) {
        muxer_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- �����Ƶ�������л��� -------------------------
    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) {
        muxer_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- �ر�muxer -------------------------
    muxer_->close();
    cleanup(); // ����������Դ
    qInfo() << "Recording stopped.";
}

bool CAVRecorder::recording(const unsigned char* rgbData)
{
    if (!isRecording_) return false;
    if (!rgbData) return false;

    // ------------------------- ��Ƶ���� -------------------------
    // 1. ��Ƶ����ֱ�ӱ���
    QVector<AVPacket*> videoPackets = videoEncoder_->encode(rgbData);
    for (AVPacket* pkt : videoPackets) 
    {
        muxer_->writePacket(pkt);
        //av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- ��Ƶ���� -------------------------
    // �������һ֡��Ƶ��Ҫ�����ֽ�
    const int audioBytesPerFrame = audioEncoder_->getBytesPerFrame();

    // ѭ�����������ڻ������л��۵�������Ƶ֡
    while (true) 
    {
        QByteArray pcmChunk;
        {
            // ��Ȼ��Ҫ���������� CAudioCapturer �������ķ��ʣ���Ϊ��������һ���̣߳�Qt�ڲ�������
            QMutexLocker locker{ &audioCapturer_->getMutex() };
            if (audioCapturer_->getBuffer().size() < audioBytesPerFrame) 
            {
                break; // ��Ƶ���ݲ���һ֡
            }
            pcmChunk = audioCapturer_->getBuffer().left(audioBytesPerFrame);
            audioCapturer_->getBuffer().remove(0, audioBytesPerFrame);
        }

        // 2. ��Ƶ��Ҫ�Ȼ�ȡPCM���ٱ���
        QVector<AVPacket*> audioPackets = audioEncoder_->encode(reinterpret_cast<const uint8_t*>(pcmChunk.constData()));
        for (AVPacket* pkt : audioPackets) 
        {
            qDebug() << "Muxer: Writing packet for stream index" << pkt->stream_index
                << "size:" << pkt->size
                << "pts:" << pkt->pts
                << "dts:" << pkt->dts;
            muxer_->writePacket(pkt);
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }

    return true;
}

bool CAVRecorder::isRecording() const
{
    return isRecording_;
}

void CAVRecorder::cleanup()
{
    muxer_.reset();
    videoEncoder_.reset();
    audioEncoder_.reset();
    audioCapturer_.reset();
}