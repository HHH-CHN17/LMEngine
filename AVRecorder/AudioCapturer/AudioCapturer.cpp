#include "AudioCapturer.h"
#include <QAudioDeviceInfo> // <--- ���ڻ�ȡ�豸��Ϣ
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

    // 2. �������п��õ������豸
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

    // ����豸�Ƿ�֧�ִ˸�ʽ
    if (!targetDevice.isFormatSupported(format_)) {
        qWarning() << "Requested audio format not supported by default device, trying nearest.";
        format_ = targetDevice.nearestFormat(format_);
        // �����ӽ��ĸ�ʽ��Ȼ��Ч����ʧ��
        if (format_.sampleRate() == 0) { // ��Ч��ʽ�Ĳ�����ͨ��Ϊ0
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

    // ------------------------- QAudioInput��ʼ�� -------------------------
    audioInput_.reset(new QAudioInput(getDeviceInfo("Main Mic (Razer Seiren Mini)"), format_, this));
    if (!audioInput_) {
        qCritical() << "Failed to create QAudioInput.";
        return false;
    }

    audioInput_->setNotifyInterval(100);

    connect(audioInput_.data(), &QAudioInput::stateChanged, this, &CAudioCapturer::slot_StateChanged);

    // ------------------------- QBuffer��ʼ�� -------------------------
    audioIOBuffer_ = new CIOBuffer{ this };
    audioIOBuffer_->open(QIODevice::ReadWrite | QIODevice::Append);

    // ------------------------- ���� -------------------------
    audioFmt.sample_rate_ = format_.sampleRate();
    audioFmt.channels_ = format_.channelCount();

    isInitialized_ = true;
    qInfo() << "Audio capturer initialized successfully with format:\n"
			<< "Sample Rate:" << format_.sampleRate() << "\n"
			<< "Channels:" << format_.channelCount() << "\n"
			<< "Sample Size:" << format_.sampleSize() << "\n"
			<< "Sample Format:" << format_.sampleType(); // ʹ�� sampleFormatName() ��ȡ����

    return true;
}

void CAudioCapturer::start()
{
    if (!isInitialized_) {
        qWarning() << "Cannot start, capturer is not initialized.";
        return;
    }

    if (audioInput_ && audioInput_->state() != QAudio::ActiveState) {

        audioInput_->start(audioIOBuffer_); //  ��ʼ����PCM���ݣ�
        // �÷����ѱ������������!
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
    // �������Ҫ���ڵ��ԣ����Թ۲���Ƶ�豸��״̬�仯
    qDebug() << "QAudioInput state changed to:" << newState;
    if (newState == QAudio::StoppedState) {
        if (audioInput_->error() != QAudio::NoError) {
            qWarning() << "Audio input error occurred:" << audioInput_->error();
        }
    }
    else if (newState == QAudio::IdleState) {
        // ������������ֻص��� IdleState��˵������ʧ��
        if (audioInput_->error() != QAudio::NoError) {
            qWarning() << "Audio input failed to start, error:" << audioInput_->error();
        }
    }
}
