#pragma once
#include "AudioEncoder/AudioEncoder.h"
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
    bool initialize(const AVConfig& config);

    void startRecording();

    void stopRecording();

    /**
     * @brief ����һ֡��Ƶ�����п��õ���Ƶ��
     *        ���������ͬ��ִ�� ��Ƶ���롢��Ƶ���� �� �ļ�д��
     * @param rgbData OpenGL�����rgb���ݣ�������FFmpeg�෴����
     * @return ����д��ɹ�����true�����򷵻�false
     */
    bool recording(const unsigned char* rgbData);

    bool isRecording() const;

private:
    QAudioFormat initAudioFormat(const AVConfig& config);

private:
    // ����������Դ
    void cleanup();

    // �������
    QScopedPointer<CMuxer> muxer_;
    QScopedPointer<CVideoEncoder> videoEncoder_;
    QScopedPointer<CAudioEncoder> audioEncoder_;
    QScopedPointer<CAudioCapturer> audioCapturer_;

    // ¼�Ʋ���
    AVConfig config_;

    // ״̬����
    bool isRecording_ = false;
};