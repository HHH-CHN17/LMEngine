#include "Muxer.h"
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

static void avCheckRet(const char* operate, int ret)
{
    char err_buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(ret, err_buf, AV_ERROR_MAX_STRING_SIZE);
    qCritical() << operate << " failed: " << err_buf << " (error code: " << ret << ")";
}

CMuxer::CMuxer()
{
    
}

CMuxer::~CMuxer()
{
    // 确保在析构时资源被释放
    close();
}

bool CMuxer::initialize(const char* filePath)
{
    if (!filePath) {
        qWarning() << "Muxer Error: Filename is null.";
        return false;
    }

    // 分配输出上下文
    int ret = avformat_alloc_output_context2(&formatCtx_, nullptr, nullptr, filePath);
    if (ret < 0 || !formatCtx_) {
        avCheckRet("avformat_alloc_output_context2", ret);
        qWarning() << "Muxer Error: Could not allocate output context.";
        return false;
    }
    filePath_ = filePath;

    // 打开文件 IO
    ret = avio_open(&formatCtx_->pb, filePath, AVIO_FLAG_WRITE);
    if (ret < 0) {
        avCheckRet("avio_open", ret);
        qWarning() << "Muxer Error: Could not open output file" << QString::fromStdString(filePath_);
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
        return false;
    }

    isHeadWritten_ = false;
    qInfo() << "Muxer initialized for file:" << QString::fromStdString(filePath_);
    return true;
}

AVStream* CMuxer::addStream(const AVCodecContext* codecContext)
{
    if (!formatCtx_) {
        qWarning() << "Muxer Error: Must initialize Muxer before adding a stream.";
        return nullptr;
    }
    if (!codecContext) {
        qWarning() << "Muxer Error: Provided codec context is null.";
        return nullptr;
    }

    // 创建一个新的流
    AVStream* stream = avformat_new_stream(formatCtx_, nullptr);
    if (!stream) {
        qWarning() << "Muxer Error: Failed to create new stream.";
        return nullptr;
    }

    // 从编码器上下文中拷贝参数到流的 codecpar
    int ret = avcodec_parameters_from_context(stream->codecpar, codecContext);
    if (ret < 0) {
        avCheckRet("avcodec_parameters_from_context", ret);
        qWarning() << "Muxer Error: Failed to copy codec parameters to stream.";
        // 这里不清理stream，返回上层直接清理muxer
        return nullptr;
    }

    // 对于 H.264/AAC，可能需要这个 flag
    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        // 注意：这里我们修改的是编码器上下文的 flag，因为 addStream 是在 avcodec_open2 之前调用的
        // 更好的做法是在 Encoder 内部设置好这个 flag
        // (AVCodecContext*)codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    stream->codecpar->codec_tag = 0;

    qInfo() << "Muxer: Added new stream #" << stream->index
        << " (type:" << av_get_media_type_string(codecContext->codec_type) << ")";

    return stream;
}

bool CMuxer::writeHeader()
{
    if (!formatCtx_) {
        qWarning() << "Muxer Error: Not initialized.";
        return false;
    }
    if (isHeadWritten_) {
        qWarning() << "Muxer Warning: Header already written.";
        return true;
    }

    // 打印容器信息，用于调试
    av_dump_format(formatCtx_, 0, filePath_.c_str(), 1);

    int ret = avformat_write_header(formatCtx_, nullptr);
    if (ret < 0) 
    {
        avCheckRet("avformat_write_header", ret);
        qWarning() << "Muxer Error: Failed to write header.";
        return false;
    }

    isHeadWritten_ = true;
    qInfo() << "Muxer: Header written successfully.";
    return true;
}

bool CMuxer::writePacket(AVPacket* packet)
{
    if (!isHeadWritten_) 
    {
        qWarning() << "Muxer Error: Cannot write packet before writing header.";
        return false;
    }
    if (!packet) 
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock{ mtx_ };
        int ret = av_interleaved_write_frame(formatCtx_, packet);
        if (ret < 0) 
        {
            avCheckRet("av_interleaved_write_frame", ret);
            qWarning() << "Muxer Error: Failed to write packet to file.";
            return false;
        }
    }
    return true;
}

void CMuxer::close()
{
    if (!formatCtx_) {
        return;
    }

    // 写入文件尾
    if (isHeadWritten_) {
        av_write_trailer(formatCtx_);
    }

    // 关闭文件 IO
    if (formatCtx_->pb) {
        avio_closep(&formatCtx_->pb);
    }

    // 释放上下文
    avformat_free_context(formatCtx_);
    formatCtx_ = nullptr;
    isHeadWritten_ = false;
    qInfo() << "Muxer closed successfully.";
}
