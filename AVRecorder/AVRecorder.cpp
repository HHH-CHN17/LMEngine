#include "AVRecorder.h"
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

/*void CAVRecorder::startRecording()
{
    if (isRecording_) return;

    audioCapturer_->start(); // 开始录音，填充音频缓冲区

	// ------------------------- 写入调试信息 -------------------------
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

	// ------------------------- 重置时间戳 -------------------------
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

/*void CAVRecorder::stopRecording()
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

    aacFile->close();
	h264File->close();
    cleanup(); // 清理所有资源
    qInfo() << "Recording stopped.";
}*/

void CAVRecorder::stopRecording() {
    if (!isRecording_.load()) {
        return;
    }

    qInfo() << "Stopping recording process...";

    // 虽然 `isRunning_` 也会设为 false，但 isRecording_ 更明确地表示录制意图
    isRecording_.store(false);

    audioCapturer_->stop(); // 停止录音

    // 停止并等待所有后台线程完成它们的工作（清空缓冲区等）
    stopThreads();

    // 【关键】在所有线程都已结束后，才能安全地执行最后的清理工作
    qInfo() << "Flushing final packets and closing muxer...";

    编码器中的残留帧需要在线程停止的时候在循环外面pop出来。
    // 清空编码器中可能残留的帧
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

    // 写入文件尾并关闭文件
    muxer_->close();

    // 释放所有FFmpeg相关的上下文资源
    cleanup();

    qInfo() << "Recording process stopped and resources cleaned up.";
}

