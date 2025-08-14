#pragma once

#include "AudioEncoder/AudioEncoder.h"
#include "Common/LockFreeQueue.h"
#include "Common/SingletonBase.h"
#include "Muxer/Muxer.h"
#include "VideoEncoder/VideoEncoder.h"

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <iostream>
#include <mutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include <QFile>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"
#include "Common/DataDefine.h"


/**
 * @class CAVRecorder
 * @brief 核心音视频录制控制器。
 *
 * 该类管理音视频的采集、编码和混合写入的全过程。
 * 它采用多线程异步架构，将UI线程、编码线程和写入线程解耦，
 * 以确保高性能和UI的流畅响应。
 */
class CAVRecorder : public QObject, public Singleton_Lazy_Base<CAVRecorder>
{
    friend class Singleton_Lazy_Base<CAVRecorder>;
    Q_OBJECT
private:
    CAVRecorder();
    ~CAVRecorder();

public:
    /**
     * @brief 初始化录制器所需的所有组件。
     * @param config 包含音视频编码、格式等所有配置信息的结构体。
     * @return 初始化成功返回true，否则返回false。
     */
    bool initialize(AVConfig& config);

    /**
     * @brief 启动异步录制流程。
     *
     * 该函数会启动所有后台工作线程（编码、混合），并开始采集音频。
     * 此后，系统便准备好接收由 pushRGBA() 推入的视频数据。
     */
    void startRecording();

    /**
     * @brief 停止异步录制流程。
     *
     * 该函数会发送停止信号给所有后台线程，并等待它们优雅地完成所有剩余工作。
     * 待所有线程结束后，它会负责最后的写入文件尾和资源清理工作。
     */
    void stopRecording();

    /**
     * @brief [UI线程调用] 将一帧从OpenGL获取的原始RGBA数据放入待编码队列。
     *        此函数是非阻塞的，会立即返回，以保证UI线程的流畅。
     * @param rgbaData 指向从PBO等来源获取的RGBA像素数据的指针。
     */
    void pushRGBA(const unsigned char* rgbaData);

    bool isRecording() const;

private:
    /**
     * @brief 根据配置为QAudioInput设置音频格式。
     * @param config 包含音频格式信息的配置。
     * @return 配置好的QAudioFormat对象。
     */
    QAudioFormat setAudioFormat(const AudioFormat& config);

    // ------------------------- 异步工作线程函数 -------------------------
    /**
     * @brief 视频编码线程的执行体。
     *
     * 循环地从`rawVideoQueue_`中取出原始视频帧，进行编码，
     * 然后将编码后的AVPacket放入`encodedPktQueue_`。
     */
    void videoEncodingLoop();

    /**
     * @brief 音频编码线程的执行体。
     *
     * 循环地从`audioCapturer_`中拉取PCM数据，进行编码，
     * 然后将编码后的AVPacket放入`encodedPktQueue_`。
     */
    void audioEncodingLoop();

    /**
     * @brief 混合写入线程的执行体。
     *
     * 循环地从`encodedPktQueue_`中取出编码好的音视频包，
     * 并通过CMuxer写入到最终的文件中。
     */
    void muxingLoop();

    /**
     * @brief 启动所有后台工作线程。
     */
    void startThreads();

    /**
     * @brief 停止所有后台工作线程，并等待它们结束。
     */
    void stopThreads();

    /**
     * @brief 辅助函数，将一个AVPacket列表封装成MediaPacket并推入队列。
     * @param packets 从编码器返回的AVPacket裸指针列表。
     * @param type 这些包的媒体类型 (VIDEO, AUDIO, END_OF_STREAM)。
     */
    void sendVecPkt(const QVector<AVPacket*>& packets, const PacketType& type);
private:
    // 清理所有资源
    void cleanup();

    // 核心组件
    /// @brief 媒体混合器，负责将编码后的音视频包写入容器格式（如MP4）。
    std::unique_ptr<CMuxer> muxer_;
    /// @brief 视频编码器，负责将RGBA图像编码为H.264等格式。
    std::unique_ptr<CVideoEncoder> videoEncoder_;
    /// @brief 音频编码器，负责将PCM音频编码为AAC等格式。
    std::unique_ptr<CAudioEncoder> audioEncoder_;
    /// @brief 音频采集器，负责从麦克风捕获原始PCM音频数据。
    std::unique_ptr<CAudioCapturer> audioCapturer_;

	QFile* h264File = nullptr; // 用于调试，保存H264数据
	QFile* aacFile = nullptr; // 用于调试，保存AAC数据

    // 录制参数
    AVConfig config_;

    // 状态管理
    /// @brief 标志位，表示录制流程是否已启动。由UI线程设置，所有线程读取。
    std::atomic<bool> isRecording_ = false;

    // ------------------------- 异步改造核心成员 -------------------------
    // 三个工作线程
    /// @brief 执行视频编码循环的线程对象。
    std::thread videoEncoderThread_;
    /// @brief 执行音频编码循环的线程对象。
    std::thread audioEncoderThread_;
    /// @brief 执行混合写入循环的线程对象。
    std::thread muxerThread_;

    // 两个核心队列
    lock_free_queue<RGBAUPtr, 60> rawVideoQueue_; // 缓冲约2秒的30fps视频帧
    lock_free_queue<MediaPacket, 300> encodedPktQueue_; // 缓冲编码后的音视频包

    // 线程运行控制标志
    /// @brief 全局运行标志。当设置为false时，关闭音视频编码线程。
    std::atomic<bool> isRunning_{ false };
};