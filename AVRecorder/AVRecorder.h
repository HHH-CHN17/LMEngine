#pragma once
#include "AudioEncoder/AudioEncoder.h"
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
    bool initialize(const AVConfig& config);

    void startRecording();

    void stopRecording();

    /**
     * @brief 处理一帧视频和所有可用的音频。
     *        这个函数将同步执行 视频编码、音频编码 和 文件写入
     * @param rgbData OpenGL捕获的rgb数据（方向与FFmpeg相反）。
     * @return 编码写入成功返回true，否则返回false
     */
    bool recording(const unsigned char* rgbData);

    bool isRecording() const;

private:
    QAudioFormat initAudioFormat(const AVConfig& config);

private:
    // 清理所有资源
    void cleanup();

    // 核心组件
    QScopedPointer<CMuxer> muxer_;
    QScopedPointer<CVideoEncoder> videoEncoder_;
    QScopedPointer<CAudioEncoder> audioEncoder_;
    QScopedPointer<CAudioCapturer> audioCapturer_;

    // 录制参数
    AVConfig config_;

    // 状态管理
    bool isRecording_ = false;
};