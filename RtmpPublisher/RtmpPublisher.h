#ifndef RTMP_PUBLISHER_H
#define RTMP_PUBLISHER_H

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
#include "AVRecorder/AudioEncoder/AudioEncoder.h"
#include "AVRecorder/VideoEncoder/VideoEncoder.h"
#include "RtmpPush/RtmpPush.h"
#include "Common/DataDefine.h"

class CRtmpPublisher : public QObject
{
    Q_OBJECT
public:
    explicit CRtmpPublisher(QObject* parent = nullptr);
    ~CRtmpPublisher();
    static CRtmpPublisher* GetInstance();

public:
    bool initialize(AVConfig& config);

    void startPush();

    /**
     * @brief 处理一帧视频和所有可用的音频。
     *        这个函数将同步执行 视频编码、音频编码 和 文件写入
     * @param rgbData OpenGL捕获的rgb数据（方向与FFmpeg相反）。
     * @return 编码写入成功返回true，否则返回false
     */
    bool pushing(const unsigned char* rgbData);

    void stopPush();

    bool isRecording() const;

    /**
	 * @brief 获取视频编码器的sps和pps
	 *          在设置AV_CODEC_FLAG_GLOBAL_HEADER之后，该参数存储于codecCtx->extradata中
     * @param sps 序列参数集
     * @param pps 图像参数集
	 * @return 配置成功返回true，否则返回false
     */
    bool getH264Config(std::vector<uint8_t>& sps, std::vector<uint8_t>& pps);

    /**
     * @brief 
     * @param asc 
     * @return 
     */
    bool getAacConfig(std::vector<uint8_t>& asc);

private:
    QAudioFormat initAudioFormat(const AudioFormat& fmt);

private:
    // 清理所有资源
    void cleanup();

    // 核心组件
    QScopedPointer<CRtmpPush> rtmpPush_;
    QScopedPointer<CVideoEncoder> videoEncoder_;
    QScopedPointer<CAudioEncoder> audioEncoder_;
    QScopedPointer<CAudioCapturer> audioCapturer_;

    // 录制参数
    AVConfig config_{};

    // 状态管理
    bool isRecording_ = false;
};

#endif // RTMP_PUBLISHER_H