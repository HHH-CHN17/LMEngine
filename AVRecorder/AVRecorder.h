#pragma once

#include "AudioEncoder/AudioEncoder.h"
#include "Common/LockFreeQueue.h"
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
#include <QFile>
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
    bool initialize(AVConfig& config);

    void startRecording();

    void stopRecording();

    /**
     * @brief [UI�̵߳���] ����һ֡��Ƶ�����п��õ���Ƶ��
     *        ��������������ģ���ͬ��ִ�� ��Ƶ���롢��Ƶ���� �� �ļ�д��
     * @param rgbData OpenGL�����rgb���ݣ�������FFmpeg�෴����
     * @return ����д��ɹ�����true�����򷵻�false
     */
    Q_DECL_UNUSED bool recording(const unsigned char* rgbData);

    /**
     * @brief [UI�̵߳���] ��һ֡��OpenGL��ȡ��ԭʼRGBA���ݷ����������С�
     *        �˺����Ƿ������ģ����������ء�
     * @param rgbaData ָ��RGBA���ݵ�ָ�롣
     */
    void enqueueVideoFrame(const unsigned char* rgbaData);

    bool isRecording() const;

private:
    QAudioFormat setAudioFormat(const AudioFormat& config);
    // ------------------------- �첽���� -------------------------
    void videoEncodingLoop();
    void audioEncodingLoop();
    void muxingLoop();

    void startThreads();
    void stopThreads();
private:
    // ����������Դ
    void cleanup();

    // �������
    QScopedPointer<CMuxer> muxer_;
    QScopedPointer<CVideoEncoder> videoEncoder_;
    QScopedPointer<CAudioEncoder> audioEncoder_;
    QScopedPointer<CAudioCapturer> audioCapturer_;

	QFile* h264File = nullptr; // ���ڵ��ԣ�����H264����
	QFile* aacFile = nullptr; // ���ڵ��ԣ�����AAC����

    // ¼�Ʋ���
    AVConfig config_;

    // ״̬����
    std::atomic<bool> isRecording_ = false;

    // ------------------------- �첽���� -------------------------
    // ���������߳�
    std::thread videoEncoderThread_;
    std::thread audioEncoderThread_;
    std::thread muxerThread_;

    // �������Ķ���
    lock_free_queue<RawVideoFrame, 60> rawVideoQueue_; // ����Լ2���30fps��Ƶ֡
    lock_free_queue<MediaPacket, 300> encodedPacketQueue_; // �������������Ƶ��

    // �߳����п��Ʊ�־
    std::atomic<bool> isRunning_{ false };
};