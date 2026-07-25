#include "mainwindow.h"
#include "ui_mainwindow.h"

Q_LOGGING_CATEGORY(logDurBar, "player.durBar") // 定义，名称为 ""
Q_LOGGING_CATEGORY(logSeek, "seek")
Q_LOGGING_CATEGORY(logPts, "pts")
Q_LOGGING_CATEGORY(logIDR, "idr")

void stream_seek(FFmpegPlayerCtx *is, double targetSec, int rel = -1)
{
    //把秒统一成 FFmpeg 的通用微秒时间戳
    int64_t pos = targetSec *  AV_TIME_BASE;
    /*默认 rel = -1 是因为：
     * is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
     * 这行是在决定：seek 到目标时间附近时，允许 FFmpeg 选目标时间之前还是之后的关键点。
     * 粗略 seek：可以根据方向决定 flag
     * 精确 seek：统一使用 AVSEEK_FLAG_BACKWARD ，后续再解码到目标帧（因为“精确”需要从目标前面的关键帧开始追帧。）
     *因为精确 seek 的经典流程是：
     * 1、seek 到目标时间之前的关键帧
     *     -> flush decoder
     *     -> 从关键帧开始解码
     *     -> 丢弃 target 前的帧
     *     -> 显示 target 附近/之后的第一帧
     * 2、如果只是普通播放器“粗 seek”，它可能这样设计：
     *     向前 +10s：不带 BACKWARD，尽快跳到后面的关键帧
     *     向后 -10s：带 BACKWARD，保证不要跳到目标之后
     *     这样响应会快一点，但不精确。*/
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
        is->seek_req = true;
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setAcceptDrops(true);// 开启对整个窗口的拖放操作的支持

    ui->btnPause->setCheckable(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initDurPcmChartView()
{
    m_durWaveSeries = new QLineSeries();
    m_durWaveSeries->setName("分块峰值降采样法");
    m_durWaveSeries->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    m_durAxisX = new QValueAxis();
    m_durAxisX->setTitleText("时间 (s)");
    m_durAxisX->setRange(0, (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base))); // 时长 0~duration(音频流)，注意要考虑时间基
    m_durAxisY = new QValueAxis();
    m_durAxisY->setTitleText("采样值");
    m_durAxisY->setRange(-32768, 32767); // 16位有符号整数范围

    QChart *chart = new QChart();
    chart->addSeries(m_durWaveSeries);
    chart->addAxis(m_durAxisX, Qt::AlignBottom);
    chart->addAxis(m_durAxisY, Qt::AlignLeft);

    //波形数据使用这两个坐标轴映射
    m_durWaveSeries->attachAxis(m_durAxisX);
    m_durWaveSeries->attachAxis(m_durAxisY);

    // 显示到UI的QChartView控件（对象名：chartView）
    ui->durPcmChartView->setChart(chart);
    ui->durPcmChartView->setRenderHint(QPainter::Antialiasing); // 抗锯齿

    /*为pcm图表显示进行布局优化*/
    chart->setTitle("");//去掉标题
    chart->legend()->hide();//隐藏图表用于解释颜色和系列名称的图例框
    chart->layout()->setContentsMargins(0, 0, 0, 0);//去掉外层layout的margin间隔
    chart->setMargins(QMargins(0, 0, 0, 0));//去掉chart内层的margin间隔
    chart->setBackgroundRoundness(0);//去掉圆角（Qt文档：此属性表示图表背景四角处圆角的直径。）
    chart->setAnimationOptions(QChart::NoAnimation); // 静态图关闭动画
    // 去掉坐标轴标题
    m_durAxisX->setTitleVisible(false);
    m_durAxisY->setTitleVisible(false);
    // 去掉坐标轴刻度
    // m_durAxisX->setLabelsVisible(false);
    m_durAxisY->setLabelsVisible(false);
    // // 去掉坐标轴网格
    // m_durAxisX->setGridLineVisible(false);
    // m_durAxisY->setGridLineVisible(false);
}

void MainWindow::seekRelative(double offsetSec)
{
    double targetSec = playerCtx->audio_clock + offsetSec;
    //边界检查 0 <= targetSec <= durationSec
    double durationSec = (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base)); // 时长 0~duration(音频流)，注意要考虑时间基
    targetSec = qBound(0.0, targetSec, durationSec);

    qCDebug(logSeek) << "seekRelative to:" << targetSec
                     << "audio_clock:" << playerCtx->audio_clock;

    stream_seek(playerCtx,targetSec);
}

