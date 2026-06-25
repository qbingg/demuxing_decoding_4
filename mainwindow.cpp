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




    m_demuxThread->start();
    m_myVideoDecodeThread->start();
}

