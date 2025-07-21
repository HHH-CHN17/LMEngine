#ifndef CAUDIOCAPTURER_H
#define CAUDIOCAPTURER_H

#include <QObject>
#include <QAudioInput>
#include <QAudioFormat>
#include <QByteArray>
#include <QMutex>
#include <QIODevice>
#include <QScopedPointer>

class CAudioCapturer : public QObject
{
    Q_OBJECT

public:
    explicit CAudioCapturer(QObject* parent = nullptr);
    ~CAudioCapturer();

    // 初始化音频捕获设备，返回是否成功
    bool initialize(const QAudioFormat& format);

    // 开始捕获
    void start();

    // 停止捕获
    void stop();

    // 获取音频缓冲区的引用，以便消费者可以访问
    QByteArray& getBuffer();
    QMutex& getMutex();

    // 获取最终确定的音频格式
    QAudioFormat getAudioFormat() const;

private slots:
    // QAudioInput 的状态变化时调用
    void slot_StateChanged(QAudio::State newState);

    void slot_ReadPCM();

private:
    QAudioFormat format_;
    QScopedPointer<QAudioInput> audioInput_;
    QIODevice* audioDevice_ = nullptr;       // 这个将是 QAudioInput::start() 返回的设备

    // 线程安全的音频数据缓冲区
    mutable QMutex mtx_;
    QByteArray buffer_;

    bool isInitialized_ = false;
};

#endif // CAUDIOCAPTURER_H