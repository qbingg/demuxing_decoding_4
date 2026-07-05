#include "mainwindow.h"
#include "ui_mainwindow.h"

Q_LOGGING_CATEGORY(logAudioChartView, "player.audio.chartview") // 定义，名称为 ""
Q_LOGGING_CATEGORY(logAudioChartView2, "player.audio.chartview2") // 定义，名称为 ""

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setAcceptDrops(true);// 开启对整个窗口的拖放操作的支持
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initPcmChartView(QLineSeries *waveSeries, QValueAxis *axisX, QValueAxis *axisY, QChartView *pcmChartView)
{
    if(!waveSeries || !axisX || !axisY|| !pcmChartView)
        return;

    QChart *chart = new QChart();
    chart->addSeries(waveSeries);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    //波形数据使用这两个坐标轴映射
    waveSeries->attachAxis(axisX);
    waveSeries->attachAxis(axisY);

    // 显示到UI的QChartView控件（对象名：chartView）
    pcmChartView->setChart(chart);
    pcmChartView->setRenderHint(QPainter::Antialiasing); // 抗锯齿

    /*为pcm图表显示进行布局优化*/
    chart->setTitle("");//去掉标题
    // chart->legend()->hide();//隐藏图表用于解释颜色和系列名称的图例框
    chart->layout()->setContentsMargins(0, 0, 0, 0);//去掉外层layout的margin间隔
    chart->setMargins(QMargins(0, 0, 0, 0));//去掉chart内层的margin间隔
    chart->setBackgroundRoundness(0);//去掉圆角（Qt文档：此属性表示图表背景四角处圆角的直径。）
    chart->setAnimationOptions(QChart::NoAnimation); // 静态图关闭动画
    // 去掉坐标轴标题
    axisX->setTitleVisible(false);
    axisY->setTitleVisible(false);
    // 去掉坐标轴刻度
    axisX->setLabelsVisible(false);
    axisY->setLabelsVisible(false);
    // // 去掉坐标轴网格
    // axisX->setGridLineVisible(false);
    // axisY->setGridLineVisible(false);
}

