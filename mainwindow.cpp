#include "mainwindow.h"
#include "ui_mainwindow.h"

Q_LOGGING_CATEGORY(logPause, "player.pause") // 定义，名称为 ""

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

    qCDebug(logPause)<<"Chart的宽："<<ui->durPcmChartView->width();
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
    connect(m_myAudioDecodeThread,&MyAudioDecodeThread::sendDequeuedPcmBytes,this,[=](QByteArray bytes){
        //涉及除法的就声明为double(qreal)
        const int channels = playerCtx->audio_tgt_channels;//声道数
        const qreal sampleRate = playerCtx->audio_tgt_freq;//采样率（每秒采样次数）44100.0;
        const int bytes_per_sample = av_get_bytes_per_sample(playerCtx->audio_tgt_fmt);//采样点格式 2 Byte = 16 bit
        if(bytes_per_sample == 2){
            //将Byte转为采样点，转为采样帧
            // 公式：Byte = ( sample * 采样点的位深 ) * 声道数
            // 采样点 = Byte / 采样点的位深
            // 采样帧 = 采样点 / 声道数
            qint16 *sampleData = reinterpret_cast<qint16*>(bytes.data());
            int totalSamples = bytes.size() / sizeof(qint16); // 总采样点数，多声道 {LR LR LR...} = 1024 * 2
            const int totalFrames = channels > 0 ?  (totalSamples / channels) : 0;

            /**
             * 进度条波形图，累计的pcm数据*/
            /**
             * "播放时间-10s"波形图，累计的pcm数据*/
            /* 2、分块峰值降采样法（等间隔法取区间最大值 + 最小值）
             * 原理：把采样点分成若干等长的小区间（块），每个区间内计算采样值的最大值和最小值，用这两个点代表整个区间的波形范围。
             * 音频波形显示的行业标准方案，Audacity、Adobe Audition、剪映等专业软件全部采用此方案。
             */
            {
                const int tgtFrames = 1;//从totalFrames降至tgtFrames：1024->64->1
                const int blockInterval = totalFrames / tgtFrames;//分块间隔
                QList<QPointF> points;//==1*声道=2
                points.reserve(tgtFrames * 2); // 每个区间两个点：分别是最大值和最小值
                // 遍历每个block，一共有tgtFrames个分块
                for (int block = 0; block < tgtFrames; ++block) {
                    int startFrame = block * blockInterval;
                    int endFrame = qMin(startFrame + blockInterval, totalFrames);

                    // 遍历块内所有frame，找峰值
                    qint16 maxVal = std::numeric_limits<qint16>::min();//-32768 获取 qint16 类型能表示的最小值。
                    qint16 minVal = std::numeric_limits<qint16>::max();// 32767 获取 qint16 类型能表示的最大值。
                    for (int frameIndex = startFrame; frameIndex < endFrame; ++frameIndex) {
                        const int sampleIndex = frameIndex * channels;
                        qint16 sampleDataL = sampleData[sampleIndex]; // 左声道
                        maxVal = qMax(maxVal, sampleDataL);
                        minVal = qMin(minVal, sampleDataL);
                        if(channels == 2){
                            qint16 sampleDataR = sampleData[sampleIndex + 1]; // 右声道
                            maxVal = qMax(maxVal, sampleDataR);
                            minVal = qMin(minVal, sampleDataR);
                        }
                    }
                    // //时间取block的中间值：startFrame->middleFrame->endFrame
                    // qreal timeSec = ((startFrame + endFrame) / 2.0) / sampleRate;
                    // // waveSeries->append(timeSec, maxVal);
                    // // waveSeries->append(timeSec, minVal);
                    // points.append(QPointF(timeSec, minVal));
                    // points.append(QPointF(timeSec, maxVal));
                    points.append(QPointF(playerCtx->audio_clock, minVal));//改用音频时钟
                    points.append(QPointF(playerCtx->audio_clock, maxVal));//改用音频时钟
                }
                // // 一次性替换所有点，比循环append性能高很多
                // waveSeries2->clear();
                // waveSeries2->replace(points);
                // m_durWaveSeries->append(points);//末尾追加，而不是清空替换
                m_durPoints.append(points);
            }
        }
    },Qt::QueuedConnection);//确保不是子线程操作GUI线程
