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
    // ------------------------- ��ʼ������ -------------------------
    // ��ʼ¼��ʱ�ĳ�ʼ������
    void setInputWH(int w, int h);
    void setOutputWH(int w, int h);
    bool initMuxer(const char* file);

    // ------------------------- ¼�ƣ�ÿ����Ⱦѭ��������õĺ�������������ÿһ֡�����ݣ� -------------------------
    bool recording(const unsigned char* rgb);
	// ��������Ƶ֡
    bool encodeVideo(AVFrame* pFrame);
    bool encodeAudio(AVFrame* pFrame);

    // ------------------------- ����¼�� -------------------------
    void stopRecord();

private:
    // ------------------------- ��ʼ����Ƶ�� -------------------------
    bool addVideoStream();
    // 1. ��ʼ����Ƶ�����������ģ���Ҫ��ʼ����Ƶ������س�Ա
    bool initVideoCodecCtx();
	// 2. ��ʼ����Ƶ����������Щ����������AVStream��AVCodecContext
    void initVideoCodecParams();

    // ------------------------- ��ʼ����Ƶ�� -------------------------
    bool addAudioStream();
    // ��ʼ����Ƶ�����������ģ���Ҫ��ʼ����Ƶ������س�Ա
    bool initAudioCodectx();
    // д������Ƶ֡
    bool writeFrame(AVPacket* packet);

    // ------------------------- ����¼�� -------------------------
    bool endWriteMp4File();
    void freeAll();


    void avCheckRet(const char* operate, int ret);
    long long getTickCount();

private:
    // Muxer: ����Ƶ��װ
    AVFormatContext* avFormatCtx_ = nullptr;
    AVStream* videoStream_ = nullptr;
    AVStream* audioStream_ = nullptr;

    // ��Ƶ��ʽת��
    SwsContext* videoSwCtx_ = nullptr;

    // ��Ƶ�������
    AVCodecContext* videoCodecCtx_ = nullptr;
    AVFrame* yuvFrame_ = nullptr;   // RGB-->YUV
    AVPacket* videoPkt_ = nullptr;  // YUV-->H.264

	// ��Ƶ�������
    AVCodecContext* audioCodecCtx_ = nullptr;
    AVFrame* audioFrame_ = nullptr; // PCM-->AAC
    AVPacket* audioPkt_ = nullptr;

    // ��Ƶ����
    mutable std::mutex videoWriterMtx_;
    int videoInWidth_ = 0;
    int videoInHeight_ = 0;
    int videoOutWidth_ = 0;
    int videoOutHeight_ = 0;

	// ��Ƶ����
    int m_audioInSamplerate = 44100;
	int m_audioInChannels = 1;
    int m_audioOutSamplerate = 44100;
	int m_audioOutChannels = 1;
    int m_audioOutBitrate = 64000;

    // �������
    std::string     filePath_{};
    bool            isRecording_ = false;
    long long       startTimeStamp_ = 0;

    long long       lastPts_ = 0;


};