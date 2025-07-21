#include "AudioCapturer.h"
#include <QAudioDeviceInfo> // <--- 用于获取设备信息
#include <QDebug>

CAudioCapturer::CAudioCapturer(QObject* parent)
    : QObject(parent)
{
}

CAudioCapturer::~CAudioCapturer()
{
    stop();
}

bool CAudioCapturer::initialize(const QAudioFormat& format)
{
    if (isInitialized_) {
        qWarning() << "Audio capturer already initialized.";
        return true;
    }

    format_ = format;
    // 获取默认输入设备信息
    const QAudioDeviceInfo defaultDevice = QAudioDeviceInfo::defaultInputDevice();
    if (defaultDevice.isNull()) {
        qCritical() << "No audio input device found.";
        return false;
    }

    // 检查设备是否支持此格式
    if (!defaultDevice.isFormatSupported(format_)) {
        qWarning() << "Requested audio format not supported by default device, trying nearest.";
        format_ = defaultDevice.nearestFormat(format_);
        // 如果最接近的格式仍然无效，则失败
        if (format_.sampleRate() == 0) { // 无效格式的采样率通常为0
            qCritical() << "No valid audio format found for the default device.";
            return false;
        }
    }

    // 创建 QAudioInput 实例
    audioInput_.reset(new QAudioInput(defaultDevice, format_, this));
    if (!audioInput_) {
        qCritical() << "Failed to create QAudioInput.";
        return false;
    }

    // 连接状态变化信号，用于调试
    connect(audioInput_.data(), &QAudioInput::stateChanged, this, &CAudioCapturer::slot_StateChanged);

    isInitialized_ = true;
    qInfo() << "Audio capturer initialized successfully with format:\n"
			<< "Sample Rate:" << format_.sampleRate() << "\n"
			<< "Channels:" << format_.channelCount() << "\n"
			<< "Sample Format:" << format_.sampleType(); // 使用 sampleFormatName() 获取名称

    return true;
}

void CAudioCapturer::start()
{
    if (!isInitialized_) {
        qWarning() << "Cannot start, capturer is not initialized.";
        return;
    }

    // QAudioInput::start() 返回一个 QIODevice，所有捕获的数据都会被写入这个设备
    // 我们需要从这个设备中读取数据
    if (audioInput_ && audioInput_->state() != QAudio::ActiveState) {
        // 清空旧的缓冲区数据
        {
            QMutexLocker locker(&mtx_);
            buffer_.clear();
        }

        audioDevice_ = audioInput_->start(); //  开始接受PCM数据！
        if (audioDevice_) {
            // 连接这个 IO 设备的 readyRead 信号
            // 这个槽函数负责从 audioDevice_ 读取数据并放入 buffer_
            connect(audioDevice_, &QIODevice::readyRead, this, CAudioCapturer::slot_ReadPCM);
            qInfo() << "Audio capture started.";
        }
        else {
            qCritical() << "Failed to start audio capture, QAudioInput::start() returned nullptr.";
        }
    }
}

void CAudioCapturer::slot_ReadPCM()
{
    if (!audioDevice_) return;

    const qint64 bytesAvailable = audioDevice_->bytesAvailable();
    if (bytesAvailable <= 0) return;

    QByteArray newData = audioDevice_->read(bytesAvailable);
    if (newData.isEmpty()) return;

    QMutexLocker locker(&mtx_);
    buffer_.append(newData);
}

void CAudioCapturer::stop()
{
    if (audioInput_ && audioInput_->state() != QAudio::StoppedState) 
    {
        audioInput_->stop();
        disconnect(audioInput_.data(), &QAudioInput::stateChanged, this, &CAudioCapturer::slot_StateChanged);
        disconnect(audioDevice_, &QIODevice::readyRead, this, CAudioCapturer::slot_ReadPCM);
        // audioDevice_ 会在 QAudioInput 停止时自动变为无效，不需要手动断开连接或删除
        audioDevice_ = nullptr;
        qInfo() << "Audio capture stopped.";
    }
}

QByteArray& CAudioCapturer::getBuffer()
{
    return buffer_;
}

QMutex& CAudioCapturer::getMutex()
{
    return mtx_;
}

QAudioFormat CAudioCapturer::getAudioFormat() const
{
    return format_;
}

void CAudioCapturer::slot_StateChanged(QAudio::State newState)
{
    // 这个槽主要用于调试，可以观察音频设备的状态变化
    qDebug() << "QAudioInput state changed to:" << newState;
    if (newState == QAudio::StoppedState) {
        if (audioInput_->error() != QAudio::NoError) {
            qWarning() << "Audio input error occurred:" << audioInput_->error();
        }
    }
}