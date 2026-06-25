#ifndef MYVIDEODECODETHREAD_H
#define MYVIDEODECODETHREAD_H

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>  // （视频）必须包含这个头文件
}
#include <QImage>
#include <QFileInfo>
#include <QThread>

struct FFmpegPlayerCtx;
class MyVideoDecodeThread : public QThread
{
    Q_OBJECT
public:
    explicit MyVideoDecodeThread(QObject *parent = nullptr);
    ~MyVideoDecodeThread();

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
    void sendCurrentFrame(QImage);

private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;

    /**
     * @brief 将 YUV 帧转换为 RGB QImage。
     * @param src 输入的 AVFrame（YUV420P），必须非空且数据有效。
     * @param dst 输出的 QImage 引用，函数内会重新分配并填充 RGB 数据。
     * @return 0 成功；-1 失败
     */
    int yuv_to_rgb(AVFrame *src,QImage &dst)
    {
        qDebug() << "准备执行AVFrame转QImage，Video dimensions:"
                 << "width =" << src->width
                 << "height =" << src->height
                 << "crop_top =" << src->crop_top
                 << "crop_bottom =" << src->crop_bottom
                 << "crop_left =" << src->crop_left
                 << "crop_right =" << src->crop_right;

        if (src->width == 0)
        {
            qDebug() << "输入的src为空";
            return -1;
        }

        // cv::cvtColor(src,dst,cv::COLOR_YUV2RGB_I420);

        SwsContext* sws_ctx = nullptr;

        sws_ctx = sws_getContext(
            src->width, src->height, AV_PIX_FMT_YUV420P,  // 输入：YUV420P 平面格式
            src->width, src->height, AV_PIX_FMT_RGB24,    // 输出：RGB24 打包格式，与QImage格式完全匹配
            SWS_BILINEAR, nullptr, nullptr, nullptr
            );

        // 直接创建目标 QImage，由它自行管理内存
        QImage qImg(src->width, src->height, QImage::Format_RGB888);
        // 配置输出缓冲区：直接使用 QImage 内部的像素内存
        uint8_t* dst_data[1] = { qImg.bits() };//返回指向第一个像素数据的指针。
        int dst_linesize[1] = { static_cast<int>(qImg.bytesPerLine()) };//等于linesize，Returns the number of bytes per image scanline.

        // 执行格式转换，结果直接写入 QImage 内存
        sws_scale(sws_ctx,
                  src->data, src->linesize,  // 输入 YUV 三平面数据与对应步长
                  0, src->height,                 // 转换全部高度的行
                  dst_data, dst_linesize         // 输出到 QImage 缓冲区
                  );

        dst = qImg.copy();

        if (sws_ctx) {
            sws_freeContext(sws_ctx);
        }

        return 0;
    }

    // 开始计时
    std::chrono::steady_clock::time_point m_start;
    // 停止计时
    std::chrono::steady_clock::time_point m_end;


protected:
    void run()override;
};


#endif // MYVIDEODECODETHREAD_H
