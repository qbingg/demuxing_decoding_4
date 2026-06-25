#include "MyAudioDecodeThread.h"
#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件
#pragma warning(disable:4576)
MyAudioDecodeThread::MyAudioDecodeThread(QObject *parent)
    : QThread(parent)
{

}

MyAudioDecodeThread::~MyAudioDecodeThread(){}

int MyAudioDecodeThread::decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame *frame)
{
    int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        qDebug() << "Error submitting a packet for decoding (" << av_err2str(ret) << ")";
        return ret;
    }

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;

            qDebug() << "Error during decoding (" << av_err2str(ret) << ")";
            return ret;
        }

        // // write the frame data to output file
        // if (dec->codec->type == AVMEDIA_TYPE_AUIDO)
        //     ret = output_audio_frame(frame);
        /* 区别于视频解码线程的decode_packet()，就在这了：*/
        // 获取音频格式信息
        AVSampleFormat fmt = (AVSampleFormat)frame->format;
        int channels = frame->ch_layout.nb_channels;
        int bytes_per_sample = av_get_bytes_per_sample(fmt);
        bool planar = av_sample_fmt_is_planar(fmt);

        qDebug() << "Audio frame: format =" << av_get_sample_fmt_name(fmt)
                 << ", channels =" << channels
                 << ", samples =" << frame->nb_samples
                 << ", planar =" << planar;

        // 打印原始字节数据（仅前 32 字节避免刷屏）
        if (planar) {
            // planar 格式：每个声道数据存放在 frame->data[ch]
            for (int ch = 0; ch < channels; ++ch) {
                QByteArray raw((const char*)frame->data[ch],
                               frame->nb_samples * bytes_per_sample);
                qDebug() << "  ch" << ch << "raw (hex):" << raw.left(32).toHex();
            }
        } else {
            // packed 格式：所有声道交错存放在 frame->data[0]
            QByteArray raw((const char*)frame->data[0],
                           frame->nb_samples * channels * bytes_per_sample);
            qDebug() << "  raw (hex):" << raw.left(32).toHex();
        }

        av_frame_unref(frame);
    }

    return ret;
}

void MyAudioDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void MyAudioDecodeThread::stopThread()
{
    m_stop = 1;
}

void MyAudioDecodeThread::run()
{
    int ret = 0;

    if(!is){
        qDebug() << "音频解码线程的is为空";
        return;
    }
    if (is->video_stream)
        qDebug() << "Decoding audio from file '" << is->iFile;

    AVFrame *frame = NULL;
    AVPacket *pkt = NULL;
    frame = av_frame_alloc();
    if (!frame) {
        qDebug() << "Could not allocate frame";
        ret = AVERROR(ENOMEM);
        goto end;
    }
    pkt = av_packet_alloc();
    if (!pkt) {
        qDebug() << "Could not allocate packet";
        ret = AVERROR(ENOMEM);
        goto end;
    }

    while (true) {

        if(m_stop)
            break;

        //尝试从队列获取一个包（阻塞）
        if(is->audioq.dequeue(pkt,m_stop) < 0){
            qDebug() << "解码线程：获取包失败。";
            break;
        }

        ret = decode_packet(is->audio_dec_ctx, pkt ,frame);
        av_packet_unref(pkt);
        if (ret < 0)
            break;
        qDebug()<<"解码线程：完成消费：pkt_size :"<<is->videoq.getSize();
    }

end:
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
