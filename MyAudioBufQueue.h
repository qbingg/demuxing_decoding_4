#ifndef MYAUDIOBUFQUEUE_H
#define MYAUDIOBUFQUEUE_H

#include <QDebug>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QWaitCondition>
class MyAudioBufQueue : public QObject
{
    Q_OBJECT
public:
    explicit MyAudioBufQueue(QObject *parent = nullptr)
        : QObject{parent}
    {}

    //入队
    int enqueue(const char* data, int len)
    {
        int ret = 0;

        if((!data)||!len){
            qDebug()<<"入队失败："<<"data:"<<data<<",len:"<<len;
            return -1;
        }

        {
            const QMutexLocker locker(&mutex);//加锁，锁不上就在这里阻塞着
            // 入队
            queue.append(data,len);
            size += len;

            cond.wakeOne();// 队列不为空，可以唤醒一个因为cond.wait(&mutex, 500);阻塞的消费者了。
        }
        return ret;
    }
    //出队
    int dequeue(char* data, int len, std::atomic<bool> &quit)
    {
        int ret = 0;
        {
            const QMutexLocker locker(&mutex);//加锁，锁不上就在这里阻塞着

            while(true)
            {
                if(size >= len){
                    memcpy(data, queue.constData(), len);
                    queue.remove(0, len);   // 从头部移除 len 字节
                    size -= len;            // 更新记录的长度

                    ret = 1;
                    break;
                } else {
                    // 带10ms超时等待（完全对应SDL_CondWaitTimeout）
                    // wait返回false=超时，true=被唤醒
                    cond.wait(&mutex, 10);
                }

                // 退出标记
                if (quit) {
                    ret = -1;
                    break;
                }
            }
        }
        return ret;
    }

    int getSize() const
    {
        return size;
    }

signals:

private:
    QByteArray queue;
    std::atomic<int> size = 0;

    QMutex mutex; // protects the buffer and the counter
    // 不建议参照QWaitCondition的Wait Conditions Example生产者消费者示例
    // 而使用两个QWaitCondition，因为生产者消费者相互阻塞和唤醒得有个前提：知道size的最大值。
    // 再者，队列就是队列，将生产者消费者的判断逻辑写在队列，不太好。
    // QWaitCondition queueNotEmpty;
    // QWaitCondition queueNotFull;
    QWaitCondition cond;
};


#endif // MYAUDIOBUFQUEUE_H
