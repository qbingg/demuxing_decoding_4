#ifndef MYAUDIODECODETHREAD_H
#define MYAUDIODECODETHREAD_H

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>  // （视频）必须包含这个头文件

#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <QDebug>
#include <QThread>
#include <SDL.h>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(logAudioClock) // 声明

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

    void getAudioData(unsigned char *stream, int len);

    int swr_cvt_pcm(AVFrame *src, QByteArray &dst,
                    const int dst_channels = 2,
                    const int dst_freq_sample_rate = 48000,
                    const enum AVSampleFormat dst_sample_fmt = AV_SAMPLE_FMT_S16)
    {
        /* 第1步、init变量 swr_ctx */
        SwrContext *swr_ctx = nullptr;
        swr_ctx = swr_alloc();//ffmpeg文档：与libavcodec和libavformat不同，此结构是不透明的。这意味着，如果您想设置选项，必须使用AVOptions API，而不能直接为该结构的成员设置值。
        //输入音频格式，直接用音频解码上下文的参数即可
        // av_opt_set_chlayout(swr_ctx, "in_chlayout", &is->audio_dec_ctx->ch_layout, 0);
        // av_opt_set_int(swr_ctx, "in_sample_rate",       is->audio_dec_ctx->sample_rate, 0);
        // av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", is->audio_dec_ctx->sample_fmt, 0);
        //不使用AVCodecContext而是使用AVFrame的参数，这样就不需要输入音频解码上下文
        av_opt_set_chlayout(swr_ctx, "in_chlayout", &src->ch_layout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate",       src->sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", static_cast<AVSampleFormat>(src->format), 0);//需要强转：dectx分为sample_fmt/pix_fmt(enum)区分音频视频，而AVFrame不区分:format(int)

        AVChannelLayout outLayout;
        // use stereo
        av_channel_layout_default(&outLayout, dst_channels);
        //输入音频格式，建议在FFmpegPlayerCtx声明输出格式，而不是使用魔法数字
        av_opt_set_chlayout(swr_ctx, "out_chlayout", &outLayout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate",       dst_freq_sample_rate, 0);
        av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);
        swr_init(swr_ctx);

        /* 第2步、转换音频格式为目标格式 */
        // 确定dst的每个采样点的位深
        int dst_bytes_per_sample = -1;
        switch (dst_sample_fmt) {
        case AV_SAMPLE_FMT_U8:{
            dst_bytes_per_sample = 1;// 8bit
            break;
        }
        case AV_SAMPLE_FMT_S16:{
            dst_bytes_per_sample = 2;// 16bit
            break;
        }
        case AV_SAMPLE_FMT_S32:{
            dst_bytes_per_sample = 4;// 32bit
            break;
        }
        default:
            break;
        }
        if(dst_bytes_per_sample == -1){
            qDebug()<<"swr_cvt_pcm()错误：未知dst_sample_fmt";
            return -1;
        }

        //swr_convert()文档：如果输入的数据量超过输出空间，则输入数据将被缓冲。
        //                  您可以通过使用swr_get_out_samples()函数来获取给定输入样本数所需输出样本数的上限，从而避免这种缓冲。
        int upper_bound_samples_per_channel = swr_get_out_samples(swr_ctx, src->nb_samples);
        uint8_t *out[4] = {0};
        int upper_bound_len = upper_bound_samples_per_channel * dst_bytes_per_sample * dst_channels;
        out[0] = (uint8_t*)av_malloc(upper_bound_len);
        // number of samples output per channel
        int samples = swr_convert(swr_ctx,
                                  out,
                                  upper_bound_samples_per_channel,//每个通道可用的输出采样点 amount of space available for output in samples per channel
                                  (const uint8_t**)src->data,
                                  src->nb_samples
                                  );
        if (samples > 0) {
            // memcpy(is->audio_buf, out[0], samples * 2 * 2);
            // 入队，数据拷贝到audio_buf
            // is->audio_buf_q.enqueue((const char*)out[0],
            //                         samples * dst_bytes * dst_channels);
            int len = samples * dst_bytes_per_sample * dst_channels;
            dst.append((const char*)out[0], len);
        }
        /* 第3步、释放局部资源 */
        av_free(out[0]);
        /* 第4步、释放全局资源 */
        swr_free(&swr_ctx);
        swr_ctx = nullptr;

        return 0;
    }

signals:
    void sendMessage();

private:
    FFmpegPlayerCtx *is = nullptr;

    std::atomic<bool> m_stop = 0;
protected:
    void run()override;
};

#endif // MYAUDIODECODETHREAD_H