void MainWindow::seekAbsolute(double targetSec)
{
    //边界检查 0 <= targetSec <= durationSec
    double durationSec = (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base)); // 时长 0~duration(音频流)，注意要考虑时间基
    targetSec = qBound(0.0, targetSec, durationSec);

    qCDebug(logSeek) << "seekAbsolute to:" << targetSec
                     << "audio_clock:" << playerCtx->audio_clock;

    stream_seek(playerCtx,targetSec);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction(); // 接受默认的拖放行为
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    // 确保文件数量仅为一个
    if (urls.size() != 1) {
        if (urls.size() > 1) {
            QMessageBox::warning(this, "warning", "请拖入单个文件");
        }
        return;
    }

    const QUrl& url = urls.first();
    QString filePath = url.toLocalFile();

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "warning", "请拖入有效的文件");
        return;
    }

    //获取文件信息，并显示到标题栏上
    setWindowTitle(fileInfo.absoluteFilePath());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    qCDebug(logDurBar)<<"Chart的宽："<<ui->durPcmChartView->width();
}

void MainWindow::on_pushButton_clicked()
{
    if(m_myAudioDecodeThread){
        // m_myAudioDecodeThread->requestInterruption();
        m_myAudioDecodeThread->stopThread();
        m_myAudioDecodeThread->wait();
        delete m_myAudioDecodeThread;
        m_myAudioDecodeThread = nullptr;
    }
    qDebug()<<"已清空视频线程";
    if(m_myVideoDecodeThread){
        // m_myVideoDecodeThread->requestInterruption();
        m_myVideoDecodeThread->stopThread();
        m_myVideoDecodeThread->wait();
        delete m_myVideoDecodeThread;
        m_myVideoDecodeThread = nullptr;
    }
    qDebug()<<"已清空视频线程";
    if(m_demuxThread){
        // m_demuxThread->requestInterruption();
        m_demuxThread->stopThread();
        m_demuxThread->wait();
        m_demuxThread->finiDemuxThread();
        delete m_demuxThread;
        m_demuxThread = nullptr;
    }
    qDebug()<<"已清空解封装线程";
    if(playerCtx){
        delete playerCtx;
        playerCtx = nullptr;
    }
    qDebug()<<"已清空playerCtx";

    // 检查文件是否存在
    QFileInfo fileInfo(windowTitle());
    if (!fileInfo.exists()) {
        QMessageBox::warning(this, "warning", "请拖入有效的文件");
        return;
    }

    /* 为新的视频文件初始化播放器结构体 */
    playerCtx = new FFmpegPlayerCtx;
    // 获取输入文件信息
    playerCtx->iFile = fileInfo;
    // 初始化解封装线程
    m_demuxThread = new MyDemuxThread;
    m_demuxThread->setPlayerCtx(playerCtx);
    if (m_demuxThread->initDemuxThread() != 0) {
        qDebug()<< "DemuxThread init Failed.";
        return;
    }
    // create video decode thread
    m_myVideoDecodeThread = new MyVideoDecodeThread;
    m_myVideoDecodeThread->setPlayerCtx(playerCtx);
    connect(m_myVideoDecodeThread,
            &MyVideoDecodeThread::sendYuv420pFrame,
            ui->widget,
            &MyYUV420POpenGLWidget::setYuv420pFrame,
            Qt::QueuedConnection);


    m_myAudioDecodeThread = new MyAudioDecodeThread;
    m_myAudioDecodeThread->setPlayerCtx(playerCtx);
    initDurPcmChartView();
    m_durPoints.clear();//清空进度条list
    connect(m_myAudioDecodeThread,&MyAudioDecodeThread::sendpcmPeakBar,this,[=](double time,int16_t max,int16_t min){
        m_durPoints.append(QPointF(time,max));
        m_durPoints.append(QPointF(time,min));
    });
    ui->horizontalSlider->setRange(0, (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base))); // 时长 0~duration(音频流)，注意要考虑时间基
    connect(&m_durTimer,&QTimer::timeout,this,[=]{
        // m_durWaveSeries->replace(m_durPoints);

        //第一次采样：totalBars -> totalCbBars
        //第二次采样：totalCbBars -> pixelBars

        //目标柱状图数量
        const int pixelBars = ui->durPcmChartView->width();
        //总时长
        const double duration = playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base);
        //采样率（不是解码后的，而是swr后给sdl播放的）
        const double sampleRate = playerCtx->audio_tgt_freq;//采样率（每秒采样次数）44100.0;
        //总采样数
        const double samples = duration * sampleRate;
        //sdl callback总次数（取水次数）
        const double totalCbBars = samples / 1024.0;
        //第二次采样间隔
        const int dspBarsInterval = totalCbBars / pixelBars;

        QList<QPointF> pList;
        // blockDownSampling(m_durPoints,pList,pixelBars);
        // intervalDownSampling(m_durPoints,pList,dspBarsInterval);
        durBarChartViewDownSampling(m_durPoints,totalCbBars,pList,ui->durPcmChartView->width());

        m_durWaveSeries->replace(pList);

        {
            const QSignalBlocker blocker(ui->horizontalSlider);
            // no signals here
            //如果用户正在拖拽Slider，则不更新
            if (!ui->horizontalSlider->isSliderDown())
                ui->horizontalSlider->setValue(playerCtx->audio_clock2);
        }

    });
    m_durTimer.start(100);

    connect(m_demuxThread,&MyDemuxThread::sendAudioPktIDR,this,[=](double pts){
        QLineSeries *idr = new QLineSeries();
        idr->setPen(QPen(Qt::red, 2));
        ui->durPcmChartView->chart()->addSeries(idr);
        //波形数据使用这两个坐标轴映射
        idr->attachAxis(m_durAxisX);
        idr->attachAxis(m_durAxisY);
        idr->append(pts, -32768);
        idr->append(pts, 32767);
        qCDebug(logIDR)<<"durationSerise->append(duration, 32767);"<<pts;
    });
    connect(m_demuxThread,&MyDemuxThread::sendVideoPktIDR,this,[=](double pts){
        QLineSeries *idr = new QLineSeries();
        idr->setPen(QPen(Qt::black, 2));
        ui->durPcmChartView->chart()->addSeries(idr);
        idr->attachAxis(m_durAxisX);
        idr->attachAxis(m_durAxisY);
        idr->append(pts, -32768);
        idr->append(pts, 32767);
        qCDebug(logIDR)<<"durationSerise->append(duration, 32767);"<<pts;
    });

    m_demuxThread->start();
    m_myVideoDecodeThread->start();
    m_myAudioDecodeThread->start();
}

