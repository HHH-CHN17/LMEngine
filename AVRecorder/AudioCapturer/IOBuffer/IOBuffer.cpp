#include "IOBuffer.h"

CIOBuffer::CIOBuffer(QObject* parent)
    : QIODevice(parent)
{
}

bool CIOBuffer::open(OpenMode mode)
{
    // ���û���� open ��������ȷ��ģʽ
    return QIODevice::open(mode);
}

// д�뷽���� QAudioInput �̵߳���
qint64 CIOBuffer::writeData(const char* data, qint64 len)
{
	/*{
        QMutexLocker locker(&mtx_);
        // ��ĩβ׷������
        buffer_.append(data, len);
	}*/


    return len;
}

// ��ȡ���������̵߳���
QByteArray CIOBuffer::readChunk(qint64 chunkSize)
{
    /*QMutexLocker locker(&mtx_);

    // 1. ������ݳ����Ƿ��㹻
    if (buffer_.size() < chunkSize) {
        return QByteArray(); // ���ؿ����飬��ʾ���ݲ���
    }

    // 2. ������ͷ�̶����ȵ����ݿ�
    QByteArray chunk = buffer_.left(chunkSize);

    // 3. �ӻ�������ͷ�Ƴ��ѱ���ȡ������
    buffer_.remove(0, chunkSize);*/

    QByteArray chunk{ static_cast<int>(chunkSize), Qt::Initialization::Uninitialized };

    // �ӻ��λ�������ȡ����
	// �ú�������ֵֻ���������ֽ����bytes_read==0 or bytes_read==chunkSize
    size_t bytes_read = ringBuffer_.read(chunk.data(), chunkSize);

    // ���û������˵�����ݲ��㣬���ؿ�
    if (bytes_read < chunkSize) {
        return QByteArray{};
    }
    // �����ȡ�ɹ���������С������
    // chunk.resize(bytes_read); // ��������ز���chunkSize�����ݣ�����Ҫ����

    return chunk;
}

qint64 CIOBuffer::bytesAvailable() const
{
    /*QMutexLocker locker(&mtx_);
    return buffer_.size();*/

    return ringBuffer_.get_size();
}

// ����������ǲ�������ʹ�ã���Ϊ�˱��� QIODevice �������Կ��Լ�ʵ��
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