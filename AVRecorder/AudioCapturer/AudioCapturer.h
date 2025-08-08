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

    // 初始化音频捕获设备，同时将最终录音设备支持的音频格式存于audioFmt中
    bool initialize(const QAudioFormat& format, AudioFormat& audioFmt);

    // 开始捕获
    void start();

    // 停止捕获
    void stop();

    // 读取chunksize个音频数据，线程安全
    QByteArray readChunk(qint64 chunkSize);

    // 获取最终确定的音频格式
    QAudioFormat getAudioFormat() const;

private:
    QAudioDeviceInfo getDeviceInfo(const char* deviceName);

private slots:
    // QAudioInput 的状态变化时调用
    void slot_StateChanged(QAudio::State newState);

private:
    QAudioFormat format_;
    QScopedPointer<QAudioInput> audioInput_;
    CIOBuffer* audioIOBuffer_ = nullptr;       // 这个将是 QAudioInput::start() 返回的设备

    // 线程安全的音频数据缓冲区
    //mutable QMutex mtx_;
    //QByteArray buffer_;

    bool isInitialized_ = false;
};

#endif // CAUDIOCAPTURER_H