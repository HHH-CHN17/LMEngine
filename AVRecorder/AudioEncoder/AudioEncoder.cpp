#include "AVRecorder.h"
#include <chrono>
#include <QObject>
#include <QDebug>
#include <libavutil/log.h>
#include <stdarg.h> 

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

CAVRecorder* CAVRecorder::GetInstance()
{
    static CAVRecorder objAVRecorder{};

    return &objAVRecorder;
}


CAVRecorder::CAVRecorder()
{
    // debug用
    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback(ffmpeg_log_callback);

    av_register_all();
    avcodec_register_all();

}

CAVRecorder::~CAVRecorder()
{
    
}

void CAVRecorder::setInputWH(int w, int h)
{
    videoInWidth_ = w;
    videoInHeight_ = h;
	qDebug() << "setInputWH:" << videoInWidth_ << " " << videoInHeight_;
}

void CAVRecorder::setOutputWH(int w, int h)
{
    videoOutWidth_ = w;
    videoOutHeight_ = h;
	qDebug() << "setOutputWH:" << videoOutWidth_ << " " << videoOutHeight_;
}

bool CAVRecorder::initMuxer(const char* file)
{
    freeAll();

    // 1. 初始化复用器上下文
    avformat_alloc_output_context2(&avFormatCtx_, NULL, NULL, file);
    if (!avFormatCtx_) {
	    qDebug() << "avformat_alloc_output_context2 failed";
        return false;
    }

    filePath_ = file;

	// 2. 添加视频流
    assert(addVideoStream());

	// 3. 添加音频流
    // todo 添加音频流

    int ret = 0;
	// 4. 打开输出文件
    ret = avio_open(&avFormatCtx_->pb, file, AVIO_FLAG_WRITE);
    if (ret != 0) {
        avCheckRet("avio_open", ret);
	    qDebug() << "avio_open failed";
        return false;
    }

	// 5. 写入mp4文件头
	ret = avformat_write_header(avFormatCtx_, nullptr);
    if (ret != 0) {
        avCheckRet("avformat_write_header", ret);
	    qDebug() << "avformat_write_header failed";
        return false;
    }
    
    lastPts_ = startTimeStamp_ = getTickCount();

    isRecording_ = true;
    return true;
}

