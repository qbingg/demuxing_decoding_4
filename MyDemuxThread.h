#ifndef MYDEMUXTHREAD_H
#define MYDEMUXTHREAD_H

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>  // （视频）必须包含这个头文件
}

#include <QFileInfo>
#include <QThread>
struct FFmpegPlayerCtx;
class MyDemuxThread : public QThread
{
    Q_OBJECT
public:
    explicit MyDemuxThread(QObject *parent = nullptr);
    ~MyDemuxThread();

    void setPlayerCtx(FFmpegPlayerCtx *ctx);
    void stopThread();

    /**
     * 初始化解封装线程
     * 在这里将MainWindow类的私有成员变量playerCtx，里面包含的信息有MainWindow、解复用、解码，3个线程共享
     * 初始化playerCtx里的：AVFormatContext、video_stream_idx、AVCodecContext等信息
     * 至少来说，解码线程一定会用到video_stream_idx、AVCodecContext
     * @return 执行结果状态码
     */
    int initDemuxThread();

    /**
     * 使用ffmpeg方法删除在此线程alloc/open的变量
     * @return
     */
    void finiDemuxThread();

    /**
     * 查找并打开指定类型的最佳流及对应解码器上下文
     * @param stream_idx    （输出）视频流的索引值（从AVFormat上下文寻找到的值）
     * @param dec_ctx       （输出）视频编码上下文（输出为初始化后的解码器上下文）
     * @param fmt_ctx       AVFormat上下文
     * @param type          媒体类型：AVMEDIA_TYPE_VIDEO
     * @param src_filename  输入文件的QFileInfo
     * @return 成功返回非负值，失败返回FFmpeg错误码
     */
    static int open_codec_context(int *stream_idx,
                                  AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type,
                                  QFileInfo src_filename);

signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;
protected:
    void run()override;
};


#endif // MYDEMUXTHREAD_H
