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

    // ��ʼ����Ƶ�����豸�������Ƿ�ɹ�
    bool initialize(const QAudioFormat& format);

    // ��ʼ����
    void start();

    // ֹͣ����
    void stop();

    // ��ȡ��Ƶ�����������ã��Ա������߿��Է���
    QByteArray& getBuffer();
    QMutex& getMutex();

    // ��ȡ����ȷ������Ƶ��ʽ
    QAudioFormat getAudioFormat() const;

private slots:
    // QAudioInput ��״̬�仯ʱ����
    void slot_StateChanged(QAudio::State newState);

    void slot_ReadPCM();

private:
    QAudioFormat format_;
    QScopedPointer<QAudioInput> audioInput_;
    QIODevice* audioDevice_ = nullptr;       // ������� QAudioInput::start() ���ص��豸

    // �̰߳�ȫ����Ƶ���ݻ�����
    mutable QMutex mtx_;
    QByteArray buffer_;

    bool isInitialized_ = false;
};

#endif // CAUDIOCAPTURER_H