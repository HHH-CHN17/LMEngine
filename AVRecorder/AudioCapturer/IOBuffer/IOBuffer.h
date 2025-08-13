#ifndef IOBUFFER_H
#define IOBUFFER_H

#include <QIODevice>
#include <QByteArray>
#include <QMutex>
#include "./Common/SPSCRingBuffer.h"

class CIOBuffer : public QIODevice
{
    Q_OBJECT

public:
    explicit CIOBuffer(QObject* parent = nullptr);

    // chunkSize: ��Ҫ��ȡ�Ĺ̶����С
    QByteArray readChunk(qint64 chunkSize);

    // ��д QIODevice �ķ���
    qint64 bytesAvailable() const override;
    bool open(OpenMode mode) override;

protected:
    // QAudioInput ���������������д������
    qint64 writeData(const char* data, qint64 len) override;

    // ���ǲ�����Ҫ�Լ�ʵ�� readData����Ϊ���ǽ�ͨ�� readChunk() ��ȡ
    qint64 readData(char* data, qint64 maxlen) override;

private:
    //mutable QMutex mtx_;
    //QByteArray buffer_; // buffer ���ڲ��������������ⲿָ��
    SpscRingBuffer ringBuffer_{4 * 1024 * 1024}; // ����4MB����
};

#endif // IOBUFFER_H