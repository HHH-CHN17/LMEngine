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

bool CAudioCapturer::initialize(const QAudioFormat& format)
{
    if (isInitialized_) {
        qWarning() << "Audio capturer already initialized.";
        return true;
    }

    format_ = format;
    // ��ȡĬ�������豸��Ϣ
    const QAudioDeviceInfo defaultDevice = QAudioDeviceInfo::defaultInputDevice();
    if (defaultDevice.isNull()) {
        qCritical() << "No audio input device found.";
        return false;
    }

    // ����豸�Ƿ�֧�ִ˸�ʽ
    if (!defaultDevice.isFormatSupported(format_)) {
        qWarning() << "Requested audio format not supported by default device, trying nearest.";
        format_ = defaultDevice.nearestFormat(format_);
        // �����ӽ��ĸ�ʽ��Ȼ��Ч����ʧ��
        if (format_.sampleRate() == 0) { // ��Ч��ʽ�Ĳ�����ͨ��Ϊ0
            qCritical() << "No valid audio format found for the default device.";
            return false;
        }
    }

    // ���� QAudioInput ʵ��
    audioInput_.reset(new QAudioInput(defaultDevice, format_, this));
    if (!audioInput_) {
        qCritical() << "Failed to create QAudioInput.";
        return false;
    }

    // ����״̬�仯�źţ����ڵ���
    connect(audioInput_.data(), &QAudioInput::stateChanged, this, &CAudioCapturer::handleStateChanged);

    isInitialized_ = true;
    qInfo() << "Audio capturer initialized successfully with format:";
    qInfo() << "Sample Rate:" << format_.sampleRate();
    qInfo() << "Channels:" << format_.channelCount();
    qInfo() << "Sample Format:" << format_.sampleType(); // ʹ�� sampleFormatName() ��ȡ����

    return true;
}

void CAudioCapturer::start()
{
    if (!isInitialized_) {
        qWarning() << "Cannot start, capturer is not initialized.";
        return;
    }

    // QAudioInput::start() ����һ�� QIODevice�����в�������ݶ��ᱻд������豸
    // ������Ҫ������豸�ж�ȡ����
    if (audioInput_ && audioInput_->state() != QAudio::ActiveState) {
        // ��վɵĻ���������
        {
            QMutexLocker locker(&mtx_);
            buffer_.clear();
        }

        audioDevice_ = audioInput_->start(); //  ��ʼ����PCM���ݣ�
        if (audioDevice_) {
            // ������� IO �豸�� readyRead �ź�
            // ����ۺ�������� audioDevice_ ��ȡ���ݲ����� buffer_
            connect(audioDevice_, &QIODevice::readyRead, this, [this]() {
                if (!audioDevice_) return;

                const qint64 bytesAvailable = audioDevice_->bytesAvailable();
                if (bytesAvailable <= 0) return;

                QByteArray newData = audioDevice_->read(bytesAvailable);
                if (newData.isEmpty()) return;

                QMutexLocker locker(&mtx_);
                buffer_.append(newData);
                });
            qInfo() << "Audio capture started.";
        }
        else {
            qCritical() << "Failed to start audio capture, QAudioInput::start() returned nullptr.";
        }
    }
}

void CAudioCapturer::stop()
{
    if (audioInput_ && audioInput_->state() != QAudio::StoppedState) {
        audioInput_->stop();
        // audioDevice_ ���� QAudioInput ֹͣʱ�Զ���Ϊ��Ч������Ҫ�ֶ��Ͽ����ӻ�ɾ��
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

void CAudioCapturer::handleStateChanged(QAudio::State newState)
{
    // �������Ҫ���ڵ��ԣ����Թ۲���Ƶ�豸��״̬�仯
    qDebug() << "QAudioInput state changed to:" << newState;
    if (newState == QAudio::StoppedState) {
        if (audioInput_->error() != QAudio::NoError) {
            qWarning() << "Audio input error occurred:" << audioInput_->error();
        }
    }
}