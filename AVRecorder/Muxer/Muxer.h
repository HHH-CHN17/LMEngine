#pragma once

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <iostream>
#include <mutex>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"
#include "Common/DataDefine.h"

class CMuxer
{
public:
    CMuxer();
    ~CMuxer();

    // 禁止拷贝和赋值
    CMuxer(const CMuxer&) = delete;
    CMuxer& operator=(const CMuxer&) = delete;

public:
    // 初始化 Muxer
    bool initialize(const char* filePath);

    /**
     * @brief 添加一个新的媒体流（视频或音频）。
     *        这个函数应该被 CVideoEncoder 或 CAudioEncoder 在它们初始化后调用。
     * @param codecContext 已经配置好的编码器上下文，Muxer将从中提取流信息。
     * @return 返回创建的 AVStream 指针，如果失败则返回 nullptr。
     *         编码器需要保存这个指针，以便知道自己的 stream_index。
     */
    AVStream* addStream(const AVCodecContext* codecContext);

    /**
     * @brief 写入文件头信息。
     *        必须在所有流都通过 addStream() 添加完毕后调用。
     * @return 成功返回 true，失败返回 false。
     */
    bool writeHeader();

    /**
     * @brief 将一个编码好的数据包写入文件。
     *        这个函数是线程安全的。
     * @param packet 要写入的 AVPacket。
     * @return 成功返回 true，失败返回 false。
     */
    bool writePacket(AVPacket* packet);

    // 关闭 Muxer，写入文件尾并释放所有资源
    void close();

    // 提供对 AVFormatContext 的只读访问，某些高级场景可能需要
    const AVFormatContext* getFormatContext() const { return formatCtx_; }

private:
    AVFormatContext* formatCtx_ = nullptr;
    std::string filePath_;
    bool isHeadWritten_ = false;

    // 使用互斥锁保护对 av_interleaved_write_frame 的调用
    mutable std::mutex mtx_;
};