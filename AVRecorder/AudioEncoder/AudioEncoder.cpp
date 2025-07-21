#include "AudioEncoder.h"
#include <chrono>
#include <QObject>
#include <QDebug>
#include <libavutil/log.h>
#include <stdarg.h> 

#include "Common/DataDefine.h"

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

static void avCheckRet(const char* operate, int ret)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    qCritical() << operate << " failed: " << err_buf << " (error code: " << ret << ")";
}


CAudioEncoder::CAudioEncoder()
{
}

CAudioEncoder::~CAudioEncoder()
{
    cleanup();
}

bool CAudioEncoder::initialize(int sampleRate, int channels, long long bitrate)
{
    cleanup();

    // 1. 查找 AAC 编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        qCritical() << "Audio Encoder: AAC codec not found.";
        return false;
    }

    // 2. 分配编码器上下文
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) {
        qCritical() << "Audio Encoder: Could not allocate codec context.";
        return false;
    }

    // 3. 设置编码器参数 (目标格式)
    codecCtx_->bit_rate = bitrate;
    codecCtx_->sample_rate = sampleRate;
    codecCtx_->channels = channels;
    codecCtx_->channel_layout = av_get_default_channel_layout(channels);
    // AAC 编码器通常使用 Planar 浮点数格式 (FLTP) 以获得最佳质量
    codecCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codecCtx_->time_base = { 1, sampleRate }; // 时间基以采样率为单位

    // 4. 打开编码器
    int ret = avcodec_open2(codecCtx_, codec, nullptr);
    if (ret < 0) {
        qCritical() << "Audio Encoder: Could not open codec.";
        cleanup();
        return false;
    }

    // 5. 初始化重采样上下文 (SwrContext)
    // 因为我们从 CAudioCapturer 接收的是 S16 Packed 格式，但编码器需要 FLTP Planar 格式
    swrCtx_ = swr_alloc_set_opts(nullptr,
        // 目标参数 (编码器想要的)
        codecCtx_->channel_layout, codecCtx_->sample_fmt, codecCtx_->sample_rate,
        // 源参数 (我们提供的)
        codecCtx_->channel_layout, AV_SAMPLE_FMT_S16, sampleRate,
        0, nullptr);
    if (!swrCtx_) 
    {
        qCritical() << "Audio Encoder: Could not create SwrContext.";
        cleanup();
        return false;
    }
    ret = swr_init(swrCtx_);
    if (ret < 0) 
    {
        avCheckRet("swr_init", ret);
        qCritical() << "Audio Encoder: Failed to initialize SwrContext.";
        cleanup();
        return false;
    }

    // 6. 分配用于存放重采样后数据的 AVFrame
    resampleFrame_ = av_frame_alloc();
    if (!resampleFrame_) 
    {
        qCritical() << "Audio Encoder: Could not allocate resampled frame.";
        cleanup();
        return false;
    }
    resampleFrame_->nb_samples = codecCtx_->frame_size;
    resampleFrame_->format = codecCtx_->sample_fmt;
    resampleFrame_->channel_layout = codecCtx_->channel_layout;
    ret = av_frame_get_buffer(resampleFrame_, 0);
    if (ret < 0) 
    {
        qCritical() << "Audio Encoder: Could not allocate buffer for resampled frame.";
        cleanup();
        return false;
    }

    ptsCnt_ = 0;
    qInfo() << "Audio Encoder initialized successfully. Frame size:" << codecCtx_->frame_size;
    return true;
}

QVector<AVPacket*> CAudioEncoder::encode(const unsigned char* pcmData)
{
    if (!codecCtx_ || !swrCtx_ || !resampleFrame_) 
    {
        return QVector<AVPacket*>{};
    }

    // --- 1. 进行重采样和格式转换 (S16 Packed -> FLTP Planar) ---
    // swr_convert 需要 const uint8_t** 类型的输入
    const uint8_t** inData = &pcmData;

    // 调用 swr_convert
    int ret = swr_convert(swrCtx_,
        resampleFrame_->data, resampleFrame_->nb_samples,
        inData, resampleFrame_->nb_samples);
    if (ret < 0) 
    {
        avCheckRet("swr_convert", ret);
        qWarning() << "Audio Encoder: Error during resampling.";
        return QVector<AVPacket*>{};
    }

    // 2. 设置pts
    resampleFrame_->pts = ptsCnt_;
    ptsCnt_ += resampleFrame_->nb_samples;

    // --- 3. 调用核心编码函数 ---
    return doEncode(resampleFrame_);
}

QVector<AVPacket*> CAudioEncoder::flush()
{
    qInfo() << "Flushing Audio Encoder...";
    return doEncode(nullptr);
}

void CAudioEncoder::setStream(AVStream* stream)
{
    stream_ = stream;
}

int CAudioEncoder::getFrameSize() const
{
    return codecCtx_ ? codecCtx_->frame_size : 0;
}

int CAudioEncoder::getBytesPerFrame() const
{
    if (!codecCtx_) return 0;
    // 计算 S16 Packed 格式每帧所需的字节数
    return codecCtx_->frame_size * codecCtx_->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
}

QVector<AVPacket*> CAudioEncoder::doEncode(AVFrame* frame)
{
    QVector<AVPacket*> packetList;

    int ret = avcodec_send_frame(codecCtx_, frame);
    if (ret < 0) {
        qWarning() << "Audio Encoder: Error sending frame to encoder.";
        return packetList;
    }

    while (ret >= 0) 
    {
        AVPacket* pkt = av_packet_alloc();
        if (!pkt) 
        {
            qCritical() << "Audio Encoder: Could not allocate AVPacket.";
            break;
        }

        ret = avcodec_receive_packet(codecCtx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
        {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            break;
        }
        else if (ret < 0) 
        {
            qWarning() << "Audio Encoder: Error receiving packet from encoder.";
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            break;
        }

        if (stream_) 
        {
            av_packet_rescale_ts(pkt, codecCtx_->time_base, stream_->time_base);
            pkt->stream_index = stream_->index;
        }

        packetList.append(pkt);
    }
    return packetList;
}

void CAudioEncoder::cleanup()
{
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    if (pcmFrame_) { // 虽然这个版本没用，但保留以防未来需要
        av_frame_free(&pcmFrame_);
        pcmFrame_ = nullptr;
    }
    if (resampleFrame_) {
        av_frame_free(&resampleFrame_);
        resampleFrame_ = nullptr;
    }
    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }
    stream_ = nullptr;
}