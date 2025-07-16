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
    faacEncHandle hEncoder;        //��Ƶ�ļ�������
    unsigned int nSampleRate;     //��Ƶ������
    unsigned int nChannels;  	      //��Ƶ������
    unsigned int nPCMBitSize;        //��Ƶ��������
    unsigned int nInputSamples;      //ÿ�ε��ñ���ʱ��Ӧ���յ�ԭʼ���ݳ���
    unsigned int nMaxOutputBytes;    //ÿ�ε��ñ���ʱ���ɵ�AAC���ݵ���󳤶�
    unsigned char* pcmBuffer;       //pcm����
    unsigned char* aacBuffer;       //aac����

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
    // ��ʼ¼��ʱ�ĳ�ʼ������
    void setOutputWH(int w, int h);
    bool initOutputFile(const char* file);

    // ÿ����Ⱦѭ��������õĺ�������������ÿһ֡������
    void setInputWH(int w, int h);
    bool recording(const unsigned char* rgb);

    // ����¼��
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