bool CAVRecorder::recording(const unsigned char* rgbData)
{
    if (!isRecording_) return false;
    if (!rgbData) return false;

    // ------------------------- 视频编码 -------------------------
    // 1. 视频可以直接编码
    QVector<AVPacket*> videoPackets = videoEncoder_->encode(rgbData);
    for (AVPacket* pkt : videoPackets) 
    {
        //h264File->write(reinterpret_cast<const char*>(pkt->data), pkt->size);
        muxer_->writePacket(pkt);
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

    // --- 第1步：通知音频循环和UI线程停止产生新数据 ---
    // isRunning_ = false; 会让 audioEncodingLoop 退出。
    // isRecording_ = false; (在 stopRecording 中设置) 会让UI线程不再推入新的视频帧。

    // --- 第2步：向视频编码线程发送哨兵值 ---
    // 这是停机链的起点。
    // videoEncodingLoop 在 pop 出这个哨兵后会退出，
    // 并在退出前向 encodedPacketQueue_ 推入另一个哨兵，以通知 muxingLoop。
    /*qInfo() << "Sending EOS to video encoding queue...";
    RawVideoFrame eos_frame;
    eos_frame.is_end_of_stream = true;
    rawVideoQueue_.push(std::move(eos_frame));*/

    // --- 第3步：等待所有线程执行完毕 ---
    // .join() 的逻辑和之前完全一样，它确保我们等待这条“停机链”完全执行完毕。

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

MediaPacket应当取消

void CAVRecorder::videoEncodingLoop()
{
    qInfo() << "[Thread: VideoEncoder] Loop started.";
    RawVideoFrame rawFrame{};

    while (isRunning_.load(std::memory_order_relaxed))
    {
	    auto rawFramePtr = rawVideoQueue_.pop();
        if (!rawFramePtr) 
        {
            // 如果队列为空，短暂休眠，避免忙等
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue; // 继续下一次循环
        }
        // 从智能指针中移出数据，避免拷贝
		rawFrame = std::move(*rawFramePtr);

        QVector<AVPacket*> packets = videoEncoder_->encode(rawFrame.rgba_data.data());
        写成lambda函数，减少代码量
        // 3. 将编码后的裸指针AVPacket打包成MediaPacket并放入下一个队列
        for (AVPacket* pkt : packets)
        {
            if (!pkt) continue; // 安全检查

            MediaPacket mediaPkt;
            mediaPkt.pkt = AVPacketUPtr(pkt); // 将裸指针的所有权交给unique_ptr
            mediaPkt.type = PacketType::VIDEO;

            // 将包含 AVPacket 的 MediaPacket 移入队列，所有权再次转移
            encodedPacketQueue_.push(std::move(mediaPkt));
        }
    }

	// 线程结束后，编码剩余的帧，清空rawVideoQueue_中缓存
    while (auto rawFramePtr = rawVideoQueue_.pop())
    {
        // 从智能指针中移出数据，避免拷贝
        rawFrame = std::move(*rawFramePtr);

        QVector<AVPacket*> packets = videoEncoder_->encode(rawFrame.rgba_data.data());

        // 3. 将编码后的裸指针AVPacket打包成MediaPacket并放入下一个队列
        for (AVPacket* pkt : packets)
        {
            if (!pkt) continue; // 安全检查

            MediaPacket mediaPkt;
            mediaPkt.pkt = AVPacketUPtr(pkt); // 将裸指针的所有权交给unique_ptr
            mediaPkt.type = PacketType::VIDEO;

            // 将包含 AVPacket 的 MediaPacket 移入队列，所有权再次转移
            encodedPacketQueue_.push(std::move(mediaPkt));
        }
    }

    // 编码完剩余的帧后，调用flush()清空编码器缓存;
    QVector<AVPacket*> videoPackets = videoEncoder_->flush();
    
    for (AVPacket* pkt : videoPackets) 
    {
        if (!pkt) continue; // 安全检查

        MediaPacket mediaPkt;
        mediaPkt.pkt = AVPacketUPtr(pkt); // 将裸指针的所有权交给unique_ptr
        mediaPkt.type = PacketType::VIDEO;

        // 将包含 AVPacket 的 MediaPacket 移入队列，所有权再次转移
        encodedPacketQueue_.push(std::move(mediaPkt));
    }

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

    while (isRunning_.load(std::memory_order_relaxed)) 
    {
        // 1. 从音频捕获器（内部是我们的无锁环形缓冲区）读取数据
        QByteArray pcmChunk = audioCapturer_->readChunk(audioBytesPerFrame);

        // 2. 检查是否有足够的数据
        if (pcmChunk.size() < audioBytesPerFrame) 
        {
            continue;
        }

        // 3. 数据足够，进行编码
        QVector<AVPacket*> packets = audioEncoder_->encode(
            reinterpret_cast<const uint8_t*>(pcmChunk.constData())
        );

        // 4. 将编码后的包放入队列
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
        if (!pkt) continue; // 安全检查

        MediaPacket mediaPkt;
        mediaPkt.pkt = AVPacketUPtr(pkt); // 将裸指针的所有权交给unique_ptr
        mediaPkt.type = PacketType::AUDIO;

        // 将包含 AVPacket 的 MediaPacket 移入队列，所有权再次转移
        encodedPacketQueue_.push(std::move(mediaPkt));
    }

    qInfo() << "[Thread: AudioEncoder] Loop finished.";
}

void CAVRecorder::muxingLoop() {
    qInfo() << "[Thread: Muxer] Loop started.";
    //bool running = true;

    while (isRunning_.load(std::memory_order_relaxed)) {
        // 1. 从队列中取出编码好的数据包
        auto popped_packet_ptr = encodedPacketQueue_.pop();

        // 2. 如果队列为空，短暂休眠，避免忙等
        if (!popped_packet_ptr) {
            // 如果 isRunning_ 已经关闭，说明可能不会再有新数据了，
            // 但我们仍然需要等待哨兵包，所以继续循环。
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        muxer_->writePacket(popped_packet_ptr->pkt.get());

        /*MediaPacket mediaPacket = std::move(*popped_packet_ptr);

        // 3. 根据包类型进行处理
        switch (mediaPacket.type) {
        case PacketType::VIDEO:
        case PacketType::AUDIO:
            // 将包写入文件/网络。unique_ptr的 .get() 方法可以获取裸指针。
            muxer_->writePacket(mediaPacket.pkt.get());
            break;

        case PacketType::END_OF_STREAM:
            // 收到哨兵包，这是我们优雅退出的信号
            qInfo() << "[Thread: Muxer] Received EOS signal. Shutting down.";
            //running = false; // 设置循环退出标志
            break;
        }*/
    }

	// 4. 循环结束后，清空无锁队列中的所有剩余包
    while (auto popped_packet_ptr = encodedPacketQueue_.pop())
    {
        muxer_->writePacket(popped_packet_ptr->pkt.get());
    }

    qInfo() << "[Thread: Muxer] Loop finished.";
}

void CAVRecorder::enqueueVideoFrame(const unsigned char* rgbaData) {
    // 1. 检查录制状态，如果已停止，则忽略新来的帧
    if (!isRecording_.load(std::memory_order_relaxed) || !rgbaData) {
        return;
    }

    // 2. 检查队列是否已满（这是一个软检查，可以防止过度积压）
    if (rawVideoQueue_.isFull()) {
        qWarning() << "Video queue is full, dropping frame to reduce latency.";
        return;
    }

    // 3. 创建一个新的 RawVideoFrame
    RawVideoFrame frame;
    frame.width = config_.videoCodecCfg_.in_width_;
    frame.height = config_.videoCodecCfg_.in_height_;
	frame.is_end_of_stream = false;

    // 4. 【核心】拷贝数据
    // rgbaData 是指向PBO映射内存的指针，该内存很快就会被重用。
    // 我们必须将数据拷贝到自己的缓冲区中，才能安全地传递给后台线程。
    const size_t dataSize = static_cast<size_t>(frame.width) * frame.height * 4;
    frame.rgba_data.resize(dataSize);
    memcpy(frame.rgba_data.data(), rgbaData, dataSize);

    // 5. 将包含数据的帧对象移入无锁队列
    rawVideoQueue_.push(std::move(frame));
}