//     connect(&m_durTimer,&QTimer::timeout,this,[=]{
//         // m_durWaveSeries->replace(m_durPoints);

//         const int pixels = ui->durPcmChartView->width();

//         //对m_durPoints取pixels个min/max点，应该是等间隔还是再次使用分块峰值降采样法？
//         if (m_durPoints.size() <= pixels) {
//             m_durWaveSeries->replace(m_durPoints);
//             return;
//         }
//         qCDebug(logPause)<<"m_durPoints.size() > pixels"<<m_durPoints.size()<<"\t"<<ui->durPcmChartView->width();

//         //m_durTgtPoints//QList<QPointF>
//         // const int tgtBlocks = pixels;
//         // const int blockInterval = m_durPoints.size() / tgtBlocks;//分块间隔
//         // QList<QPointF> points;//==pixels*2
//         // points.reserve(tgtBlocks * 2); // 每个区间两个点：分别是最大值和最小值
//         // // 遍历每个block，一共有tgtFrames个分块
//         // for (int block = 0; block < tgtBlocks; ++block) {
//         //     int startPoint = block * blockInterval;
//         //     int endPoint = qMin(startPoint + blockInterval, m_durPoints.size());
//         //
//         //     // 遍历块内所有frame，找峰值
//         //     qint16 maxVal = std::numeric_limits<qint16>::min();//-32768 获取 qint16 类型能表示的最小值。
//         //     qint16 minVal = std::numeric_limits<qint16>::max();// 32767 获取 qint16 类型能表示的最大值。
//         //     QPointF maxPoint = QPointF(0, maxVal);
//         //     QPointF minPoint = QPointF(0, minVal);
//         //     for (int i = startPoint; i < endPoint; ++i) {
//         //         if (maxPoint.y() < m_durPoints[i].y())
//         //             maxPoint = m_durPoints[i];
//         //         // minPoint = qMin(minPoint,m_durPoints[i])
//         //         if (minPoint.y() > m_durPoints[i].y())
//         //             minPoint = m_durPoints[i];
//         //     }
//         //     points.append(maxPoint);
//         //     points.append(minPoint);
//         // }
//         const int tgtBlocks = pixels;
//         const int total = m_durPoints.size();

//         QList<QPointF> points;
//         points.reserve(tgtBlocks * 2);

//         for (int block = 0; block < tgtBlocks; ++block) {
//             int startPoint = block * total / tgtBlocks;
//             int endPoint = (block + 1) * total / tgtBlocks;

//             if (startPoint >= endPoint)
//                 continue;

//             qreal maxVal = std::numeric_limits<qreal>::lowest();
//             qreal minVal = std::numeric_limits<qreal>::max();

//             for (int i = startPoint; i < endPoint; ++i) {
//                 maxVal = qMax(maxVal, m_durPoints[i].y());
//                 minVal = qMin(minVal, m_durPoints[i].y());
//             }

//             qreal x = (m_durPoints[startPoint].x() + m_durPoints[endPoint - 1].x()) / 2.0;

//             points.append(QPointF(x, minVal));
//             points.append(QPointF(x, maxVal));
//         }

//         m_durWaveSeries->replace(points);
// qCDebug(logPause)<<"m_durWaveSeries->replace(points)";//<<m_durPoints.size()<<"\t"<<ui->durPcmChartView->width();

//     });
//     m_durTimer.start(100);
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
        chartViewDownSampling(m_durPoints,totalCbBars,pList,ui->durPcmChartView->width());

        m_durWaveSeries->replace(pList);

    });
    m_durTimer.start(100);

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

    }else{
        ui->btnPause->setText("暂停");

        playerCtx->pause = false;
    }

    qCDebug(logPause) << "playerCtx->pause: " << playerCtx->pause;
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

        dstPointList.append(QPointF(x, minVal));
        dstPointList.append(QPointF(x, maxVal));
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

int MainWindow::chartViewDownSampling(const QList<QPointF> &srcPointList,
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
        qCDebug(logPause) << "src.size太小了，除数为0，无需降采样";
        return 0;
    }

    intervalDownSampling(srcPointList,dstPointList,barInterval);
}
