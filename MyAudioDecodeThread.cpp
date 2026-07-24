#include "MyAudioDecodeThread.h"
#include "mainwindow.h" //FFmpegPlayerCtx结构体前向声明后，还需要在.cpp包含该头文件
#pragma warning(disable:4576)

Q_LOGGING_CATEGORY(logAudioClock, "player.audio.decode.clock") // 定义，名称为 ""

static void FN_Audio_Cb(void *userdata, Uint8 *stream, int len)
{
    MyAudioDecodeThread *adt = (MyAudioDecodeThread*)userdata;
    adt->getAudioData(stream,len);
}

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

        /**
         *  将数据转化为目标格式
         */
        QByteArray dst;
        swr_cvt_pcm(frame,dst);
        is->audio_buf_q.enqueue(dst.data(),dst.size());

        {
            // 累计入队Byte
            uint64_t bytes = is->total_enqueued_pcm_bytes+= dst.size();
            // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
            double channels = static_cast<double>(is->audio_tgt_channels);
            double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
            uint64_t samples = (bytes / channels) / bytes_per_sample;
            // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
            double sample_rate = is->audio_tgt_freq;
            double duration = samples / sample_rate;

            // qCDebug(logAudioClock) <<"total_enqueued_pcm_bytes:\t"<<bytes<<"(Byte)\t"
            //                        <<"samples:\t"<<samples<<"\t"
            //                        <<"duration:\t"<<duration;
        }
        // //第一步：将解码后的数据拷贝到audio_buf
        // is->audio_buf_q.enqueue((const char*)frame->data[0],
        //                         frame->nb_samples * bytes_per_sample);
        qDebug()<<"audio_buf队列数量："<<is->audio_buf_q.getSize();

        is->audio_enqueue_pts = frame->pts * av_q2d(is->audio_stream->time_base);//frame->time_base = 0,！！！24、时间基time_base用AVStream
        // qCDebug(logPts) << "audio_enqueue_pts：" << is->audio_enqueue_pts;
        {
            //frame的PCM播放持续时间
            uint64_t bytes = dst.size();
            // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
            double channels = static_cast<double>(is->audio_tgt_channels);
            double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
            uint64_t samples = (bytes / channels) / bytes_per_sample;
            // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
            double sample_rate = is->audio_tgt_freq;
            double duration = samples / sample_rate;

            double pts = frame->pts * av_q2d(is->audio_stream->time_base);//frame->time_base = 0,！！！24、时间基time_base用AVStream
            double frame_duration = duration;
            is->audio_enqueue_tail_clock = pts + frame_duration;
        }

        av_frame_unref(frame);
    }

    return ret;
}

int pcmS16PeakBarDownSampling(int16_t *src,const int srcLen,int16_t &dstMax,int16_t &dstMin)
{
    //求采样点集的最大最小值，无论是LRLRLR,LLLRRR,LLLLLL

    if (!src || srcLen <= 0)
        return -1;

    int16_t maxVal = std::numeric_limits<int16_t>::min();//-32768 获取 qint16 类型能表示的最小值。
    int16_t minVal = std::numeric_limits<int16_t>::max();// 32767 获取 qint16 类型能表示的最大值。

    for (int i = 0; i < srcLen; ++i) {
        maxVal = qMax(maxVal, src[i]);
        minVal = qMin(minVal, src[i]);
    }

    dstMax = maxVal;
    dstMin = minVal;

    return 0;
}

