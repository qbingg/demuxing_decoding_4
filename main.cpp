#include "mainwindow.h"

#include <QApplication>

/*
 * 目标：去掉demuxing_decoding_3已掌握的注解，引入队列pkt
 */

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    // 启动时，默认将“测试视频文件路径”添加到标题栏
    QFileInfo testFile("C:\\Users\\lwm\\Desktop\\352x288_25fps.mp4");
    if(testFile.exists())
        w.setWindowTitle(testFile.absoluteFilePath());

    return a.exec();
}