void CAVRecorder::initVideoCodecParams()
{
    videoCodecCtx_->width = videoOutWidth_;
    videoCodecCtx_->height = videoOutHeight_;

    AVRational time_base;
    time_base.num = 1; time_base.den = 1000;
    videoCodecCtx_->time_base = time_base;

    videoCodecCtx_->gop_size = 50;
    videoCodecCtx_->max_b_frames = 0;
    videoCodecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecCtx_->codec_id = AV_CODEC_ID_H264;
    av_opt_set(videoCodecCtx_->priv_data, "preset", "superfast", 0);
    videoCodecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

bool CAVRecorder::initVideoCodecCtx()
{
    if (!avFormatCtx_) {
        return false;
    }

	// 1. 查找视频编码器
    AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (videoCodec == NULL) {
        qDebug() << "avcodec_find_encoder failed";
        return false;
    }

    // 2. 初始化视频编码器上下文
    videoCodecCtx_ = avcodec_alloc_context3(videoCodec);
    if (!videoCodecCtx_) {
        qDebug() << "avcodec_alloc_context3 failed";
        return false;
    }

	// 3. 设置AVCodecParameters的参数
    initVideoCodecParams();

	// 4. 打开视频编码器
    // 注意：h.264要求视频宽高必须为偶数
    int ret = avcodec_open2(videoCodecCtx_, videoCodec, NULL);
    if (ret != 0) {
        avCheckRet("avcodec_open2", ret);
        avcodec_free_context(&videoCodecCtx_);
        return false;
    }
    qDebug() << "avcodec_open2 success...";

    av_dump_format(avFormatCtx_, 0, filePath_.c_str(), 1);

    // 5. 初始化AVPacket
    videoPkt_ = av_packet_alloc();
    if (!videoPkt_)
    {
        qDebug() << "av_packet_alloc failed";
        return false;
    }

    // 6. 初始化AVFrame
    yuvFrame_ = av_frame_alloc();
    if (!yuvFrame_) {
        qDebug() << "Could not allocate video frame";
        return false;
    }
    yuvFrame_->format = AV_PIX_FMT_YUV420P;
    yuvFrame_->width = videoOutWidth_;
    yuvFrame_->height = videoOutHeight_;
    yuvFrame_->pts = 0;
    if (av_frame_get_buffer(yuvFrame_, 0) != 0) {
        qDebug() << "Could not allocate the video frame data";
        return false;
    }

    return true;
}

bool CAVRecorder::addVideoStream()
{
    // 1. 初始化视频编码器上下文
	// 该函数需要初始化：videoCodecCtx_, yuvFrame_, videoPkt_
    assert(initVideoCodecCtx());

    // 2. 添加视频流
    videoStream_ = avformat_new_stream(avFormatCtx_, nullptr);
    if (!videoStream_) 
    {
        qDebug() << "avformat_new_stream failed";
        return false;
    }
    videoStream_->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(videoStream_->codecpar, videoCodecCtx_);

    // 3. 初始化视频格式转换上下文
    videoSwCtx_ = sws_getCachedContext(videoSwCtx_,
        videoInWidth_, videoInHeight_, AV_PIX_FMT_RGBA,
        videoOutWidth_, videoOutHeight_, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );
    if (!videoSwCtx_) 
    {
        qDebug() << "sws_getCachedContext failed";
        return false;
    }

    return true;
}

bool CAVRecorder::addAudioStream()
{
    if (avFormatCtx_ == NULL) {
        return false;
    }
    AVCodec* audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (audioCodec == NULL) {
        qDebug() << "avcodec_find_encoder failed";
        return false;
    }

    audioCodecCtx_ = avcodec_alloc_context3(audioCodec);
    if (audioCodecCtx_ == NULL) {
        qDebug() << " avcodec_alloc_context3 failed";
        return false;
    }

    audioCodecCtx_->sample_rate = m_audioOutSamplerate;
    audioCodecCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    audioCodecCtx_->channels = m_audioOutChannels;
    audioCodecCtx_->channel_layout = av_get_default_channel_layout(m_audioOutChannels);
    audioCodecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(audioCodecCtx_, audioCodec, NULL) != 0) {
        qDebug() << " avcodec_open2 failed";
        return false;
    }

    audioStream_ = avformat_new_stream(avFormatCtx_, NULL);
    if (audioStream_ == NULL) {
        qDebug() << " avformat_new_stream failed";
        return false;
    }

    audioStream_->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(audioStream_->codecpar, audioCodecCtx_);

    av_dump_format(avFormatCtx_, 0, filePath_.c_str(), 1);

    return true;
}

void CAVRecorder::stopRecord()
{
    isRecording_ = false;

    if (videoCodecCtx_) 
    {
		// 发送NULL帧，告诉编码器没有更多的帧了
        encodeVideo(nullptr);
    }

    {
        std::lock_guard<std::mutex> lg{ videoWriterMtx_ };
        endWriteMp4File();
        freeAll();
    }
}

bool CAVRecorder::encodeVideo(AVFrame* pFrame)
{
    int ret = 0;
    
	//编码
    ret = avcodec_send_frame(videoCodecCtx_, yuvFrame_);
	if (ret != 0) {
        avCheckRet("avcodec_send_frame", ret);
	    return false;
	}

	// 如果frame的发送顺序是I->B->B->B->P，那么packet的接受顺序是I->P->B->B->B
	// 这个while循环是为了在向编码器发送P帧后，接受P->B->B->B的包
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(videoCodecCtx_, videoPkt_);

        // 向编码器发送B帧后，返回EAGAIN，表示编码器需要更多输入（即需要P帧）
		// 向编码器发送NULL后，编码器会循环输出所有剩余的packet，当最后一个packet输出完后再次调用该函数时，返回AVERROR_EOF
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
        {
            av_packet_unref(videoPkt_); 
            return true;
        }
        else if (ret < 0 || videoPkt_->size <= 0) 
        {
            avCheckRet("avcodec_receive_packet", ret);
            av_packet_unref(videoPkt_);
            return false;
        }

        // 时间基转换
        av_packet_rescale_ts(videoPkt_, videoCodecCtx_->time_base, videoStream_->time_base);

        videoPkt_->stream_index = videoStream_->index;

        writeFrame(videoPkt_);

        av_packet_unref(videoPkt_);
    }

    return true;
}

