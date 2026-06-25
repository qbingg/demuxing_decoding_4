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
#include "MyDemuxThread.h"
#include "MyPacketQueue.h"
#include "MyVideoDecodeThread.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

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

};
#endif // MAINWINDOW_H
