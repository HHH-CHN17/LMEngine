#include "IOBuffer.h"

CIOBuffer::CIOBuffer(QObject* parent)
    : QIODevice(parent)
{
}

bool CIOBuffer::open(OpenMode mode)
{
    // 调用基类的 open 来设置正确的模式
    return QIODevice::open(mode);
}

// 写入方：由 QAudioInput 线程调用
qint64 CIOBuffer::writeData(const char* data, qint64 len)
{
	/*{
        QMutexLocker locker(&mtx_);
        // 在末尾追加数据
        buffer_.append(data, len);
	}*/


    return len;
}

// 读取方：由主线程调用
QByteArray CIOBuffer::readChunk(qint64 chunkSize)
{
    /*QMutexLocker locker(&mtx_);

    // 1. 检查数据长度是否足够
    if (buffer_.size() < chunkSize) {
        return QByteArray(); // 返回空数组，表示数据不够
    }

    // 2. 拷贝开头固定长度的数据块
    QByteArray chunk = buffer_.left(chunkSize);

    // 3. 从缓冲区开头移除已被读取的数据
    buffer_.remove(0, chunkSize);*/

    QByteArray chunk{ static_cast<int>(chunkSize), Qt::Initialization::Uninitialized };

    // 从环形缓冲区读取数据
	// 该函数返回值只可能有两种结果：bytes_read==0 or bytes_read==chunkSize
    size_t bytes_read = ringBuffer_.read(chunk.data(), chunkSize);

    // 如果没读够，说明数据不足，返回空
    if (bytes_read < chunkSize) {
        return QByteArray{};
    }
    // 如果读取成功，调整大小并返回
    // chunk.resize(bytes_read); // 如果允许返回不足chunkSize的数据，则需要这行

    return chunk;
}

qint64 CIOBuffer::bytesAvailable() const
{
    /*QMutexLocker locker(&mtx_);
    return buffer_.size();*/

    return ringBuffer_.get_size();
}

// 这个函数我们不再主动使用，但为了保持 QIODevice 的完整性可以简单实现
qint64 CIOBuffer::readData(char* data, qint64 maxlen)
{
    /*QMutexLocker locker(&mtx_);
    qint64 bytesToRead = qMin(maxlen, (qint64)buffer_.size());

    if (bytesToRead <= 0) {
        return 0;
    }

    memcpy(data, buffer_.constData(), bytesToRead);
    buffer_.remove(0, bytesToRead);

    return bytesToRead;*/
	return -1;
}