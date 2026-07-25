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
#include <QChart>
#include <QLineSeries>
#include <QValueAxis>
#include <QChartView>
#include <QGraphicsLayout>
#include <QTimer>
#include "MyAudioBufQueue.h"
#include "MyAudioDecodeThread.h"
#include "MyDemuxThread.h"
#include "MyPacketQueue.h"
#include "MyVideoDecodeThread.h"

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(logDurBar) // 声明
Q_DECLARE_LOGGING_CATEGORY(logSeek) // 声明
Q_DECLARE_LOGGING_CATEGORY(logPts) // 声明
Q_DECLARE_LOGGING_CATEGORY(logIDR) // 声明

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

    std::atomic<uint64_t> total_enqueued_pcm_bytes = 0;//记录pcm入队列size数（Byte）
    std::atomic<uint64_t> total_dequeued_pcm_bytes = 0;//记录pcm入队列size数（Byte）

    std::atomic<double> audio_clock = 0;//使用出队PCM Byte clock作为音频时钟
    //使用解码frame.pts-出队size作为音频时钟
    std::atomic<double> audio_enqueue_pts = 0;
    std::atomic<double> audio_pts_clock = 0;
    //这就和 ffmpeg-simple-player 的核心思想一致：用已入队音频末尾时间，减去队列里还没播放的音频时长。
    std::atomic<double> audio_enqueue_tail_clock = 0;//解码frame.pts + frame的PCM播放持续时间
    std::atomic<double> audio_clock2 = 0;

    /* 暂停功能：只需要暂停消费端，生产端不需要控制
     * 1、暂停音频播放设备：SDL_PauseAudio(0);//跟OpenGL一样是状态机，可全局调用
     * 2、暂停视频解码：if(is->pause) {msleep(10);continue;}
     */
    std::atomic<bool> pause = false;

    /* 跳转seek功能
     * 精度为I帧AVPacket级别
     *
     */
    // seek flags and pos for seek
    std::atomic<bool> seek_req;//请求标志
    int              seek_flags;//跳转标志：前进、后退
    int64_t          seek_pos;//跳转的时间(微秒)，注意：seek_pos = sec * AV_TIME_BASE 是先把秒统一成 FFmpeg 的通用微秒时间戳；av_rescale_q 再把这个通用时间戳换算成具体音频/视频流自己的时间基。
    // flush flag for seek//清的是 FFmpeg 解码器内部缓存。（旧的 P/B 帧依赖的前后帧等）
    std::atomic<bool> flush_actx = false;
    std::atomic<bool> flush_vctx = false;




    // int width, height;我直接使用了AVFrame解码后自带的宽高，也就不需要video_dec_ctx->width;
    // enum AVPixelFormat pix_fmt;在demux初始化，在yuv转rgb用到，但是这个项目就是yuv420P转rgb，不考虑其他格式的话，就不需要这个变量
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void initDurPcmChartView();

    void seekRelative(double offsetSec);
    void seekAbsolute(double targetSec);

protected:
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    void resizeEvent(QResizeEvent *event);
private slots:
    void on_pushButton_clicked();

    void on_btnPause_clicked(bool checked);

    void on_btnRewind_clicked();

    void on_btnForward_clicked();

    void on_horizontalSlider_sliderReleased();

private:
    Ui::MainWindow *ui;

    FFmpegPlayerCtx *playerCtx = nullptr;

    MyDemuxThread *m_demuxThread = nullptr;

    MyVideoDecodeThread *m_myVideoDecodeThread = nullptr;

    MyAudioDecodeThread *m_myAudioDecodeThread = nullptr;

    //进度条
    QLineSeries *m_durWaveSeries = nullptr;
    QValueAxis *m_durAxisX = nullptr;
    QValueAxis *m_durAxisY = nullptr;
    QList<QPointF> m_durPoints;//sdl每次取水的min/max点：1024->2（降采样）
    // QList<QPointF> m_durTgtPoints;//依据chartView宽度，按像素数量从points取点
    QTimer m_durTimer;// = nullptr;
    int blockDownSampling(const QList<QPointF>& srcPointList,QList<QPointF>& dstPointList,const int dstBars);
    int intervalDownSampling(const QList<QPointF>& srcPointList,QList<QPointF>& dstPointList,const int dstBarInterval);
    int durBarChartViewDownSampling(const QList<QPointF>& srcPointList,const int totalCbBars,QList<QPointF>& dstPointList,const int width);
};
#endif // MAINWINDOW_H
