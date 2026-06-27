#ifndef MYAUDIODECODETHREAD_H
#define MYAUDIODECODETHREAD_H

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>  // （视频）必须包含这个头文件
}

#include <QThread>
#include <SDL.h>
struct FFmpegPlayerCtx;
class MyAudioDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit MyAudioDecodeThread(QObject *parent = nullptr);
    ~MyAudioDecodeThread();
    void setPlayerCtx(FFmpegPlayerCtx *ctx);
    void stopThread();

    /**
     * 解码音视频数据包，将AVPacket解析为AVFrame
     * @param dec 解码器上下文，怎么来的请看open_codec_context()
     * @param pkt 输入包
     * @param frame 一个包有多个帧，并不是最终输出帧，只是作为载体，全局变量不用重复new AVFrame
     * @return 解码结果状态码
     */
    int decode_packet(AVCodecContext *dec, const AVPacket *pkt ,AVFrame *frame);


signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;
protected:
    void run()override;
};

#endif // MYAUDIODECODETHREAD_H
