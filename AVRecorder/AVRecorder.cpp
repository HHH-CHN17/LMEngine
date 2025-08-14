#include "AVRecorder.h"

#include <QDebug>
#include <qguiapplication.h>

using namespace std;

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

static void avCheckRet(const char* operate, int ret)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    qCritical() << operate << " failed: " << err_buf << " (error code: " << ret << ")";
}


CAVRecorder::CAVRecorder()
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

QAudioFormat CAVRecorder::setAudioFormat(const AudioFormat& config)
{
    QAudioFormat audioFormat;
    audioFormat.setSampleRate(config.sample_rate_);
    audioFormat.setChannelCount(config.channels_);
    audioFormat.setSampleSize(config.sample_size_);
    audioFormat.setSampleType(config.sample_fmt_);
    audioFormat.setByteOrder(config.byte_order_);
    audioFormat.setCodec(config.codec_);

    return audioFormat;
}

bool CAVRecorder::initialize(AVConfig& config)
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
    if (!muxer_->initialize(config_.path_.c_str())) 
    {
        qCritical() << "Failed to initialize Muxer.";
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
    AVStream* videoStream = muxer_->addStream(videoEncoder_->getCodecContext());
    if (!videoStream)
    {
        qCritical() << "Failed to Add Video Stream.";
        return false;
    }
    videoEncoder_->setStream(videoStream);

    // ------------------------- ¼���豸��ʼ�� -------------------------
    audioCapturer_.reset(new CAudioCapturer{});
    QAudioFormat audioFormat = setAudioFormat(config.audioFmt_);
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

void CAVRecorder::startRecording() {
    if (isRecording_.load()) 
    {
        qWarning() << "Recording is already in progress.";
        return;
    }

    // 1. ����ʱ���
    videoEncoder_->resetTimestamp();
    audioEncoder_->resetTimestamp();

    // 2. ���ÿ��Ʊ�־
    isRecording_.store(true);
	isRunning_.store(true);  // �����߳�����

    // 3. �������к�̨�߳�
    startThreads();

    // 4. ������Ƶ�ɼ��豸
    audioCapturer_->start();

    qInfo() << "Asynchronous recording process started.";
}

void CAVRecorder::stopRecording() {
    if (!isRecording_.exchange(false)) {
        return;
    }
    qInfo() << "Stopping recording process...";

    audioCapturer_->stop(); // ֹͣ¼��

    stopThreads();

    qInfo() << "Flushing final packets and closing muxer...";
    muxer_->close();
    cleanup();

    //aacFile->close();
    //h264File->close();

    qInfo() << "Recording process stopped and resources cleaned up.";
}

void CAVRecorder::pushRGBA(const unsigned char* rgbaData) {
    // 1. ���¼��״̬�������ֹͣ�������������֡
    if (!isRecording_.load(std::memory_order_relaxed) || !rgbaData) {
        return;
    }

    // 2. �������Ƿ�����������һ�����飬���Է�ֹ���Ȼ�ѹ��
    if (rawVideoQueue_.isFull()) {
        qWarning() << "Video queue is full, dropping frame to reduce latency.";
        return;
    }

	// ����ʹ��unique_ptr��ֻ��Ҫ����һ�ζ��ڴ棬���ֱ��ʹ��vector���ᷢ�����ζ��ڴ���䣺һ���ڴ˴���һ����lock_free_queue��push�����С�
    auto uptr_rgba = std::make_unique<std::vector<uint8_t>>();
    const size_t dataSize = static_cast<size_t>(config_.videoCodecCfg_.in_width_) * config_.videoCodecCfg_.in_height_ * 4;
    uptr_rgba->resize(dataSize);
    memcpy(uptr_rgba->data(), rgbaData, dataSize);

    // 5. ���������ݵ�֡����������������
    rawVideoQueue_.push(std::move(uptr_rgba));
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

// ------------------------- �첽���� -------------------------

void CAVRecorder::startThreads() {
    qInfo() << "Starting background threads...";

    // ÿ���߳��������������ʼִ�����Ӧ�� Loop ����
    videoEncoderThread_ = std::thread(&CAVRecorder::videoEncodingLoop, this);
    audioEncoderThread_ = std::thread(&CAVRecorder::audioEncodingLoop, this);
    muxerThread_ = std::thread(&CAVRecorder::muxingLoop, this);
}

void CAVRecorder::stopThreads() {
    if (!isRunning_.exchange(false)) {
        return;
    }

    qInfo() << "Signaling all threads to stop...";

    // isRunning_ = false; ����Ƶ�����߳� �˳���
	// ���յ�����EOS����muxer�̻߳��˳���
    // isRecording_ = false; (�� stopRecording ������) ����UI�̲߳��������µ���Ƶ֡��

    if (audioEncoderThread_.joinable()) {
        audioEncoderThread_.join();
        qInfo() << "Audio encoder thread joined.";
    }
    if (videoEncoderThread_.joinable()) {
        videoEncoderThread_.join();
        qInfo() << "Video encoder thread joined.";
    }
    if (muxerThread_.joinable()) {
        muxerThread_.join();
        qInfo() << "Muxer thread joined.";
    }

    qInfo() << "All background threads have been successfully joined.";
}

void CAVRecorder::sendVecPkt(const QVector<AVPacket*>& packets, const PacketType& type)
{
    for (AVPacket* pkt : packets)
    {
        if (!pkt)
        {
            qDebug() << "Received a null packet, skipping.";
			continue;
        }

		MediaPacket mediaPkt{ AVPacketUPtr{ pkt }, type };

        // ������ AVPacket �� MediaPacket ������У�����Ȩ�ٴ�ת��
        encodedPktQueue_.push(std::move(mediaPkt));
    }
}

void CAVRecorder::videoEncodingLoop()
{
    qInfo() << "[Thread: VideoEncoder] Loop started.";

    // ------------------------- �߳���ѭ�� -------------------------
    while (isRunning_.load(std::memory_order_relaxed))
    {
		// pop������ֵ������unique_ptr����һ����lock_free_queue���������ڶ����Ǵ洢RGBA���ݵ�unique_ptr
        auto container = rawVideoQueue_.pop();
        if (!container) 
        {
            // �������Ϊ�գ��������ߣ�����æ��
            std::this_thread::yield();
            continue;
		}
        RGBAUPtr& pRawData = *container;
        if (!pRawData) 
        {
            // �������Ϊ�գ��������ߣ�����æ��
            std::this_thread::sleep_for(5ms);
            continue;
        }

        sendVecPkt(videoEncoder_->encode(pRawData->data()), PacketType::VIDEO);
    }

    // ------------------------- �߳̽��������rawVideoQueue_�л��� -------------------------
    while (auto container = rawVideoQueue_.pop())
    {
        RGBAUPtr& uptrRgba = *container;
        sendVecPkt(videoEncoder_->encode(uptrRgba->data()), PacketType::VIDEO);
    }

    // ------------------------- ��ն��л���󣬵���flush()��ձ��������� -------------------------
    sendVecPkt(videoEncoder_->flush(), PacketType::VIDEO);

    // ------------------------- ������ڱ�������ʾ��ǰ���ı������̽��� -------------------------
    MediaPacket eosPkt{ AVPacketUPtr{ nullptr }, PacketType::END_OF_STREAM };
    encodedPktQueue_.push(std::move(eosPkt));

    qInfo() << "[Thread: VideoEncoder] Loop finished.";
}

void CAVRecorder::audioEncodingLoop()
{
    qInfo() << "[Thread: AudioEncoder] Loop started.";

    // ��ȡ����һ֡��Ƶ�����ȷ���ֽ���
    const int audioBytesPerFrame = audioEncoder_->getBytesPerFrame();
    if (audioBytesPerFrame <= 0)
    {
        qCritical() << "[Thread: AudioEncoder] Invalid audio bytes per frame. Thread will not run.";
        return;
    }

    // ------------------------- �߳���ѭ�� -------------------------
    while (isRunning_.load(std::memory_order_relaxed))
    {
        QByteArray pcmChunk = audioCapturer_->readChunk(audioBytesPerFrame);
        if (pcmChunk.isEmpty() || pcmChunk.size() < audioBytesPerFrame)
        {
            std::this_thread::yield();
            continue;
        }
            
        sendVecPkt(
            audioEncoder_->encode(reinterpret_cast<const uint8_t*>(pcmChunk.constData())),
            PacketType::AUDIO
        );
    }

    // ------------------------- �߳̽��������pcm������ -------------------------
    while (true)
    {
        QByteArray pcmChunk = audioCapturer_->readChunk(audioBytesPerFrame);
        if (pcmChunk.isEmpty() || pcmChunk.size() < audioBytesPerFrame)
            break;
        sendVecPkt(
            audioEncoder_->encode(reinterpret_cast<const uint8_t*>(pcmChunk.constData())),
            PacketType::AUDIO
        );
    }

    // ------------------------- ���pcm����󣬵���flush()��ձ��������� -------------------------
    sendVecPkt(audioEncoder_->flush(), PacketType::AUDIO);

    // ------------------------- ������ڱ�������ʾ��ǰ���ı������̽��� -------------------------
    MediaPacket eosPkt{ AVPacketUPtr{ nullptr }, PacketType::END_OF_STREAM };
    encodedPktQueue_.push(std::move(eosPkt));

    qInfo() << "[Thread: AudioEncoder] Loop finished.";
}

void CAVRecorder::muxingLoop() {
    qInfo() << "[Thread: Muxer] Loop started.";

	int streamFin = 0; // ��¼����ɵ�������
	int streamTotal = muxer_->getFormatContext()->nb_streams; // ��ȡ��������

    // ------------------------- �߳���ѭ�� -------------------------
    while (streamFin < streamTotal)
    {
        // pop������ֵ������unique_ptr����һ����lock_free_queue���������ڶ����Ǵ洢packet��unique_ptr
        auto container = encodedPktQueue_.pop();
        if (!container)
        {
            // �������Ϊ�գ��������ߣ�����æ��
            std::this_thread::yield();
            continue;
        }
        MediaPacket& upPkt = *container;

        switch (upPkt.type)
        {
        case PacketType::VIDEO:
        case PacketType::AUDIO:
			muxer_->writePacket(upPkt.pkt.get());
            break;
        case PacketType::END_OF_STREAM:
            qInfo() << "[Thread: Muxer] Received end of stream packet.";
			++streamFin;
			break;
        }
    }

    qInfo() << "[Thread: Muxer] Loop finished.";
}