void MainWindow::initBarChartView(QBarSeries *barSeries, QBarCategoryAxis *axisX, QValueAxis *axisY, QChartView *barChartView)
{
    if(!barSeries || !axisX || !axisY|| !barChartView)
        return;

    QChart *chart = new QChart();
    chart->addSeries(barSeries);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    //波形数据使用这两个坐标轴映射
    barSeries->attachAxis(axisX);
    barSeries->attachAxis(axisY);

    // 显示到UI的QChartView控件（对象名：chartView）
    barChartView->setChart(chart);
    barChartView->setRenderHint(QPainter::Antialiasing); // 抗锯齿

    /*为bar图表显示进行布局优化*/
    chart->setTitle("");//去掉标题
    // chart->legend()->hide();//隐藏图表用于解释颜色和系列名称的图例框
    chart->layout()->setContentsMargins(0, 0, 0, 0);//去掉外层layout的margin间隔
    chart->setMargins(QMargins(0, 0, 0, 0));//去掉chart内层的margin间隔
    chart->setBackgroundRoundness(0);//去掉圆角（Qt文档：此属性表示图表背景四角处圆角的直径。）
    chart->setAnimationOptions(QChart::NoAnimation); // 静态图关闭动画
    // 去掉坐标轴标题
    axisX->setTitleVisible(false);
    axisY->setTitleVisible(false);
    // // 去掉坐标轴刻度
    // axisX->setLabelsVisible(false);
    // axisY->setLabelsVisible(false);
    // // 去掉坐标轴网格
    // axisX->setGridLineVisible(false);
    // axisY->setGridLineVisible(false);
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
    connect(m_myVideoDecodeThread,&MyVideoDecodeThread::sendCurrentFrame,this,[=](QImage qimg){
        ui->widget->setPixmap(QPixmap::fromImage(qimg));
    });

    m_myAudioDecodeThread = new MyAudioDecodeThread;
    m_myAudioDecodeThread->setPlayerCtx(playerCtx);
    // 初始化pcmChartView
    /*基于出队dequeue单次的pcm数据*/
    //1、等间隔法（分块取队首）
    QLineSeries *waveSeries = new QLineSeries();
    waveSeries->setName("等间隔法");
    waveSeries->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("时间 (s)");
    axisX->setRange(0, 1024 / static_cast<double>(playerCtx->audio_tgt_freq)); // 总时长
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("采样值");
    axisY->setRange(-32768, 32767); // 16位有符号整数范围
    initPcmChartView(waveSeries,axisX,axisY,ui->pcmChartView);
    //2、分块峰值降采样法（等间隔法取区间最大值 + 最小值）
    QLineSeries *waveSeries2 = new QLineSeries();
    waveSeries2->setName("分块峰值降采样法");
    waveSeries2->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    QValueAxis *axisX2 = new QValueAxis();
    axisX2->setTitleText("时间 (s)");
    axisX2->setRange(0, 1024 / static_cast<double>(playerCtx->audio_tgt_freq)); // 总时长
    QValueAxis *axisY2 = new QValueAxis();
    axisY2->setTitleText("采样值");
    axisY2->setRange(-32768, 32767); // 16位有符号整数范围
    initPcmChartView(waveSeries2,axisX2,axisY2,ui->pcmChartView2);
    /*进度条波形图，累计的pcm数据*/
    /*"播放时间-10s"波形图，累计的pcm数据*/
    //2、分块峰值降采样法（等间隔法取区间最大值 + 最小值）
    QLineSeries *durWaveSeries6 = new QLineSeries();
    durWaveSeries6->setName("音频波形\"播放时间-10s\"：分块峰值降采样法（等间隔法取区间最大值 + 最小值）");
    durWaveSeries6->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    QValueAxis *durAxisX6 = new QValueAxis();
    durAxisX6->setTitleText("时间 (s)");
    durAxisX6->setRange(0, (playerCtx->audio_stream->duration * av_q2d(playerCtx->audio_stream->time_base))); // 时长 0~duration(音频流)，注意要考虑时间基
    QValueAxis *durAxisY6 = new QValueAxis();
    durAxisY6->setTitleText("采样值");
    durAxisY6->setRange(-32768, 32767); // 16位有符号整数范围
    initPcmChartView(durWaveSeries6,durAxisX6,durAxisY6,ui->durPcmChartView6);
    durAxisX6->setLabelsVisible(true);//单独开启durAxisX的刻度
    /*dBFS图*/
    // 创建柱状序列
    QBarSeries *barSeries = new QBarSeries();
    barSeries->setName("dBFS图");
    // X 轴使用类别轴
    QBarCategoryAxis *barAxisX = new QBarCategoryAxis();
    barAxisX->append("L");
    barAxisX->append("R");
    // Y 轴不变
    QValueAxis *barAxisY = new QValueAxis();
    barAxisY->setTitleText("dBFS");
    barAxisY->setRange(-100, 0);
    initBarChartView(barSeries,barAxisX,barAxisY,ui->dBFSChartView);
    // 只用一个 QBarSet，包含左右声道两个值
    QBarSet *set = new QBarSet("声道");
    *set << -12.5 << -15.8;   // 第一个是 L，第二个是 R
    barSeries->append(set);

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
             * 基于出队dequeue单次的pcm数据*/
            /* 1、等间隔法（分块取队首）*/
            {
                constexpr int INTERVAL = 16;//0、16、32、48....1024，特例：间隔=1
                QList<QPointF> points;//==(1024/16)*声道=128
                points.reserve((totalFrames/INTERVAL) * 2); // 每个间隔区间两个点：分别是左声道、右声道
                for (int frameIndex = 0; frameIndex < totalFrames; frameIndex+=INTERVAL) {
                    const int sampleIndex = frameIndex * channels;
                    qreal timeSec = frameIndex / sampleRate;
                    qint16 sampleDataL = sampleData[sampleIndex];
                    // waveSeries->append(timeSec, sampleDataL);
                    points.append(QPointF(timeSec, sampleDataL));
                    if(channels > 2){
                        qint16 sampleDataR = sampleData[sampleIndex + 1];
                        // waveSeries->append(timeSec, sampleDataR);
                        points.append(QPointF(timeSec, sampleDataR));
                    }
                }
                waveSeries->clear();
                waveSeries->replace(points);
            }
            /* 2、分块峰值降采样法（等间隔法取区间最大值 + 最小值）
             * 原理：把采样点分成若干等长的小区间（块），每个区间内计算采样值的最大值和最小值，用这两个点代表整个区间的波形范围。
             * 音频波形显示的行业标准方案，Audacity、Adobe Audition、剪映等专业软件全部采用此方案。
             */
            {
                const int tgtFrames = 64;//从totalFrames降至tgtFrames：1024->64
                const int blockInterval = totalFrames / tgtFrames;//分块间隔
                QList<QPointF> points;//==64*声道=128
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
                        if(channels > 2){
                            qint16 sampleDataR = sampleData[sampleIndex + 1]; // 右声道
                            maxVal = qMax(maxVal, sampleDataR);
                            minVal = qMin(minVal, sampleDataR);
                        }
                    }
                    //时间取block的中间值：startFrame->middleFrame->endFrame
                    qreal timeSec = ((startFrame + endFrame) / 2.0) / sampleRate;
                    // waveSeries->append(timeSec, maxVal);
                    // waveSeries->append(timeSec, minVal);
                    points.append(QPointF(timeSec, minVal));
                    points.append(QPointF(timeSec, maxVal));
                }
                // 一次性替换所有点，比循环append性能高很多
                waveSeries2->clear();
                waveSeries2->replace(points);
            }

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
                        if(channels > 2){
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
                durWaveSeries6->append(points);//末尾追加，而不是清空替换
                durAxisX6->setRange(playerCtx->audio_clock < 10 ? 0 : (playerCtx->audio_clock - 10),
                                    playerCtx->audio_clock);
            }
            /*dBFS图*/
            /* 2、分块峰值降采样法（等间隔法取区间最大值 + 最小值）
             * 原理：把采样点分成若干等长的小区间（块），每个区间内计算采样值的最大值和最小值，用这两个点代表整个区间的波形范围。
             * 音频波形显示的行业标准方案，Audacity、Adobe Audition、剪映等专业软件全部采用此方案。
             */
            {
                const int tgtFrames = 1;//从totalFrames降至tgtFrames：1024->64 //==1*声道=2
                const int blockInterval = totalFrames / tgtFrames;//分块间隔
                qint16 maxValL = std::numeric_limits<qint16>::min();//-32768 获取 qint16 类型能表示的最小值。
                qint16 maxValR = std::numeric_limits<qint16>::min();//-32768 获取 qint16 类型能表示的最小值。
                // 遍历每个block，一共有tgtFrames个分块
                for (int block = 0; block < tgtFrames; ++block) {
                    int startFrame = block * blockInterval;
                    int endFrame = qMin(startFrame + blockInterval, totalFrames);

                    // 遍历块内所有frame，找峰值
                    for (int frameIndex = startFrame; frameIndex < endFrame; ++frameIndex) {
                        const int sampleIndex = frameIndex * channels;
                        qint16 sampleDataL = sampleData[sampleIndex]; // 左声道
                        maxValL = qMax(maxValL, sampleDataL);
                        if(channels > 2){
                            qint16 sampleDataR = sampleData[sampleIndex + 1]; // 右声道
                            qCDebug(logAudioChartView2)<<"sampleData[sampleIndex + 1]:"<<sampleData[sampleIndex + 1];
                            maxValR = qMax(maxValR, sampleDataR);
                        }
                    }
                }
                //求峰值分贝（dBFS）
                QBarSet *set = new QBarSet("峰值分贝（dBFS）");
                // 20 * log10(峰值 / 最大可能值)
                *set << (20.0f * log10f((float)maxValL / 32768.0f))
                     << (20.0f * log10f((float)maxValR / 32768.0f));
                barSeries->clear();
                barSeries->append(set);

                qCDebug(logAudioChartView2)<<"maxValL:"<<maxValL
                                            <<"\t maxValR:"<<maxValR
                                            <<"\t dbL ="<<(20.0f * log10f((float)maxValL / 32768.0f))
                                            <<"\t dbR ="<<(20.0f * log10f((float)maxValR / 32768.0f));
            }

        }
    },Qt::QueuedConnection);//确保不是子线程操作GUI线程

    m_demuxThread->start();
    m_myVideoDecodeThread->start();
    m_myAudioDecodeThread->start();
}

