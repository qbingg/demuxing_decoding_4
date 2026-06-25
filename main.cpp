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
    return a.exec();
}
