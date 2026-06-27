#ifndef MAINWINDOW_H
#define MAINWINDOW_H

extern "C"{
// demux_decode.c的头文件
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
// yuv 转 rgb
#include <libswscale/swscale.h>
}

#include <QFileInfo>
#include <QMainWindow>
#include <QFileInfo>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>
#include "MyAudioBufQueue.h"
#include "MyAudioDecodeThread.h"
#include "MyDemuxThread.h"
#include "MyPacketQueue.h"
#include "MyVideoDecodeThread.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 根据ffplay.c的q->size += pkt1.pkt->size + sizeof(pkt1);
// 可知，单位是Byte，所以这里的16估计和16位深bit，没什么关系。
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIO_BUF_Q_SIZE ((MAX_AUDIO_FRAME_SIZE * 3) / 2)
// 5 * 256 KB (字节) 1字节Byte = 8比特bit
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
/* 3个线程共享的变量*/
struct FFmpegPlayerCtx {
    QFileInfo iFile;
    AVFormatContext *fmt_ctx = NULL;

    int video_stream_idx = -1;
    AVCodecContext *video_dec_ctx = NULL;
    AVStream *video_stream = NULL;
    MyPacketQueue videoq;

    int audio_stream_idx = -1;
    AVCodecContext *audio_dec_ctx = NULL;
    AVStream *audio_stream = NULL;
    MyPacketQueue audioq;

    MyAudioBufQueue audio_buf_q;

    /* 不要让 SDL spec 直接跟着输入文件走，而是跟着你的目标输出格式走。
     * 约定：SDL照着这些格式初始化，ffmpeg经过sws转为这些格式
     */
    int audio_tgt_freq = 48000;
    AVSampleFormat audio_tgt_fmt = AV_SAMPLE_FMT_S16;
    SDL_AudioFormat audio_tgt_sdl_fmt = AUDIO_S16SYS;
    int audio_tgt_channels = 2;

    // int width, height;我直接使用了AVFrame解码后自带的宽高，也就不需要video_dec_ctx->width;
    // enum AVPixelFormat pix_fmt;在demux初始化，在yuv转rgb用到，但是这个项目就是yuv420P转rgb，不考虑其他格式的话，就不需要这个变量
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private slots:
    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;

    FFmpegPlayerCtx *playerCtx = nullptr;

    MyDemuxThread *m_demuxThread = nullptr;

    MyVideoDecodeThread *m_myVideoDecodeThread = nullptr;

    MyAudioDecodeThread *m_myAudioDecodeThread = nullptr;
};
#endif // MAINWINDOW_H
