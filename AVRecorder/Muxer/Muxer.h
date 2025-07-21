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
#include "Common/DataDefine.h"

class CMuxer
{
public:
    CMuxer();
    ~CMuxer();

    // ��ֹ�����͸�ֵ
    CMuxer(const CMuxer&) = delete;
    CMuxer& operator=(const CMuxer&) = delete;

public:
    // ��ʼ�� Muxer
    bool initialize(const char* filePath);

    /**
     * @brief ���һ���µ�ý��������Ƶ����Ƶ����
     *        �������Ӧ�ñ� CVideoEncoder �� CAudioEncoder �����ǳ�ʼ������á�
     * @param codecContext �Ѿ����úõı����������ģ�Muxer��������ȡ����Ϣ��
     * @return ���ش����� AVStream ָ�룬���ʧ���򷵻� nullptr��
     *         ��������Ҫ�������ָ�룬�Ա�֪���Լ��� stream_index��
     */
    AVStream* addStream(const AVCodecContext* codecContext);

    /**
     * @brief д���ļ�ͷ��Ϣ��
     *        ��������������ͨ�� addStream() �����Ϻ���á�
     * @return �ɹ����� true��ʧ�ܷ��� false��
     */
    bool writeHeader();

    /**
     * @brief ��һ������õ����ݰ�д���ļ���
     *        ����������̰߳�ȫ�ġ�
     * @param packet Ҫд��� AVPacket��
     * @return �ɹ����� true��ʧ�ܷ��� false��
     */
    bool writePacket(AVPacket* packet);

    // �ر� Muxer��д���ļ�β���ͷ�������Դ
    void close();

    // �ṩ�� AVFormatContext ��ֻ�����ʣ�ĳЩ�߼�����������Ҫ
    const AVFormatContext* getFormatContext() const { return formatCtx_; }

private:
    AVFormatContext* formatCtx_ = nullptr;
    std::string filePath_;
    bool isHeadWritten_ = false;

    // ʹ�û����������� av_interleaved_write_frame �ĵ���
    mutable std::mutex mtx_;
};