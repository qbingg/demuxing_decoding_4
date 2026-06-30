#include "mainwindow.h"

#include <QApplication>

/*
 * 目标：去掉demuxing_decoding_3已掌握的注解，引入队列pkt
 */

int main(int argc, char *argv[])
{
    QLoggingCategory::setFilterRules(
        "*.debug=false\n"
        "player.audio.decode.clock.debug=false\n"
        "player.video.decode.sync.debug=false\n"
        "player.audio.chartview.debug=true\n"
        );
    qDebug()<<"测试1，qDebug()：这是过滤调试信息的测试。";
    qCDebug(logAudioClock)<<"测试2，qCDebug(logAudioClock)：这是过滤调试信息的测试。";

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    // 启动时，默认将“测试视频文件路径”添加到标题栏
    QFileInfo testFile("C:\\Users\\lwm\\Desktop\\352x288_25fps.mp4");
    if(testFile.exists())
        w.setWindowTitle(testFile.absoluteFilePath());

    return a.exec();
}
