#ifndef CAUDIOCAPTURER_H
#define CAUDIOCAPTURER_H

#include <QObject>
#include <QAudioInput>
#include <QAudioFormat>
#include <QByteArray>
#include <QMutex>
#include <QBuffer>
#include <QScopedPointer>
#include "AVRecorder/AudioCapturer/IOBuffer/IOBuffer.h"
#include "Common/DataDefine.h"

class CAudioCapturer : public QObject
{
    Q_OBJECT

public:
    explicit CAudioCapturer(QObject* parent = nullptr);
    ~CAudioCapturer();

    // ��ʼ����Ƶ�����豸��ͬʱ������¼���豸֧�ֵ���Ƶ��ʽ����audioFmt��
    bool initialize(const QAudioFormat& format, AudioFormat& audioFmt);

    // ��ʼ����
    void start();

    // ֹͣ����
    void stop();

    // ��ȡchunksize����Ƶ���ݣ��̰߳�ȫ
    QByteArray readChunk(qint64 chunkSize);

    // ��ȡ����ȷ������Ƶ��ʽ
    QAudioFormat getAudioFormat() const;

private:
    QAudioDeviceInfo getDeviceInfo(const char* deviceName);

private slots:
    // QAudioInput ��״̬�仯ʱ����
    void slot_StateChanged(QAudio::State newState);

private:
    QAudioFormat format_;
    QScopedPointer<QAudioInput> audioInput_;
    CIOBuffer* audioIOBuffer_ = nullptr;       // ������� QAudioInput::start() ���ص��豸

    // �̰߳�ȫ����Ƶ���ݻ�����
    //mutable QMutex mtx_;
    //QByteArray buffer_;

    bool isInitialized_ = false;
};

#endif // CAUDIOCAPTURER_H