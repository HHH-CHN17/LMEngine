#pragma once

#include "AudioEncoder/AudioEncoder.h"
#include "Common/LockFreeQueue.h"
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

class CAVRecorder : public QObject
{
    Q_OBJECT
public:
    explicit CAVRecorder(QObject* parent = nullptr);
    ~CAVRecorder();
    static CAVRecorder* GetInstance();

public:
    bool initialize(AVConfig& config);

    void startRecording();

    void stopRecording();

    /**
     * @brief [UI线程调用] 处理一帧视频和所有可用的音频。
     *        这个函数是阻塞的，会同步执行 视频编码、音频编码 和 文件写入
     * @param rgbData OpenGL捕获的rgb数据（方向与FFmpeg相反）。
     * @return 编码写入成功返回true，否则返回false
     */
    Q_DECL_UNUSED bool recording(const unsigned char* rgbData);

    /**
     * @brief [UI线程调用] 将一帧从OpenGL获取的原始RGBA数据放入待编码队列。
     *        此函数是非阻塞的，会立即返回。
     * @param rgbaData 指向RGBA数据的指针。
     */
    void enqueueVideoFrame(const unsigned char* rgbaData);

    bool isRecording() const;

private:
    QAudioFormat setAudioFormat(const AudioFormat& config);
    // ------------------------- 异步改造 -------------------------
    void videoEncodingLoop();
    void audioEncodingLoop();
    void muxingLoop();

    void startThreads();
    void stopThreads();
private:
    // 清理所有资源
    void cleanup();

    // 核心组件
    QScopedPointer<CMuxer> muxer_;
    QScopedPointer<CVideoEncoder> videoEncoder_;
    QScopedPointer<CAudioEncoder> audioEncoder_;
    QScopedPointer<CAudioCapturer> audioCapturer_;

	QFile* h264File = nullptr; // 用于调试，保存H264数据
	QFile* aacFile = nullptr; // 用于调试，保存AAC数据

    // 录制参数
    AVConfig config_;

    // 状态管理
    std::atomic<bool> isRecording_ = false;

    // ------------------------- 异步改造 -------------------------
    // 三个工作线程
    std::thread videoEncoderThread_;
    std::thread audioEncoderThread_;
    std::thread muxerThread_;

    // 两个核心队列
    lock_free_queue<RawVideoFrame, 60> rawVideoQueue_; // 缓冲约2秒的30fps视频帧
    lock_free_queue<MediaPacket, 300> encodedPacketQueue_; // 缓冲编码后的音视频包

    // 线程运行控制标志
    std::atomic<bool> isRunning_{ false };
};