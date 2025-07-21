#pragma once

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <QVector>
#include <mutex>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"
#include "Common/DataDefine.h"

class CVideoEncoder
{
public:
    CVideoEncoder();
    ~CVideoEncoder();

    // 禁止拷贝和赋值
    CVideoEncoder(const CVideoEncoder&) = delete;
    CVideoEncoder& operator=(const CVideoEncoder&) = delete;

public:
    // 初始化视频编码器
    bool initialize(const VideoConfig& cfg);

    /**
     * @brief 编码一帧视频数据。
     * @param rgbData 指向原始RGBA数据的指针（大小必须是 inWidth * inHeight * 4）。
     * @param dataSize 数据的大小（可选，用于验证）。
     * @return 返回一个包含零个或多个编码好的 AVPacket 的列表。
     *         调用者在使用完 packet 后必须负责调用 av_packet_free() 来释放它们。
     */
    QVector<AVPacket*> encode(const unsigned char* rgbData, int dataSize);

    // 清空编码器中所有缓存的packet
    QVector<AVPacket*> flush();

    /**
     * @brief 设置此编码器关联的 AVStream。
     *        Muxer 创建 stream 后，必须调用此函数将 stream 信息传递进来。
     * @param stream Muxer 创建的视频流。
     */
    void setStream(AVStream* stream);

    // 提供对编码器上下文的只读访问，以便 Muxer 可以从中获取参数
    const AVCodecContext* getCodecContext() const { return codecCtx_; }

private:
    // 内部核心编码函数
    QVector<AVPacket*> doEncode(AVFrame* frame);

    // 清理所有资源
    void cleanup();

private:
    AVCodecContext* codecCtx_ = nullptr;
    AVFrame* yuvFrame_ = nullptr;   // 用于存放转换后的 YUV 数据
    SwsContext* swsCtx_ = nullptr;    // 用于 RGB -> YUV 的转换

    AVStream* stream_ = nullptr; // Muxer 创建的视频流

    // 编码参数
    int inWidth_ = 0;
    int inHeight_ = 0;
    int outWidth_ = 0;
    int outHeight_ = 0;

    // 用于计算PTS
    int64_t ptsCnt_ = 0;
};