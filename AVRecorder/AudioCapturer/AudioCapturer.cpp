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

QAudioDeviceInfo CAudioCapturer::getDeviceInfo(const char* deviceName)
{
    QString expect = QString::fromUtf8(deviceName);
    QAudioDeviceInfo targetDevice{};

    // 2. 查找所有可用的输入设备
    const QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    bool deviceFound = false;
    for (const QAudioDeviceInfo& deviceInfo : devices) {
        if (deviceInfo.deviceName() == expect) {
            targetDevice = deviceInfo;
            deviceFound = true;
            qInfo() << "Found target audio device:" << expect;
            break;
        }
    }
    if (!deviceFound)
    {
    	qWarning() << "Could not find target device'" << expect << "'. Falling back to default device.";
        targetDevice = QAudioDeviceInfo::defaultInputDevice();
    }

    if (targetDevice.isNull()) {
        qCritical() << "No audio input device found.";
        return QAudioDeviceInfo{};
    }

    // 检查设备是否支持此格式
    if (!targetDevice.isFormatSupported(format_)) {
        qWarning() << "Requested audio format not supported by default device, trying nearest.";
        format_ = targetDevice.nearestFormat(format_);
        // 如果最接近的格式仍然无效，则失败
        if (format_.sampleRate() == 0) { // 无效格式的采样率通常为0
            qCritical() << "No valid audio format found for the default device.";
            return QAudioDeviceInfo{};
        }
    }
    return targetDevice;
}

bool CAudioCapturer::initialize(const QAudioFormat& format, AudioFormat& audioFmt)
{
    if (isInitialized_) {
        qWarning() << "Audio capturer already initialized.";
        return true;
    }
    

    format_ = format;

    // ------------------------- QAudioInput初始化 -------------------------
    audioInput_.reset(new QAudioInput(getDeviceInfo("Main Mic (Razer Seiren Mini)"), format_, this));
    if (!audioInput_) {
        qCritical() << "Failed to create QAudioInput.";
        return false;
    }

    audioInput_->setNotifyInterval(100);

    connect(audioInput_.data(), &QAudioInput::stateChanged, this, &CAudioCapturer::slot_StateChanged);

    // ------------------------- QBuffer初始化 -------------------------
    audioIOBuffer_ = new CIOBuffer{ this };
    audioIOBuffer_->open(QIODevice::ReadWrite | QIODevice::Append);

    // ------------------------- 其他 -------------------------
    audioFmt.sample_rate_ = format_.sampleRate();
    audioFmt.channels_ = format_.channelCount();

    isInitialized_ = true;
    qInfo() << "Audio capturer initialized successfully with format:\n"
			<< "Sample Rate:" << format_.sampleRate() << "\n"
			<< "Channels:" << format_.channelCount() << "\n"
			<< "Sample Size:" << format_.sampleSize() << "\n"
			<< "Sample Format:" << format_.sampleType(); // 使用 sampleFormatName() 获取名称

    return true;
}

void CAudioCapturer::start()
{
    if (!isInitialized_) {
        qWarning() << "Cannot start, capturer is not initialized.";
        return;
    }

    if (audioInput_ && audioInput_->state() != QAudio::ActiveState) {

        audioInput_->start(audioIOBuffer_); //  开始接受PCM数据！
        // 该方法已被抛弃，他妈的!
        /*if (audioIOBuffer_) {
            
            connect(audioIOBuffer_, &QIODevice::readyRead, this, &CAudioCapturer::slot_ReadPCM);
            qInfo() << "Audio capture started.";
        }
        else {
            qCritical() << "Failed to start audio capture, QAudioInput::start() returned nullptr.";
        }*/
    }
    else
    {
        qDebug() << "audioInput_ has started";
    }
}

void CAudioCapturer::stop()
{
    if (audioInput_ && audioInput_->state() != QAudio::StoppedState) 
    {
        audioInput_->stop();
        disconnect(audioInput_.data(), &QAudioInput::stateChanged, this, &CAudioCapturer::slot_StateChanged);
        if (audioIOBuffer_ && audioIOBuffer_->isOpen()) {
            audioIOBuffer_->close();
        }
        delete audioIOBuffer_;
        qInfo() << "Audio capture stopped.";
    }
}

QByteArray CAudioCapturer::readChunk(qint64 chunkSize)
{
	return audioIOBuffer_->readChunk(chunkSize);
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
    else if (newState == QAudio::IdleState) {
        // 如果在启动后又回到了 IdleState，说明启动失败
        if (audioInput_->error() != QAudio::NoError) {
            qWarning() << "Audio input failed to start, error:" << audioInput_->error();
        }
    }
}
