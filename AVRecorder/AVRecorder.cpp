#include "AVRecorder.h"

#include <QDebug>
#include <qguiapplication.h>

using namespace std;

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

    // ------------------------- muxer初始化 -------------------------
    muxer_.reset(new CMuxer{});
    if (!muxer_->initialize(config_.path_.c_str())) 
    {
        qCritical() << "Failed to initialize Muxer.";
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
    AVStream* videoStream = muxer_->addStream(videoEncoder_->getCodecContext());
    if (!videoStream)
    {
        qCritical() << "Failed to Add Video Stream.";
        return false;
    }
    videoEncoder_->setStream(videoStream);

    // ------------------------- 录音设备初始化 -------------------------
    audioCapturer_.reset(new CAudioCapturer{});
    QAudioFormat audioFormat = setAudioFormat(config.audioFmt_);
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

void CAVRecorder::startRecording() {
    if (isRecording_.load()) 
    {
        qWarning() << "Recording is already in progress.";
        return;
    }

    // 1. 重置时间戳
    videoEncoder_->resetTimestamp();
    audioEncoder_->resetTimestamp();

    // 2. 设置控制标志
    isRecording_.store(true);
	isRunning_.store(true);  // 允许线程运行

    // 3. 启动所有后台线程
    startThreads();

    // 4. 启动音频采集设备
    audioCapturer_->start();

    qInfo() << "Asynchronous recording process started.";
}

void CAVRecorder::stopRecording() {
    if (!isRecording_.exchange(false)) {
        return;
    }
    qInfo() << "Stopping recording process...";

    audioCapturer_->stop(); // 停止录音

    stopThreads();

    qInfo() << "Flushing final packets and closing muxer...";
    muxer_->close();
    cleanup();

    //aacFile->close();
    //h264File->close();

    qInfo() << "Recording process stopped and resources cleaned up.";
}

void CAVRecorder::pushRGBA(const unsigned char* rgbaData) {
    // 1. 检查录制状态，如果已停止，则忽略新来的帧
    if (!isRecording_.load(std::memory_order_relaxed) || !rgbaData) {
        return;
    }

    // 2. 检查队列是否已满（这是一个软检查，可以防止过度积压）
    if (rawVideoQueue_.isFull()) {
        qWarning() << "Video queue is full, dropping frame to reduce latency.";
        return;
    }

	// 这里使用unique_ptr，只需要分配一次堆内存，如果直接使用vector，会发生两次堆内存分配：一次在此处，一次在lock_free_queue的push函数中。
    auto uptr_rgba = std::make_unique<std::vector<uint8_t>>();
    const size_t dataSize = static_cast<size_t>(config_.videoCodecCfg_.in_width_) * config_.videoCodecCfg_.in_height_ * 4;
    uptr_rgba->resize(dataSize);
    memcpy(uptr_rgba->data(), rgbaData, dataSize);

    // 5. 将包含数据的帧对象移入无锁队列
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

// ------------------------- 异步改造 -------------------------

void CAVRecorder::startThreads() {
    qInfo() << "Starting background threads...";

    // 每个线程启动后会立即开始执行其对应的 Loop 函数
    videoEncoderThread_ = std::thread(&CAVRecorder::videoEncodingLoop, this);
    audioEncoderThread_ = std::thread(&CAVRecorder::audioEncodingLoop, this);
    muxerThread_ = std::thread(&CAVRecorder::muxingLoop, this);
}

void CAVRecorder::stopThreads() {
    if (!isRunning_.exchange(false)) {
        return;
    }

    qInfo() << "Signaling all threads to stop...";

    // isRunning_ = false; 音视频编码线程 退出。
	// 接收到两个EOS包后，muxer线程会退出。
    // isRecording_ = false; (在 stopRecording 中设置) 会让UI线程不再推入新的视频帧。

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

        // 将包含 AVPacket 的 MediaPacket 移入队列，所有权再次转移
        encodedPktQueue_.push(std::move(mediaPkt));
    }
}

void CAVRecorder::videoEncodingLoop()
{
    qInfo() << "[Thread: VideoEncoder] Loop started.";

    // ------------------------- 线程主循环 -------------------------
    while (isRunning_.load(std::memory_order_relaxed))
    {
		// pop出来的值有两层unique_ptr，第一层是lock_free_queue的容器，第二层是存储RGBA数据的unique_ptr
        auto container = rawVideoQueue_.pop();
        if (!container) 
        {
            // 如果队列为空，短暂休眠，避免忙等
            std::this_thread::yield();
            continue;
		}
        RGBAUPtr& pRawData = *container;
        if (!pRawData) 
        {
            // 如果队列为空，短暂休眠，避免忙等
            std::this_thread::sleep_for(5ms);
            continue;
        }

        sendVecPkt(videoEncoder_->encode(pRawData->data()), PacketType::VIDEO);
    }

    // ------------------------- 线程结束后，清空rawVideoQueue_中缓存 -------------------------
    while (auto container = rawVideoQueue_.pop())
    {
        RGBAUPtr& uptrRgba = *container;
        sendVecPkt(videoEncoder_->encode(uptrRgba->data()), PacketType::VIDEO);
    }

    // ------------------------- 清空队列缓存后，调用flush()清空编码器缓存 -------------------------
    sendVecPkt(videoEncoder_->flush(), PacketType::VIDEO);

    // ------------------------- 最后发送哨兵包，表示当前流的编码流程结束 -------------------------
    MediaPacket eosPkt{ AVPacketUPtr{ nullptr }, PacketType::END_OF_STREAM };
    encodedPktQueue_.push(std::move(eosPkt));

    qInfo() << "[Thread: VideoEncoder] Loop finished.";
}

void CAVRecorder::audioEncodingLoop()
{
    qInfo() << "[Thread: AudioEncoder] Loop started.";

    // 获取编码一帧音频所需的确切字节数
    const int audioBytesPerFrame = audioEncoder_->getBytesPerFrame();
    if (audioBytesPerFrame <= 0)
    {
        qCritical() << "[Thread: AudioEncoder] Invalid audio bytes per frame. Thread will not run.";
        return;
    }

    // ------------------------- 线程主循环 -------------------------
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

    // ------------------------- 线程结束后，清空pcm缓冲区 -------------------------
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

    // ------------------------- 清空pcm缓冲后，调用flush()清空编码器缓存 -------------------------
    sendVecPkt(audioEncoder_->flush(), PacketType::AUDIO);

    // ------------------------- 最后发送哨兵包，表示当前流的编码流程结束 -------------------------
    MediaPacket eosPkt{ AVPacketUPtr{ nullptr }, PacketType::END_OF_STREAM };
    encodedPktQueue_.push(std::move(eosPkt));

    qInfo() << "[Thread: AudioEncoder] Loop finished.";
}

void CAVRecorder::muxingLoop() {
    qInfo() << "[Thread: Muxer] Loop started.";

	int streamFin = 0; // 记录已完成的流数量
	int streamTotal = muxer_->getFormatContext()->nb_streams; // 获取总流数量

    // ------------------------- 线程主循环 -------------------------
    while (streamFin < streamTotal)
    {
        // pop出来的值有两层unique_ptr，第一层是lock_free_queue的容器，第二层是存储packet的unique_ptr
        auto container = encodedPktQueue_.pop();
        if (!container)
        {
            // 如果队列为空，短暂休眠，避免忙等
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
