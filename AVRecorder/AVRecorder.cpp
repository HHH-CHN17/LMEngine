#include "AVRecorder.h"
#include <QDebug>
#include <qguiapplication.h>


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

    // ------------------------- muxer初始化 -------------------------
    muxer_.reset(new CMuxer{});
    if (!muxer_->initialize(config_.filePath.c_str())) 
    {
        qCritical() << "Failed to initialize Muxer.";
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
    AVStream* videoStream = muxer_->addStream(videoEncoder_->getCodecContext());
    if (!videoStream)
    {
        qCritical() << "Failed to Add Video Stream.";
        return false;
    }
    videoEncoder_->setStream(videoStream);

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
    AVStream* audioStream = muxer_->addStream(audioEncoder_->getCodecContext());
    if (!audioStream)
    {
        qCritical() << "Failed to Add Audio Stream.";
        return false;
    }
    audioEncoder_->setStream(audioStream);

    // ------------------------- 写入文件头 -------------------------
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

    audioSamplesEncoded_ = 0;
    audioCapturer_->start(); // 开始录音，填充音频缓冲区
    isRecording_ = true;
    qInfo() << "Recording started.";
}

void CAVRecorder::stopRecording()
{
    if (!isRecording_) return;

    isRecording_ = false;
    qInfo() << "Stopping recording...";

    audioCapturer_->stop(); // 停止录音

    qInfo() << "Flushing encoders...";

    // ------------------------- 清空视频编码器缓存 -------------------------
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    for (AVPacket* pkt : videoPackets) {
        muxer_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 清空音频编码器中缓存 -------------------------
    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) {
        muxer_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 关闭muxer -------------------------
    muxer_->close();
    cleanup(); // 清理所有资源
    qInfo() << "Recording stopped.";
}

bool CAVRecorder::recording(const unsigned char* rgbData)
{
    if (!isRecording_) return false;
    if (!rgbData) return false;

    // ------------------------- 视频编码 -------------------------
    // 1. 视频可以直接编码
    QVector<AVPacket*> videoPackets = videoEncoder_->encode(rgbData, config_.videoCfg.inWidth * config_.videoCfg.inHeight * 4);
    for (AVPacket* pkt : videoPackets) 
    {
        muxer_->writePacket(pkt);
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }

    // ------------------------- 音频编码 -------------------------
    // 计算编码一帧音频需要多少字节
    const int audioBytesPerFrame = audioEncoder_->getBytesPerFrame();

    // 循环处理所有在缓冲区中积累的完整音频帧
    while (true) 
    {
        QByteArray pcmChunk;
        {
            // 仍然需要锁来保护对 CAudioCapturer 缓冲区的访问，因为它是由另一个线程（Qt内部）填充的
            QMutexLocker locker{ &audioCapturer_->getMutex() };
            if (audioCapturer_->getBuffer().size() < audioBytesPerFrame) 
            {
                break; // 音频数据不够一帧
            }
            pcmChunk = audioCapturer_->getBuffer().left(audioBytesPerFrame);
            audioCapturer_->getBuffer().remove(0, audioBytesPerFrame);
        }

        // 2. 音频需要先获取PCM，再编码
        QVector<AVPacket*> audioPackets = audioEncoder_->encode(reinterpret_cast<const uint8_t*>(pcmChunk.constData()), pcmChunk.size());
        for (AVPacket* pkt : audioPackets) 
        {
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