bool CAVRecorder::recording(const unsigned char* rgbData)
{
    assert(isRecording_);
    assert(avFormatCtx_ || videoSwCtx_ || yuvFrame_);
    assert(rgbData);

    uint8_t* indata[AV_NUM_DATA_POINTERS] = { nullptr };
    indata[0] = const_cast<uint8_t*>(rgbData);

    int insize[AV_NUM_DATA_POINTERS] = { 0 };
    insize[0] = videoInWidth_ * 4;

    int ret = 0;
    ret = av_frame_make_writable(yuvFrame_);
    if (ret < 0)
    {
        avCheckRet("av_frame_make_writable", ret);
        return false;
    }

    ret = sws_scale(videoSwCtx_, indata, insize, 0, videoInHeight_, yuvFrame_->data, yuvFrame_->linesize);
    if (ret < 0) 
    {
        avCheckRet("sws_scale", ret);
        return false;
    }

    //解码出来是倒置的，这里把yuv做一个转换.
    yuvFrame_->data[0] += static_cast<ptrdiff_t>(yuvFrame_->linesize[0] * (videoCodecCtx_->height - 1));
    yuvFrame_->linesize[0] *= -1;
    yuvFrame_->data[1] += static_cast<ptrdiff_t>(yuvFrame_->linesize[1] * (videoCodecCtx_->height / 2 - 1));
    yuvFrame_->linesize[1] *= -1;
    yuvFrame_->data[2] += static_cast<ptrdiff_t>(yuvFrame_->linesize[2] * (videoCodecCtx_->height / 2 - 1));
    yuvFrame_->linesize[2] *= -1;

    long long currentPts = getTickCount() - startTimeStamp_;
    if (currentPts - lastPts_ <= 0) {
        currentPts = currentPts + 1;
    }
    yuvFrame_->pts = currentPts;

    lastPts_ = currentPts;

    assert(encodeVideo(yuvFrame_));

    return true;
}

bool CAVRecorder::writeFrame(AVPacket* packet)
{
    if (isRecording_ == false) {
        return false;
    }

    if (packet == NULL) {
        return false;
    }

    if (packet->data == NULL) {
        return false;
    }

    if (avFormatCtx_ == NULL || packet == NULL || packet->size <= 0) {
        return false;
    }
    int retValue = 0;
    {
        std::lock_guard<std::mutex> lg{ videoWriterMtx_ };
        retValue = av_interleaved_write_frame(avFormatCtx_, packet);
    }
    if (retValue != 0) {
	    qDebug() << "av_interleaved_write_frame failed :" << retValue;
        return false;
    }

    return true;
}

bool CAVRecorder::endWriteMp4File()
{
    if (avFormatCtx_ == NULL) {
        return false;
    }
    if (avFormatCtx_->pb == NULL) {
        return false;
    }

    if (av_write_trailer(avFormatCtx_) != 0) {
	    qDebug() << "av_write_trailer failed";
        return false;
    }

    if (avio_closep(&avFormatCtx_->pb) != 0) {
	    qDebug() << "avio_close failed";
        return false;
    }

    qDebug() << "endWriteMp4File success...";
    return true;

}

void CAVRecorder::freeAll()
{
    if (videoCodecCtx_) {
        avcodec_free_context(&videoCodecCtx_);
        videoCodecCtx_ = nullptr;
    }

    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
        audioCodecCtx_ = nullptr;
    }

    if (videoSwCtx_) {
        sws_freeContext(videoSwCtx_);
        videoSwCtx_ = nullptr;
    }

    if (videoPkt_) {
        av_packet_free(&videoPkt_);
        videoPkt_ = nullptr;
    }

    if (yuvFrame_) {
        av_frame_free(&yuvFrame_);
        yuvFrame_ = nullptr;
    }

    if (avFormatCtx_) {
        avformat_close_input(&avFormatCtx_);
        avFormatCtx_ = nullptr;
    }
}

long long CAVRecorder::getTickCount() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void CAVRecorder::avCheckRet(const char* operate, int ret)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    qDebug() << operate << " failed: " << err_buf << " (error code: " << ret << ")";
}
