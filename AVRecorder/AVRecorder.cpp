#include "AVRecorder.h"
#include <QDebug>
#include <qguiapplication.h>

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

/*void CAVRecorder::startRecording()
{
    if (isRecording_) return;

    audioCapturer_->start(); // ��ʼ¼���������Ƶ������

	// ------------------------- д�������Ϣ -------------------------
	h264File = new QFile(qApp->applicationDirPath() + "/" + "h264_data.h264");
    if (h264File->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qInfo() << "H264 file opened for writing.";
    } else {
        qCritical() << "Failed to open H264 file for writing.";
	}
    //h264File->write(reinterpret_cast<const char*>(videoEncoder_->getCodecContext()->extradata), videoEncoder_->getCodecContext()->extradata_size);

	aacFile = new QFile(qApp->applicationDirPath() + "/" + "aac_data.aac");
    if (aacFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qInfo() << "AAC file opened for writing.";
    } else {
		qCritical() << "Failed to open AAC file for writing.";
	}
	//aacFile->write(reinterpret_cast<const char*>(audioEncoder_->getCodecContext()->extradata), audioEncoder_->getCodecContext()->extradata_size);

	// ------------------------- ����ʱ��� -------------------------
    audioEncoder_->resetTimestamp();
    videoEncoder_->resetTimestamp();

    isRecording_ = true;
    qInfo() << "Recording started.";
}*/

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

/*void CAVRecorder::stopRecording()
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

    aacFile->close();
	h264File->close();
    cleanup(); // ����������Դ
    qInfo() << "Recording stopped.";
}*/

void CAVRecorder::stopRecording() {
    if (!isRecording_.load()) {
        return;
    }

    qInfo() << "Stopping recording process...";

    // ��Ȼ `isRunning_` Ҳ����Ϊ false���� isRecording_ ����ȷ�ر�ʾ¼����ͼ
    isRecording_.store(false);

    audioCapturer_->stop(); // ֹͣ¼��

    // ֹͣ���ȴ����к�̨�߳�������ǵĹ�������ջ������ȣ�
    stopThreads();

    // ���ؼ����������̶߳��ѽ����󣬲��ܰ�ȫ��ִ������������
    qInfo() << "Flushing final packets and closing muxer...";

    �������еĲ���֡��Ҫ���߳�ֹͣ��ʱ����ѭ������pop������
    // ��ձ������п��ܲ�����֡
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    for (AVPacket* pkt : videoPackets) {
        muxer_->writePacket(pkt);
        av_packet_free(&pkt);
    }
    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) {
        muxer_->writePacket(pkt);
        av_packet_free(&pkt);
    }

    // д���ļ�β���ر��ļ�
    muxer_->close();

    // �ͷ�����FFmpeg��ص���������Դ
    cleanup();

    qInfo() << "Recording process stopped and resources cleaned up.";
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
        //h264File->write(reinterpret_cast<const char*>(pkt->data), pkt->size);
        muxer_->writePacket(pkt);
        //av_packet_unref(pkt);
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
			//aacFile->write(reinterpret_cast<const char*>(pkt->data), pkt->size);
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

    // --- ��1����֪ͨ��Ƶѭ����UI�߳�ֹͣ���������� ---
    // isRunning_ = false; ���� audioEncodingLoop �˳���
    // isRecording_ = false; (�� stopRecording ������) ����UI�̲߳��������µ���Ƶ֡��

    // --- ��2��������Ƶ�����̷߳����ڱ�ֵ ---
    // ����ͣ��������㡣
    // videoEncodingLoop �� pop ������ڱ�����˳���
    // �����˳�ǰ�� encodedPacketQueue_ ������һ���ڱ�����֪ͨ muxingLoop��
    /*qInfo() << "Sending EOS to video encoding queue...";
    RawVideoFrame eos_frame;
    eos_frame.is_end_of_stream = true;
    rawVideoQueue_.push(std::move(eos_frame));*/

    // --- ��3�����ȴ������߳�ִ����� ---
    // .join() ���߼���֮ǰ��ȫһ������ȷ�����ǵȴ�������ͣ��������ȫִ����ϡ�

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

