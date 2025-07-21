#pragma once

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <QVector>
#include <mutex>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"
#include "Common/DataDefine.h"

class CVideoEncoder
{
public:
    CVideoEncoder();
    ~CVideoEncoder();

    // ��ֹ�����͸�ֵ
    CVideoEncoder(const CVideoEncoder&) = delete;
    CVideoEncoder& operator=(const CVideoEncoder&) = delete;

public:
    // ��ʼ����Ƶ������
    bool initialize(const VideoConfig& cfg);

    /**
     * @brief ����һ֡��Ƶ���ݡ�
     * @param rgbData ָ��ԭʼRGBA���ݵ�ָ�루��С������ inWidth * inHeight * 4����
     * @param dataSize ���ݵĴ�С����ѡ��������֤����
     * @return ����һ�����������������õ� AVPacket ���б�
     *         ��������ʹ���� packet ����븺����� av_packet_free() ���ͷ����ǡ�
     */
    QVector<AVPacket*> encode(const unsigned char* rgbData, int dataSize);

    // ��ձ����������л����packet
    QVector<AVPacket*> flush();

    /**
     * @brief ���ô˱����������� AVStream��
     *        Muxer ���� stream �󣬱�����ô˺����� stream ��Ϣ���ݽ�����
     * @param stream Muxer ��������Ƶ����
     */
    void setStream(AVStream* stream);

    // �ṩ�Ա����������ĵ�ֻ�����ʣ��Ա� Muxer ���Դ��л�ȡ����
    const AVCodecContext* getCodecContext() const { return codecCtx_; }

private:
    // �ڲ����ı��뺯��
    QVector<AVPacket*> doEncode(AVFrame* frame);

    // ����������Դ
    void cleanup();

private:
    AVCodecContext* codecCtx_ = nullptr;
    AVFrame* yuvFrame_ = nullptr;   // ���ڴ��ת����� YUV ����
    SwsContext* swsCtx_ = nullptr;    // ���� RGB -> YUV ��ת��

    AVStream* stream_ = nullptr; // Muxer ��������Ƶ��

    // �������
    int inWidth_ = 0;
    int inHeight_ = 0;
    int outWidth_ = 0;
    int outHeight_ = 0;

    // ���ڼ���PTS
    int64_t ptsCnt_ = 0;
};