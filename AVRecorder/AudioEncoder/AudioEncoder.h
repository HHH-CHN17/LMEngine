#pragma once

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <iostream>
#include <mutex>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"

class CAudioEncoder
{
public:
    CAudioEncoder();
    ~CAudioEncoder();

    // 禁止拷贝和赋值
    CAudioEncoder(const CAudioEncoder&) = delete;
    CAudioEncoder& operator=(const CAudioEncoder&) = delete;

    /**
     * @brief 初始化音频编码器。
     * @param sampleRate 采样率 (e.g., 48000)。
     * @param channels 声道数 (e.g., 2 for stereo)。
     * @param bitrate 音频比特率 (e.g., 128000)。
     * @return 成功返回 true。
     */
    bool initialize(int sampleRate, int channels, long long bitrate);

    /**
     * @brief 编码一帧音频数据。
     * @param pcmData 指向原始 S16 (交错格式) PCM 数据的指针。
     * @param dataSize 数据的大小（字节）。
     * @return 返回一个包含零个或多个编码好的 AVPacket 的列表。
     *         调用者在使用完 packet 后必须负责调用 av_packet_free() 来释放它们。
     */
    QVector<AVPacket*> encode(const unsigned char* pcmData, int dataSize);

    /**
     * @brief 清空编码器中所有缓存的帧。
     * @return 返回一个包含所有剩余 AVPacket 的列表。
     */
    QVector<AVPacket*> flush();

    /**
     * @brief 设置此编码器关联的 AVStream。
     * @param stream Muxer 创建的音频流。
     */
    void setStream(AVStream* stream);

    // 提供对编码器上下文的只读访问
    const AVCodecContext* getCodecContext() const { return codecCtx_; }

    // 获取编码器期望的每帧样本数
    int getFrameSize() const;

    // 获取编码器期望的每帧字节数 (对于 S16_PACKED 格式)
    int getBytesPerFrame() const;

private:
    // 内部核心编码函数
    QVector<AVPacket*> doEncode(AVFrame* frame);

    // 清理所有资源
    void cleanup();

private:
    AVCodecContext* codecCtx_ = nullptr;
    AVFrame* pcmFrame_ = nullptr;   // 用于存放待编码的 S16 Packed PCM 数据
    AVFrame* resampleFrame_ = nullptr; // 用于存放重采样后的 FLTP Planar 数据 (如果需要)
    SwrContext* swrCtx_ = nullptr;    // 用于 PCM 格式和采样率的转换

    AVStream* stream_ = nullptr; // Muxer 创建的音频流

    // 用于计算PTS
    int64_t ptsCnt_ = 0;
};