MediaPacketӦ��ȡ��

void CAVRecorder::videoEncodingLoop()
{
    qInfo() << "[Thread: VideoEncoder] Loop started.";
    RawVideoFrame rawFrame{};

    while (isRunning_.load(std::memory_order_relaxed))
    {
	    auto rawFramePtr = rawVideoQueue_.pop();
        if (!rawFramePtr) 
        {
            // �������Ϊ�գ��������ߣ�����æ��
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue; // ������һ��ѭ��
        }
        // ������ָ�����Ƴ����ݣ����⿽��
		rawFrame = std::move(*rawFramePtr);

        QVector<AVPacket*> packets = videoEncoder_->encode(rawFrame.rgba_data.data());
        д��lambda���������ٴ�����
        // 3. ����������ָ��AVPacket�����MediaPacket��������һ������
        for (AVPacket* pkt : packets)
        {
            if (!pkt) continue; // ��ȫ���

            MediaPacket mediaPkt;
            mediaPkt.pkt = AVPacketUPtr(pkt); // ����ָ�������Ȩ����unique_ptr
            mediaPkt.type = PacketType::VIDEO;

            // ������ AVPacket �� MediaPacket ������У�����Ȩ�ٴ�ת��
            encodedPacketQueue_.push(std::move(mediaPkt));
        }
    }

	// �߳̽����󣬱���ʣ���֡�����rawVideoQueue_�л���
    while (auto rawFramePtr = rawVideoQueue_.pop())
    {
        // ������ָ�����Ƴ����ݣ����⿽��
        rawFrame = std::move(*rawFramePtr);

        QVector<AVPacket*> packets = videoEncoder_->encode(rawFrame.rgba_data.data());

        // 3. ����������ָ��AVPacket�����MediaPacket��������һ������
        for (AVPacket* pkt : packets)
        {
            if (!pkt) continue; // ��ȫ���

            MediaPacket mediaPkt;
            mediaPkt.pkt = AVPacketUPtr(pkt); // ����ָ�������Ȩ����unique_ptr
            mediaPkt.type = PacketType::VIDEO;

            // ������ AVPacket �� MediaPacket ������У�����Ȩ�ٴ�ת��
            encodedPacketQueue_.push(std::move(mediaPkt));
        }
    }

    // ������ʣ���֡�󣬵���flush()��ձ���������;
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    
    for (AVPacket* pkt : videoPackets) 
    {
        if (!pkt) continue; // ��ȫ���

        MediaPacket mediaPkt;
        mediaPkt.pkt = AVPacketUPtr(pkt); // ����ָ�������Ȩ����unique_ptr
        mediaPkt.type = PacketType::VIDEO;

        // ������ AVPacket �� MediaPacket ������У�����Ȩ�ٴ�ת��
        encodedPacketQueue_.push(std::move(mediaPkt));
    }

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

    while (isRunning_.load(std::memory_order_relaxed)) 
    {
        // 1. ����Ƶ���������ڲ������ǵ��������λ���������ȡ����
        QByteArray pcmChunk = audioCapturer_->readChunk(audioBytesPerFrame);

        // 2. ����Ƿ����㹻������
        if (pcmChunk.size() < audioBytesPerFrame) 
        {
            continue;
        }

        // 3. �����㹻�����б���
        QVector<AVPacket*> packets = audioEncoder_->encode(
            reinterpret_cast<const uint8_t*>(pcmChunk.constData())
        );

        // 4. �������İ��������
        for (AVPacket* pkt : packets) 
        {
            if (!pkt) continue;

            MediaPacket mediaPkt;
            mediaPkt.pkt = AVPacketUPtr(pkt);
            mediaPkt.type = PacketType::AUDIO;
            encodedPacketQueue_.push(std::move(mediaPkt));
        }
    }

    QVector<AVPacket*> audioPackets = audioEncoder_->flush();
    for (AVPacket* pkt : audioPackets) 
    {
        if (!pkt) continue; // ��ȫ���

        MediaPacket mediaPkt;
        mediaPkt.pkt = AVPacketUPtr(pkt); // ����ָ�������Ȩ����unique_ptr
        mediaPkt.type = PacketType::AUDIO;

        // ������ AVPacket �� MediaPacket ������У�����Ȩ�ٴ�ת��
        encodedPacketQueue_.push(std::move(mediaPkt));
    }

    qInfo() << "[Thread: AudioEncoder] Loop finished.";
}

