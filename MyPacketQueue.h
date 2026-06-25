#ifndef MYPACKETQUEUE_H
#define MYPACKETQUEUE_H

extern "C" {
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QWaitCondition>
class MyPacketQueue : public QObject
{
    Q_OBJECT
public:
    explicit MyPacketQueue(QObject *parent = nullptr)
        : QObject{parent}
    {}

    //入队，参考自ffplay.c的int packet_queue_put(PacketQueue *q, AVPacket *pkt)
    int enqueue(AVPacket *pkt)
    {
        AVPacket *pkt1;
        int ret = 0;

        pkt1 = av_packet_alloc();
        if (!pkt1) {
            av_packet_unref(pkt);
            return -1;
        }
        av_packet_move_ref(pkt1, pkt);

        {
            const QMutexLocker locker(&mutex);//加锁，锁不上就在这里阻塞着
            // 入队
            queue.enqueue(pkt1);
            size += pkt1->size;// 注意不是队列元素个数size = queue.size();

            cond.wakeOne();// 队列不为空，可以唤醒一个因为cond.wait(&mutex, 500);阻塞的消费者了。
        }
        return ret;
    }
    //出队
    int dequeue(AVPacket *pkt, std::atomic<bool> &quit)
    {
        int ret = 0;
        {
            const QMutexLocker locker(&mutex);//加锁，锁不上就在这里阻塞着

            while(true)
            {
                if(!queue.isEmpty()){
                    // 取队首packet
                    AVPacket *pkt1 = queue.dequeue();
                    size -= pkt1->size;// 注意不是队列元素个数size = queue.size();
                    av_packet_move_ref(pkt, pkt1);
                    av_packet_free(&pkt1);

                    ret = 1;
                    break;
                } else {
                    // 带500ms超时等待（完全对应SDL_CondWaitTimeout）
                    // wait返回false=超时，true=被唤醒
                    cond.wait(&mutex, 500);
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
    QQueue<AVPacket*> queue;
    /* 参考自ffpaly.c的struct PacketQueue { int size; }
     * size 是总数据量（字节数），不是包个数queue.size()。
     *      用总数据量可以精确控制缓冲区的内存上限，避免大包撑爆内存或小包堆积过多。
     *      这也解释了为什么每次取出包后要 size -= packet.size，而不是简单的 size--。
     *      这种设计在多线程音视频缓冲中是标准做法，将条件变量与字节级流量控制结合，既保证了并发效率，又保证了内存安全。
     */
    std::atomic<int> size = 0;

    QMutex mutex; // protects the buffer and the counter
    // 不建议参照QWaitCondition的Wait Conditions Example生产者消费者示例
    // 而使用两个QWaitCondition，因为生产者消费者相互阻塞和唤醒得有个前提：知道size的最大值。
    // 再者，队列就是队列，将生产者消费者的判断逻辑写在队列，不太好。
    // QWaitCondition queueNotEmpty;
    // QWaitCondition queueNotFull;
    QWaitCondition cond;
};



#endif // MYPACKETQUEUE_H
