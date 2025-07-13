#pragma once

#include <QList>
#include <QMutex>

template <class T>
class MsgQueue : public QList<T>
{
public:
    MsgQueue()
    {

    }

    ~MsgQueue()
    {

    }
    void enqueue(const T& t) {
        m_mutex.lock();
        QList<T>::append(t);
        m_mutex.unlock();
    }

    T dequeue()
    {
        m_mutex.lock();
        T t = NULL;
        if (!QList<T>::isEmpty())
            t = QList<T>::takeFirst();
        m_mutex.unlock();
        return t;
    }

    bool isEmpty()
    {
        m_mutex.lock();
        bool b = QList<T>::isEmpty();
        m_mutex.unlock();
        return b;
    }

private:
    QMutex m_mutex;
};