void CAVRecorder::muxingLoop() {
    qInfo() << "[Thread: Muxer] Loop started.";
    //bool running = true;

    while (isRunning_.load(std::memory_order_relaxed)) {
        // 1. �Ӷ�����ȡ������õ����ݰ�
        auto popped_packet_ptr = encodedPacketQueue_.pop();

        // 2. �������Ϊ�գ��������ߣ�����æ��
        if (!popped_packet_ptr) {
            // ��� isRunning_ �Ѿ��رգ�˵�����ܲ��������������ˣ�
            // ��������Ȼ��Ҫ�ȴ��ڱ��������Լ���ѭ����
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        muxer_->writePacket(popped_packet_ptr->pkt.get());

        /*MediaPacket mediaPacket = std::move(*popped_packet_ptr);

        // 3. ���ݰ����ͽ��д���
        switch (mediaPacket.type) {
        case PacketType::VIDEO:
        case PacketType::AUDIO:
            // ����д���ļ�/���硣unique_ptr�� .get() �������Ի�ȡ��ָ�롣
            muxer_->writePacket(mediaPacket.pkt.get());
            break;

        case PacketType::END_OF_STREAM:
            // �յ��ڱ������������������˳����ź�
            qInfo() << "[Thread: Muxer] Received EOS signal. Shutting down.";
            //running = false; // ����ѭ���˳���־
            break;
        }*/
    }

	// 4. ѭ��������������������е�����ʣ���
    while (auto popped_packet_ptr = encodedPacketQueue_.pop())
    {
        muxer_->writePacket(popped_packet_ptr->pkt.get());
    }

    qInfo() << "[Thread: Muxer] Loop finished.";
}

void CAVRecorder::enqueueVideoFrame(const unsigned char* rgbaData) {
    // 1. ���¼��״̬�������ֹͣ�������������֡
    if (!isRecording_.load(std::memory_order_relaxed) || !rgbaData) {
        return;
    }

    // 2. �������Ƿ�����������һ�����飬���Է�ֹ���Ȼ�ѹ��
    if (rawVideoQueue_.isFull()) {
        qWarning() << "Video queue is full, dropping frame to reduce latency.";
        return;
    }

    // 3. ����һ���µ� RawVideoFrame
    RawVideoFrame frame;
    frame.width = config_.videoCodecCfg_.in_width_;
    frame.height = config_.videoCodecCfg_.in_height_;
	frame.is_end_of_stream = false;

    // 4. �����ġ���������
    // rgbaData ��ָ��PBOӳ���ڴ��ָ�룬���ڴ�ܿ�ͻᱻ���á�
    // ���Ǳ��뽫���ݿ������Լ��Ļ������У����ܰ�ȫ�ش��ݸ���̨�̡߳�
    const size_t dataSize = static_cast<size_t>(frame.width) * frame.height * 4;
    frame.rgba_data.resize(dataSize);
    memcpy(frame.rgba_data.data(), rgbaData, dataSize);

    // 5. ���������ݵ�֡����������������
    rawVideoQueue_.push(std::move(frame));
}