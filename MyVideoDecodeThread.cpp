#include "MyVideoDecodeThread.h"
#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件
#pragma warning(disable:4576)

Q_LOGGING_CATEGORY(logVideoSync, "player.video.decode.sync") // 定义，名称为 ""

MyVideoDecodeThread::MyVideoDecodeThread(QObject *parent)
    : QThread(parent)
{
    // 开始时刻
    m_start = std::chrono::high_resolution_clock::now();
}

MyVideoDecodeThread::~MyVideoDecodeThread(){}

void MyVideoDecodeThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void MyVideoDecodeThread::stopThread()
{
    m_stop = 1;
}

int MyVideoDecodeThread::decode_packet(AVCodecContext *dec, const AVPacket *pkt, AVFrame *frame)
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
        // if (dec->codec->type == AVMEDIA_TYPE_VIDEO)
        //     ret = output_video_frame(frame);

        qDebug()<<"is->height:"<<is->video_dec_ctx->height
                 <<"is->width:"<<is->video_dec_ctx->width
                 <<"frame->linesize[0]"<<frame->linesize[0];

        QImage rgb;
        yuv_to_rgb(frame,rgb);
        {
            // pts概念：呈现时间戳Presentation timestamp
            // 时间换算：duration = pts * 时间基time_base

            // 解码后的frame中的时间基time_base是0：QString::number(av_q2d(frame->time_base),'d',20);//"0.00000000000000000000"
            // video_dec_ctx->time_base也是0
            //player.video.decode.sync: frame pts:	 512 	 frame->time_base:	 "0.0000000000" 	 video_stream->time_base	 "0.0000781250" 	 video_dec_ctx->time_base	 "0.0000000000"
            // qCDebug(logVideoSync) <<"frame pts:\t"<< frame->pts<<"\t"
            //                       <<"frame->time_base:\t"<< QString::number(av_q2d(frame->time_base),'d',10)<<"\t"
            //                       <<"video_stream->time_base\t"<<QString::number(av_q2d(is->video_stream->time_base),'d',10)<<"\t"
            //                       <<"video_dec_ctx->time_base\t"<<QString::number(av_q2d(is->video_dec_ctx->time_base),'d',10);
            //结论：时间基用video_stream->time_base

            double video_clock = frame->pts * av_q2d(is->video_stream->time_base);
            double audio_clock = is->audio_clock;

            QString diff = QString::number((video_clock-audio_clock),'d',15);

            qCDebug(logVideoSync) <<"frame pts:\t"<< frame->pts<<"\t"
                                  <<"video_clock:\t"<< video_clock<<"\t"
                                  <<"audio_clock:\t"<< audio_clock<<"       \t"
                                  <<"diff："<< diff;

            // 每帧持续时间ms
            AVRational frame_dur = av_inv_q(is->video_stream->avg_frame_rate);//平均帧率的倒数，也就是每帧持续时间(s)
            double sleep_dur = av_q2d(frame_dur) * 1000;//(ms)

            // 如果“视频时钟快”于“音频时钟”
            if(video_clock > audio_clock)
                msleep(sleep_dur);
        }
        emit sendCurrentFrame(rgb.copy());

        av_frame_unref(frame);

        // // msleep(15);
        // // Sleep(1);
        // qDebug()<<"avg_frame_rate:"<<av_q2d(is->video_stream->avg_frame_rate);//运行结果：avg_frame_rate: 25
        // qDebug()<<"av_inv_q(avg_frame_rate):"<<av_q2d(av_inv_q(is->video_stream->avg_frame_rate));//运行结果：av_inv_q(avg_frame_rate): 0.04
        // // 先求帧率的倒数（单帧时长，单位秒），再转毫秒
        // AVRational frame_time = av_inv_q(is->video_stream->avg_frame_rate);//平均帧率的倒数，也就是每帧持续时间(s)
        // double sleep_ms = av_q2d(frame_time) * 1000;//(ms)
        // // msleep(sleep_ms);
        //
        // //记录当前时刻
        // m_end = std::chrono::high_resolution_clock::now();
        // //转为ms
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - m_start).count();
        // while(duration<sleep_ms){
        //     //记录当前时刻
        //     m_end = std::chrono::high_resolution_clock::now();
        //     duration = std::chrono::duration_cast<std::chrono::milliseconds>(m_end - m_start).count();
        //     // msleep(1);
        // }
        // qDebug()<<"duration:"<<duration;//运行结果：duration: 40
        // m_start = std::chrono::high_resolution_clock::now();
    }

    return ret;
}

void MyVideoDecodeThread::run()
{
    int ret = 0;

    if(!is){
        qDebug() << "解码线程的is为空";
        return;
    }
    if (is->video_stream)
        qDebug() << "Decoding video from file '" << is->iFile;

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
        if(is->videoq.dequeue(pkt,m_stop) < 0){
            qDebug() << "解码线程：获取包失败。";
            break;
        }

        ret = decode_packet(is->video_dec_ctx, pkt ,frame);
        av_packet_unref(pkt);
        if (ret < 0)
            break;
        qDebug()<<"解码线程：完成消费：pkt_size :"<<is->videoq.getSize();
    }

end:
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
