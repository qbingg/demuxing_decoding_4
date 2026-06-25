#include "MyDemuxThread.h"

#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件

MyDemuxThread::MyDemuxThread(QObject *parent)
    : QThread(parent)
{

}

MyDemuxThread::~MyDemuxThread()
{

}

void MyDemuxThread::setPlayerCtx(FFmpegPlayerCtx *ctx)
{
    is = ctx;
}

void MyDemuxThread::stopThread()
{
    m_stop = 1;
}

int MyDemuxThread::initDemuxThread()
{
    int ret = 0;
    QByteArray arr1 = is->iFile.absoluteFilePath().toUtf8();
    const char *src_filename = arr1.data();

    AVFormatContext *fmt_ctx = NULL;
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        qDebug()<<"Could not open source file "<<src_filename;
        exit(1);
    }
    is->fmt_ctx = fmt_ctx;
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        qDebug()<<"Could not find stream information";
        exit(1);
    }

    int video_stream_idx = -1;
    AVCodecContext *video_dec_ctx = NULL;
    AVStream *video_stream = NULL;
    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO,is->iFile) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];

        is->video_stream_idx = video_stream_idx;
        is->video_stream = video_stream;
        is->video_dec_ctx = video_dec_ctx;
    }
    int audio_stream_idx = -1;
    AVCodecContext *audio_dec_ctx = NULL;
    AVStream *audio_stream = NULL;
    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO,is->iFile) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];

        is->audio_stream_idx = audio_stream_idx;
        is->audio_dec_ctx = audio_dec_ctx;
        is->audio_stream = audio_stream;
    }

    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!video_stream) {
        qDebug() << "Could not find audio or video stream in the input, aborting";
        ret = 1;
    }

    return ret;
}

void MyDemuxThread::finiDemuxThread()
{
    if(is->fmt_ctx){
        avformat_close_input(&is->fmt_ctx);
        is->fmt_ctx = nullptr;
    }

    if(is->video_dec_ctx){
        avcodec_free_context(&is->video_dec_ctx);
        is->video_dec_ctx = nullptr;
    }
}

int MyDemuxThread::open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, AVMediaType type, QFileInfo src_filename)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;

    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);//返回视频流的stream number，确认fmt_ctx->streams[ret]就是视频流
    if (ret < 0) {
        qDebug() <<"Could not find"<<av_get_media_type_string(type)
        <<"stream in input file "<< src_filename;
        return ret;
    } else {
        /*总的来说就是：
         * 找到fmt_ctx的视频流的索引值
         * 使用视频流的st->codecpar->codec_id创建了一个AVCodec dec对象
         * 根据dec对象new了解码器上下文dec_ctx对象
         * 再把st->codecpar参数填充到dec_ctx对象里
         * 根据dec对象打开了解码器上下文dec_ctx对象
         */

        stream_index = ret;
        st = fmt_ctx->streams[stream_index];//AVStream st指向“AVFormat上下文”的视频流

        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);//dec找到视频流的编码
        if (!dec) {
            qDebug()<<"Failed to find "<<av_get_media_type_string(type)<<" codec";
            return AVERROR(EINVAL);
        }

        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);//“视频编码上下文空指针”根据“dec找到视频流的编码”创建上下文
        if (!*dec_ctx) {
            qDebug() << "Failed to allocate the" << av_get_media_type_string(type) << "codec context";
            return AVERROR(ENOMEM);
        }

        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {//将st->codecpar的值填充到dec_ctx，在参数（par）中没有对应字段的字段则保持不变。
            qDebug() << "Failed to copy" << av_get_media_type_string(type) << "codec parameters to decoder context";
            return ret;
        }

        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            qDebug() << "Failed to open" << av_get_media_type_string(type) << "codec";
            return ret;
        }
        *stream_idx = stream_index;
    }

    return 0;
}

void MyDemuxThread::run()
{
    int ret = 0;

    if(!is){
        qDebug() << "解复用线程的is为空";
        return;
    }
    if (is->video_stream)
        qDebug() << "Demuxing video from file '" << is->iFile;

    AVPacket *pkt = NULL;
    pkt = av_packet_alloc();
    if (!pkt) {
        qDebug() << "Could not allocate packet";
        ret = AVERROR(ENOMEM);
        goto end;
    }
    while (true) {

        if(m_stop)
            break;

        // 检查队列pkt的数量
        if(is->videoq.getSize() > MAX_VIDEOQ_SIZE){
            msleep(10);// SDL_Delay(10);
            continue;
        }

        if(av_read_frame(is->fmt_ctx, pkt) < 0){
            qDebug()<< "av_read_frame error";
            // break;
            msleep(10);
            continue;
        }

        if(pkt->stream_index == is->video_stream_idx){
            is->videoq.enqueue(pkt);
            qDebug()<<"解封装线程：完成生产：视频pkt_size :"<<is->videoq.getSize();
        }else if(pkt->stream_index == is->audio_stream_idx){
            is->audioq.enqueue(pkt);
            qDebug()<<"解封装线程：完成生产：音频pkt_size :"<<is->audioq.getSize();
        }else{
            av_packet_unref(pkt);
        }
    }
end:
    av_packet_free(&pkt);
}
