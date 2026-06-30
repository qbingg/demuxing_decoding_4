#include "mainwindow.h"
#include "ui_mainwindow.h"

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
    QChart *chart = new QChart();
    chart->setTitle("");
    chart->legend()->hide();
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->layout()->setContentsMargins(0, 0, 0, 0);
    chart->setBackgroundRoundness(0);
    chart->setAnimationOptions(QChart::NoAnimation); // 静态图关闭动画

    QLineSeries *waveSeries = new QLineSeries();
    waveSeries->setName("音频波形");
    waveSeries->setPen(QPen(QColor(0, 180, 255), 1)); // 浅蓝色线条
    chart->addSeries(waveSeries);

    // 配置X/Y轴（X轴总时间，Y轴16位PCM范围）
    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("时间 (s)");
    // axisX->setRange(0, totalSamples / sampleRate); // 总时长
    axisX->setRange(0, 1024 / static_cast<double>(playerCtx->audio_tgt_freq)); // 总时长

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("采样值");
    axisY->setRange(-32768, 32767); // 16位有符号整数范围

    // 绑定坐标轴
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);
    waveSeries->attachAxis(axisX);
    waveSeries->attachAxis(axisY);

    // 显示到UI的QChartView控件（对象名：chartView）
    ui->pcmChartView->setChart(chart);
    ui->pcmChartView->setRenderHint(QPainter::Antialiasing); // 抗锯齿

    connect(m_myAudioDecodeThread,&MyAudioDecodeThread::sendDequeuedPcmBytes,this,[=](QByteArray bytes){

        //将byte转为采样点：公式：Byte = ( sample * 采样点的位深 ) * 声道数
        double channels = static_cast<double>(playerCtx->audio_tgt_channels);
        double bytes_per_sample = av_get_bytes_per_sample(playerCtx->audio_tgt_fmt);

        if(bytes_per_sample == 2){
            int totalSamples = bytes.size() / sizeof(qint16); // 总采样点数
            qint16 *sampleData = reinterpret_cast<qint16*>(bytes.data());

            waveSeries->clear();

            // 3. 填充数据：X轴=时间(秒)，Y轴=16位PCM采样值
            const qreal sampleRate = playerCtx->audio_tgt_freq;//44100.0;
            for (int i = 0; i < totalSamples; i+=100) {
                qreal timeSec = i / sampleRate;        // X轴：时间
                qreal pcmValue = sampleData[i];       // Y轴：采样值
                waveSeries->append(timeSec, pcmValue);
            }

            qCDebug(logVideoSync) <<"x轴最大值(1024个采样点的时长)："<<(1024 / static_cast<double>(playerCtx->audio_tgt_freq))<<"\t"
                                  <<"最大采样点时间是："<<  (static_cast<double>(totalSamples) / static_cast<double>(sampleRate));
        }
    },Qt::QueuedConnection);//确保不是子线程操作GUI线程

    m_demuxThread->start();
    m_myVideoDecodeThread->start();
    m_myAudioDecodeThread->start();
}

