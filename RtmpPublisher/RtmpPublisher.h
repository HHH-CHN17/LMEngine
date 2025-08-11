#ifndef RTMP_PUBLISHER_H
#define RTMP_PUBLISHER_H
#include <QFile>

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

#ifdef _WIN32
#include <winsock2.h>

// 应用程序在使用任何网络套接字 (socket) 功能之前，必须先调用 WSAStartup 函数来初始化 Winsock 库
class WinsockGuard {
public:
    WinsockGuard() {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            qFatal("WSAStartup failed with error: %d", result);
        }
    }
    ~WinsockGuard() {
        WSACleanup();
    }
};
#endif

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
	 * @brief 解析Annex B，获取视频编码器的sps和pps
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

#ifdef _WIN32
    WinsockGuard winsockGuard_{};
#endif

    // 录制参数
    AVConfig config_{};

    // 用于处理音视频时间戳
    qint64 startTime_ = 0;

    // 状态管理
    bool isPushing_ = false;

    QFile* h264File = nullptr; // 用于调试，保存H264数据
    QFile* aacFile = nullptr; // 用于调试，保存AAC数据
};

#endif // RTMP_PUBLISHER_H