void MainWindow::on_btnPause_clicked(bool checked)
{
    if(!playerCtx){
        ui->btnPause->setChecked(false);
        return;
    }

    if(checked){
        ui->btnPause->setText("继续");

        playerCtx->pause = true;
        SDL_PauseAudio(1);


    }else{
        ui->btnPause->setText("暂停");

        playerCtx->pause = false;
        SDL_PauseAudio(0);
    }

    qCDebug(logDurBar) << "playerCtx->pause: " << playerCtx->pause;
}

int MainWindow::blockDownSampling(const QList<QPointF> &srcPointList,
                                  QList<QPointF> &dstPointList,
                                  const int dstBars)
{
    //分块峰值降采样
    //目标柱状图数量 dstBars
    //目标输出点的数量（一个bar对应2个点min/max）
    // const int dstPoints = dstBars * 2;

    dstPointList.clear();

    const int totalBars = srcPointList.size() / 2;
    if (totalBars <= 0 || dstBars <= 0)
        return -1;

    //src太少，直接返回即可
    if(totalBars<=dstBars){
        dstPointList = srcPointList;
        return 0;
    }

    const int blocks = dstBars;//块数 ==  柱状图数
    //分块
    for (int block = 0; block < blocks; ++block) {
        int startBar = block * totalBars / blocks;
        int endBar = (block + 1) * totalBars / blocks;

        if (startBar >= endBar)
            continue;

        qreal maxVal = std::numeric_limits<qreal>::lowest();
        qreal minVal = std::numeric_limits<qreal>::max();
        for (int bar = startBar; bar < endBar; ++bar) {
            const QPointF& p0 = srcPointList[bar * 2];
            const QPointF& p1 = srcPointList[bar * 2 + 1];
            maxVal = qMax(maxVal, qMax(p0.y(), p1.y()));
            minVal = qMin(minVal, qMin(p0.y(), p1.y()));
        }
        //时间取块的中间值
        qreal x = (srcPointList[startBar * 2].x() + srcPointList[(endBar - 1) * 2 + 1].x()) / 2.0;

        dstPointList.append(QPointF(x, maxVal));
        dstPointList.append(QPointF(x, minVal));
    }
    return 0;
}

