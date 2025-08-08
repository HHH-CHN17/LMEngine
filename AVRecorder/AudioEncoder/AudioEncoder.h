#pragma once

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/avutil.h>
#include <libavutil/common.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}
#include <iostream>
#include <mutex>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"

class CAudioEncoder
{
public:
    CAudioEncoder();
    ~CAudioEncoder();

    // ��ֹ�����͸�ֵ
    CAudioEncoder(const CAudioEncoder&) = delete;
    CAudioEncoder& operator=(const CAudioEncoder&) = delete;

    /**
     * @brief ��ʼ����Ƶ��������
     * @param dstFmt
     * @param srcFmt
     * @return �ɹ����� true��
     */
    bool initialize(const AudioCodecCfg& dstFmt, const AudioFormat& srcFmt);

    /**
     * @brief ����һ֡��Ƶ���ݡ�
     * @param pcmData ָ��ԭʼ S16 (�����ʽ) PCM ���ݵ�ָ�롣
     * @return ����һ�����������������õ� AVPacket ���б�
     *         ��������ʹ���� packet ����븺����� av_packet_free() ���ͷ����ǡ�
     */
    QVector<AVPacket*> encode(const unsigned char* pcmData);

    /**
     * @brief ��ձ����������л����֡��
     * @return ����һ����������ʣ�� AVPacket ���б�
     */
    QVector<AVPacket*> flush();

    /**
     * @brief ���ô˱����������� AVStream��
     * @param stream Muxer ��������Ƶ����
     */
    void setStream(AVStream* stream);

    /**
     * @brief ���ú���pkt��Ҫת����ʱ��������û����setStream������Ҫ���øú�����
     * @param timeBase �ֶ����õ�ʱ�����
     */
    void setTimeBase(AVRational timeBase);

    // �ṩ�Ա����������ĵ�ֻ������
    const AVCodecContext* getCodecContext() const { return codecCtx_; }

    // ��ȡ������������ÿ֡������
    int getFrameSize() const;

    // ��ȡ������������ÿ֡�ֽ��� (���� S16_PACKED ��ʽ)
    int getBytesPerFrame() const;

private:
    // �ڲ����ı��뺯��
    QVector<AVPacket*> doEncode(AVFrame* frame);

    // ����������Դ
    void cleanup();

private:
    AVCodecContext* codecCtx_ = nullptr;
    AVFrame* pcmFrame_ = nullptr;   // ���ڴ�Ŵ������ S16 Packed PCM ����
    AVFrame* resampleFrame_ = nullptr; // ���ڴ���ز������ FLTP Planar ���� (�����Ҫ)
    SwrContext* swrCtx_ = nullptr;    // ���� PCM ��ʽ�Ͳ����ʵ�ת��

    AVStream* stream_ = nullptr;    // Muxer ��������Ƶ��
    AVRational timeBase_{};         // ���û��avstream������Ҫ����timeBase

    // ���ڼ���PTS
    int64_t ptsCnt_ = 0;
};