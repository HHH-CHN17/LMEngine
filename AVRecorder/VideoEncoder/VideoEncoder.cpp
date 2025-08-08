#include "VideoEncoder.h"
#include <chrono>
#include <QObject>
#include <QDebug>
#include <libavutil/log.h>
#include <stdarg.h> 

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


CVideoEncoder::CVideoEncoder()
{
}

CVideoEncoder::~CVideoEncoder()
{
    cleanup();
}

bool CVideoEncoder::initialize(const VideoCodecCfg& cfg)
{
    cleanup(); // 清理旧的资源

    inWidth_ = cfg.in_width_;
    inHeight_ = cfg.in_height_;
    outWidth_ = cfg.out_width_;
    outHeight_ = cfg.out_height_;

    // 1. 查找 H.264 编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        qCritical() << "Video Encoder: H.264 codec not found.";
        return false;
    }

    // 2. 分配编码器上下文
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) {
        qCritical() << "Video Encoder: Could not allocate codec context.";
        return false;
    }

    // 3. 设置编码器参数
    codecCtx_->width = cfg.out_width_;
    codecCtx_->height = cfg.out_height_;
    //codecCtx_->bit_rate = cfg.bit_rate;
    codecCtx_->framerate = cfg.framerate_;
    codecCtx_->time_base = cfg.time_base_; // 时间基与帧率保持一致
    codecCtx_->gop_size = cfg.gop_size_; // 设置 GOP 大小，例如1秒一个I帧
    codecCtx_->max_b_frames = cfg.max_b_frames_;     // 允许B帧以提高压缩率
    codecCtx_->pix_fmt = cfg.pix_fmt_;
    codecCtx_->flags |= cfg.flags_;
    codecCtx_->codec_id = cfg.codec_id_;

    // 设置一些 H.264 的优化选项
    if (codec->id == AV_CODEC_ID_H264) {
        av_opt_set(codecCtx_->priv_data, "preset", "ultrafast", 0);
        av_opt_set(codecCtx_->priv_data, "tune", "zerolatency", 0);
    }

    // 4. 打开编码器
    int ret = avcodec_open2(codecCtx_, codec, nullptr);
    if (ret < 0) {
        avCheckRet("avcodec_open2", ret);
        qCritical() << "Video Encoder: Could not open codec.";
        cleanup();
        return false;
    }

    // 5. 初始化格式转换上下文 (SwsContext)
    swsCtx_ = sws_getContext(
        inWidth_, inHeight_, AV_PIX_FMT_RGBA,
        outWidth_, outHeight_, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!swsCtx_) {
        qCritical() << "Video Encoder: Could not create SwsContext.";
        cleanup();
        return false;
    }

    // 6. 分配用于存放 YUV 数据的 AVFrame
    yuvFrame_ = av_frame_alloc();
    if (!yuvFrame_) {
        qCritical() << "Video Encoder: Could not allocate YUV frame.";
        cleanup();
        return false;
    }
    yuvFrame_->format = AV_PIX_FMT_YUV420P;
    yuvFrame_->width = outWidth_;
    yuvFrame_->height = outHeight_;
    ret = av_frame_get_buffer(yuvFrame_, 0);
    if (ret < 0) {
        avCheckRet("av_frame_get_buffer", ret);
        qCritical() << "Video Encoder: Could not allocate buffer for YUV frame.";
        cleanup();
        return false;
    }

    ptsCnt_ = 0;
    qInfo() << "Video Encoder initialized successfully.";
    return true;
}

QVector<AVPacket*> CVideoEncoder::encode(const unsigned char* rgbData)
{
    if (!codecCtx_ || !swsCtx_ || !yuvFrame_) {
        return QVector<AVPacket*>{};
    }

    // 确保帧数据是可写的
    if (av_frame_make_writable(yuvFrame_) < 0) {
        qWarning() << "Video Encoder: YUV frame is not writable.";
        return QVector<AVPacket*>{};
    }

    // --- 1. 进行色彩空间转换和缩放 (RGB -> YUV) ---
    // 注意：这里我们假设输入的RGBA数据是上下颠倒的 (来自OpenGL)
    const uint8_t* const inData[1] = { rgbData + static_cast<ptrdiff_t>(inWidth_ * (inHeight_ - 1) * 4) }; // 指向最后一行，此时inData[0]存放了rgbData的最后一行数据的地址
    const int inLinesize[1] = { -inWidth_ * 4 }; // linesize为负，实现垂直翻转
    sws_scale(swsCtx_, inData, inLinesize, 0, inHeight_, yuvFrame_->data, yuvFrame_->linesize);

    // --- 2. 设置时间戳 (PTS) ---
    yuvFrame_->pts = ptsCnt_++;

    // --- 3. 调用核心编码函数 ---
    return doEncode(yuvFrame_);
}

QVector<AVPacket*> CVideoEncoder::flush()
{
    qInfo() << "Flushing Video Encoder...";
    // 传入 NULL frame 来清空编码器
    return doEncode(nullptr);
}

void CVideoEncoder::setStream(AVStream* stream)
{
    stream_ = stream;
    /* AVStream的时间基由muxer在avformat_write_header()时自动填入，不应该手动设置
    if (stream_) {
        stream_->time_base = codecCtx_->time_base;
    }*/
}

void CVideoEncoder::setTimeBase(AVRational timeBase)
{
    timeBase_ = timeBase;
}

QVector<AVPacket*> CVideoEncoder::doEncode(AVFrame* frame)
{
    QVector<AVPacket*> packetList;

    // 发送帧到编码器
    int ret = avcodec_send_frame(codecCtx_, frame);
    if (ret < 0) {
        qWarning() << "Video Encoder: Error sending frame to encoder.";
        return packetList;
    }

    // 循环接收所有可用的 packet
    while (ret >= 0) {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            qCritical() << "Video Encoder: Could not allocate AVPacket.";
            break;
        }

        ret = avcodec_receive_packet(codecCtx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
        {
            av_packet_unref(pkt);
            av_packet_free(&pkt); // 没有packet产出，释放分配的内存
            break; // 正常退出
        }
        else if (ret < 0) 
        {
            avCheckRet("avcodec_receive_packet", ret);
            qWarning() << "Video Encoder: Error receiving packet from encoder.";
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            break; // 发生错误，退出
        }

        // 设置每一个pkt的时间基和stream_index
        if (stream_) {
            av_packet_rescale_ts(pkt, codecCtx_->time_base, stream_->time_base);
            pkt->stream_index = stream_->index;
        }
        else if (timeBase_.den != 0)
        {
            av_packet_rescale_ts(pkt, codecCtx_->time_base, timeBase_);
        }
        else
        {
            qWarning() << "Video Encoder: No time_base.";
        }

        packetList.push_back(pkt);
    }
    return packetList;
}

void CVideoEncoder::cleanup()
{
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    if (yuvFrame_) {
        av_frame_free(&yuvFrame_);
        yuvFrame_ = nullptr;
    }
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    stream_ = nullptr;
}