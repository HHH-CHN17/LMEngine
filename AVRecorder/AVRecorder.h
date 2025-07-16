#pragma once

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <iostream>
#include <mutex>
#include "faac.h"

typedef struct MP4_AAC_CONFIGURE
{
    faacEncHandle hEncoder;        //音频文件描述符
    unsigned int nSampleRate;     //音频采样数
    unsigned int nChannels;  	      //音频声道数
    unsigned int nPCMBitSize;        //音频采样精度
    unsigned int nInputSamples;      //每次调用编码时所应接收的原始数据长度
    unsigned int nMaxOutputBytes;    //每次调用编码时生成的AAC数据的最大长度
    unsigned char* pcmBuffer;       //pcm数据
    unsigned char* aacBuffer;       //aac数据

}AACEncodeConfig;

class CAVRecorder
{
public:
    CAVRecorder();
    virtual ~CAVRecorder();
    static CAVRecorder* GetInstance();


private:

    AVFormatContext* avFormatCtx;

    AVCodecContext* videoCodecCtx;
    AVCodecContext* audioCodecCtx;

    AVStream* videoStream;
    AVStream* audioStream;

    SwsContext* videoSwCtx_;

    AVFrame* yuvFrame; //RGB-->YUV-->H.264

    std::string             m_filePath;

    bool                    m_bRecording;
    unsigned long           startTimeStamp;

    mutable std::mutex              videoWriterMtx_;

    int videoInWidth_; int videoInHeight_;
    int videoOutWidth_; int videoOutHeight_;
    int m_videoOutBitrate;

    int m_audioInSamplerate; int m_audioInChannels;
    int m_audioOutSamplerate; int m_audioOutChannels;
    int m_audioOutBitrate;

    long long           m_lastPts;

    int                 m_audioFramePts = 0;
    int                 m_samples = 960;

    int                 m_pcmBufferSize = 0;
    int                 m_pcmBufferRemainSize = 0;
    int                 m_pcmWriteRemainSize = 0;
    AACEncodeConfig*    g_aacEncodeConfig;


public:
    // 开始录制时的初始化函数
    void setOutputWH(int w, int h);
    bool initOutputFile(const char* file);

    // 每个渲染循环中需调用的函数，用于设置每一帧的数据
    void setInputWH(int w, int h);
    bool recording(const unsigned char* rgb);

    // 结束录制
    void stopRecord();
    
    bool WriteAudioFrameWithPCMData(unsigned char* data, int captureSize);

private:
    bool addVideoStream();
    bool addAudioStream();

    bool writeFrame(AVPacket* packet);

    bool endWriteMp4File();

    void freeAll();
    unsigned long  getTickCount();

    AACEncodeConfig* initAudioEncodeConfiguration();
    void ReleaseAccConfiguration();

    int linearPCM2AAC(unsigned char* pData, int captureSize);

};