void MyAudioDecodeThread::getAudioData(unsigned char *stream, int len)
{
    // decoder is not ready or in pause state, output silence
    if (!is->audio_dec_ctx) {
        memset(stream, 0, len);
        return;
    }

    {
        // 队内剩余Byte
        int bytes = is->audio_buf_q.getSize();
        // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
        double channels = static_cast<double>(is->audio_tgt_channels);
        double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
        uint64_t samples = (bytes / channels) / bytes_per_sample;
        // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
        double sample_rate = is->audio_tgt_freq;
        double duration = samples / sample_rate;

        // pts - 队内剩余Byte的时间 = 音频时钟
        is->audio_clock2 = is->audio_enqueue_tail_clock - duration;
        qCDebug(logPts) << "ffmpeg-simple-player audio_clock:" << is->audio_clock2;
    }

    int ret = is->audio_buf_q.dequeue(stream, len, m_stop,is->pause);
    if (ret == -1) {
        memset(stream, 0, len);
        return;
    }


    // QByteArray pcm(reinterpret_cast<const char*>(stream), len);
    // emit sendDequeuedPcmBytes(pcm);

    {
        // 累计出队Byte
        uint64_t bytes = is->total_dequeued_pcm_bytes+= len;
        // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
        double channels = static_cast<double>(is->audio_tgt_channels);
        double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
        uint64_t samples = (bytes / channels) / bytes_per_sample;
        // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
        double sample_rate = is->audio_tgt_freq;
        double duration = samples / sample_rate;

        qCDebug(logAudioClock) <<"total_dequeued_pcm_bytes:\t"<<bytes<<"(Byte)\t"
                               <<"samples:\t"<<samples<<"\t"
                               <<"duration:\t"<<duration;

        is->audio_clock = duration;

        {
            int16_t max,min;
            pcmS16PeakBarDownSampling(reinterpret_cast<int16_t *>(stream),
                                      len / bytes_per_sample,
                                      max,
                                      min);
            sendpcmPeakBar(duration,max,min);
        }

    }
    {
        // 队内剩余Byte
        int bytes = is->audio_buf_q.getSize();
        // 换算成采样点sample，公式：Byte = ( sample * 采样点的位深 ) * 声道数
        double channels = static_cast<double>(is->audio_tgt_channels);
        double bytes_per_sample = av_get_bytes_per_sample(is->audio_tgt_fmt);
        uint64_t samples = (bytes / channels) / bytes_per_sample;
        // 换算成时间s，公式：s = 采样点 / 每秒采样次数sample_rate
        double sample_rate = is->audio_tgt_freq;
        double duration = samples / sample_rate;

        // pts - 队内剩余Byte的时间 = 音频时钟
        is->audio_pts_clock = is->audio_enqueue_pts - duration;
        is->audio_clock2 = is->audio_enqueue_tail_clock - duration;
        qCDebug(logPts) << "audio_clock:" << is->audio_clock
                        << "\t\t audio_pts_clock:" << is->audio_pts_clock
                        << "\t\t 绝对差：" << abs(is->audio_clock - is->audio_pts_clock)
                        << "\t\t audio_clock2:" << is->audio_clock2;
    }
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

    /* SDL初始化*/
    SDL_AudioSpec spec;
    //SDL initialize
    if (SDL_Init(SDL_INIT_AUDIO))    // 支持AUDIO
    {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }
    // 音频参数设置SDL_AudioSpec
    // spec.freq = is->audio_dec_ctx->sample_rate;// 采样频率48000
    // spec.format = AUDIO_F32SYS; // 采样点格式 AUDIO_S16SYS
    // spec.channels = 1; //is->audio_dec_ctx->ch_layout.nb_channels;// 2通道
    // spec.silence = 0;
    // spec.samples = 1024;// 23.2ms -> 46.4ms 每次读取的采样数量，多久产生一次回调和 samples
    // spec.callback = FN_Audio_Cb; // 回调函数
    // spec.userdata = this;
    spec.freq = is->audio_tgt_freq;
    spec.format = is->audio_tgt_sdl_fmt;
    spec.channels = is->audio_tgt_channels;
    spec.silence = 0;
    spec.samples = 1024;// 23.2ms -> 46.4ms 每次读取的采样数量，多久产生一次回调和 samples
    spec.callback = FN_Audio_Cb; // 回调函数
    spec.userdata = this;

    qDebug() <<"spec.freq ="<<is->audio_dec_ctx->sample_rate
             <<"spec.channels ="<<is->audio_dec_ctx->ch_layout.nb_channels;
    //打开音频设备
    if (SDL_OpenAudio(&spec, NULL))
    {
        fprintf(stderr, "Failed to open audio device, %s\n", SDL_GetError());
        goto _FAIL;
    }
    //play audio
    SDL_PauseAudio(0);

    while (true) {

        if(m_stop)
            break;

        //seek后，刷新dec_ctx解码上下文，音频解码线程还得单独清理PCM buf队列
        if (is->flush_actx) {
            is->flush_actx = false;
            qCDebug(logSeek) << "音频解码线程：seek后，刷新dec_ctx解码上下文";
            avcodec_flush_buffers(is->audio_dec_ctx);
            //清理PCM buf队列，不然1~2秒后才会播放seek的音频
            is->audio_buf_q.bufFlush();
            continue;
        }

        // 检查audio_buf队列的数量
        if(is->audio_buf_q.getSize() > MAX_AUDIO_BUF_Q_SIZE){
            msleep(10);// SDL_Delay(10);
            continue;
        }

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
_FAIL:
    //release some resources
    // 关闭音频设备
    SDL_CloseAudio();
    //quit SDL
    SDL_Quit();
end:
    av_frame_free(&frame);
    av_packet_free(&pkt);
}
