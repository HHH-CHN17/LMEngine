#include "AudioEncoder.h"
#include <chrono>
#include <QObject>
#include <QDebug>
#include <libavutil/log.h>
#include <stdarg.h> 

#include "Common/DataDefine.h"

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

    // 1. ���� AAC ������
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        qCritical() << "Audio Encoder: AAC codec not found.";
        return false;
    }

    // 2. ���������������
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) {
        qCritical() << "Audio Encoder: Could not allocate codec context.";
        return false;
    }

    // 3. ���ñ��������� (Ŀ���ʽ)
    codecCtx_->bit_rate = bitrate;
    codecCtx_->sample_rate = sampleRate;
    codecCtx_->channels = channels;
    codecCtx_->channel_layout = av_get_default_channel_layout(channels);
    // AAC ������ͨ��ʹ�� Planar ��������ʽ (FLTP) �Ի���������
    codecCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
    codecCtx_->time_base = { 1, sampleRate }; // ʱ����Բ�����Ϊ��λ

    // 4. �򿪱�����
    int ret = avcodec_open2(codecCtx_, codec, nullptr);
    if (ret < 0) {
        qCritical() << "Audio Encoder: Could not open codec.";
        cleanup();
        return false;
    }

    // 5. ��ʼ���ز��������� (SwrContext)
    // ��Ϊ���Ǵ� CAudioCapturer ���յ��� S16 Packed ��ʽ������������Ҫ FLTP Planar ��ʽ
    swrCtx_ = swr_alloc_set_opts(nullptr,
        // Ŀ����� (��������Ҫ��)
        codecCtx_->channel_layout, codecCtx_->sample_fmt, codecCtx_->sample_rate,
        // Դ���� (�����ṩ��)
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

    // 6. �������ڴ���ز��������ݵ� AVFrame
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

    // --- 1. �����ز����͸�ʽת�� (S16 Packed -> FLTP Planar) ---
    // swr_convert ��Ҫ const uint8_t** ���͵�����
    const uint8_t** inData = &pcmData;

    // ���� swr_convert
    int ret = swr_convert(swrCtx_,
        resampleFrame_->data, resampleFrame_->nb_samples,
        inData, resampleFrame_->nb_samples);
    if (ret < 0) 
    {
        avCheckRet("swr_convert", ret);
        qWarning() << "Audio Encoder: Error during resampling.";
        return QVector<AVPacket*>{};
    }

    // 2. ����pts
    resampleFrame_->pts = ptsCnt_;
    ptsCnt_ += resampleFrame_->nb_samples;

    // --- 3. ���ú��ı��뺯�� ---
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
    // ���� S16 Packed ��ʽÿ֡������ֽ���
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
    if (pcmFrame_) { // ��Ȼ����汾û�ã��������Է�δ����Ҫ
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