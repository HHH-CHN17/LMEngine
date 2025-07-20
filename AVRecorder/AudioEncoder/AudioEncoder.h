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

class CAVRecorder
{
public:
    CAVRecorder();
    virtual ~CAVRecorder();
    static CAVRecorder* GetInstance();

public:
    // ------------------------- 初始化容器 -------------------------
    // 开始录制时的初始化函数
    void setInputWH(int w, int h);
    void setOutputWH(int w, int h);
    bool initMuxer(const char* file);

    // ------------------------- 录制（每个渲染循环中需调用的函数，用于设置每一帧的数据） -------------------------
    bool recording(const unsigned char* rgb);
	// 编码音视频帧
    bool encodeVideo(AVFrame* pFrame);
    bool encodeAudio(AVFrame* pFrame);

    // ------------------------- 结束录制 -------------------------
    void stopRecord();

private:
    // ------------------------- 初始化视频流 -------------------------
    bool addVideoStream();
    // 1. 初始化视频编码器上下文，需要初始化视频编码相关成员
    bool initVideoCodecCtx();
	// 2. 初始化视频参数，该这些参数会用于AVStream和AVCodecContext
    void initVideoCodecParams();

    // ------------------------- 初始化音频流 -------------------------
    bool addAudioStream();
    // 初始化音频编码器上下文，需要初始化音频编码相关成员
    bool initAudioCodectx();
    // 写入音视频帧
    bool writeFrame(AVPacket* packet);

    // ------------------------- 结束录制 -------------------------
    bool endWriteMp4File();
    void freeAll();


    void avCheckRet(const char* operate, int ret);
    long long getTickCount();

private:
    // Muxer: 音视频封装
    AVFormatContext* avFormatCtx_ = nullptr;
    AVStream* videoStream_ = nullptr;
    AVStream* audioStream_ = nullptr;

    // 视频格式转换
    SwsContext* videoSwCtx_ = nullptr;

    // 视频编码相关
    AVCodecContext* videoCodecCtx_ = nullptr;
    AVFrame* yuvFrame_ = nullptr;   // RGB-->YUV
    AVPacket* videoPkt_ = nullptr;  // YUV-->H.264

	// 音频编码相关
    AVCodecContext* audioCodecCtx_ = nullptr;
    AVFrame* audioFrame_ = nullptr; // PCM-->AAC
    AVPacket* audioPkt_ = nullptr;

    // 视频参数
    mutable std::mutex videoWriterMtx_;
    int videoInWidth_ = 0;
    int videoInHeight_ = 0;
    int videoOutWidth_ = 0;
    int videoOutHeight_ = 0;

	// 音频参数
    int m_audioInSamplerate = 44100;
	int m_audioInChannels = 1;
    int m_audioOutSamplerate = 44100;
	int m_audioOutChannels = 1;
    int m_audioOutBitrate = 64000;

    // 其余参数
    std::string     filePath_{};
    bool            isRecording_ = false;
    long long       startTimeStamp_ = 0;

    long long       lastPts_ = 0;


};