int MainWindow::intervalDownSampling(const QList<QPointF> &srcPointList,
                                     QList<QPointF> &dstPointList,
                                     const int dstBarInterval)
{
    //等间隔峰值降采样
    //从srcBar里按BarInterval的大小，分成若干块，在从块里峰值降采样得到dstBar
    //目标输出点的数量（一个bar对应2个点min/max）

    dstPointList.clear();

    const int totalBars = srcPointList.size() / 2;
    if (totalBars <= 0 || dstBarInterval <= 0)
        return -1;

    //间隔为1，直接返回即可
    if (dstBarInterval == 1) {
        dstPointList = srcPointList;
        return 0;
    }

    // const int blocks = (totalBars + dstBarInterval - 1) / dstBarInterval;//如果想向上取整，不能简单blocks整除 +1。
    //不按 bar 分块，而是按照dstBarInterval固定间隔一刀一刀地切
    for (int startBar = 0; startBar < totalBars; startBar+=dstBarInterval) {
        // int startBar = block * totalBars / blocks;
        // int endBar = (block + 1) * totalBars / blocks;
        int endBar = qMin(startBar + dstBarInterval, totalBars);

        qreal maxVal = std::numeric_limits<qreal>::lowest();
        qreal minVal = std::numeric_limits<qreal>::max();
        for (int bar = startBar; bar < endBar; ++bar) {
            const QPointF& p0 = srcPointList[bar * 2];
            const QPointF& p1 = srcPointList[bar * 2 + 1];
            maxVal = qMax(maxVal, qMax(p0.y(), p1.y()));
            minVal = qMin(minVal, qMin(p0.y(), p1.y()));
        }
        //时间取块的中间值
        qreal x = (srcPointList[startBar * 2].x() + srcPointList[(endBar - 1) * 2 + 1].x()) / 2.0;
        dstPointList.append(QPointF(x, maxVal));
        dstPointList.append(QPointF(x, minVal));
    }
    return 0;
}

int MainWindow::durBarChartViewDownSampling(const QList<QPointF> &srcPointList,
                                            const int totalCbBars,
                                            QList<QPointF> &dstPointList,
                                            const int width)
{
    //降采样：totalCbBars -> pixelBars
    //目标柱状图数量
    const int pixelBars = width;

    const int barInterval = totalCbBars / pixelBars;
    if (barInterval == 0) {
        dstPointList = srcPointList;
        qCDebug(logDurBar) << "src.size太小了，除数为0，无需降采样";
        return 0;
    }

    return intervalDownSampling(srcPointList,dstPointList,barInterval);
}

void MainWindow::on_btnRewind_clicked()
{
    if (!playerCtx)
        return;

    seekRelative(-10);
}

void MainWindow::on_btnForward_clicked()
{
    if (!playerCtx)
        return;

    seekRelative(+10);
}

void MainWindow::on_horizontalSlider_sliderReleased()
{
    if (!playerCtx)
        return;

    seekAbsolute(ui->horizontalSlider->value());
}
