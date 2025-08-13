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

    // chunkSize: 想要读取的固定块大小
    QByteArray readChunk(qint64 chunkSize);

    // 重写 QIODevice 的方法
    qint64 bytesAvailable() const override;
    bool open(OpenMode mode) override;

protected:
    // QAudioInput 将调用这个函数来写入数据
    qint64 writeData(const char* data, qint64 len) override;

    // 我们不再需要自己实现 readData，因为我们将通过 readChunk() 读取
    qint64 readData(char* data, qint64 maxlen) override;

private:
    //mutable QMutex mtx_;
    //QByteArray buffer_; // buffer 在内部管理，不再依赖外部指针
    SpscRingBuffer ringBuffer_{4 * 1024 * 1024}; // 分配4MB容量
};

#endif // IOBUFFER_H