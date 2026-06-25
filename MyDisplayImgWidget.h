#ifndef MYDISPLAYIMGWIDGET_H
#define MYDISPLAYIMGWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QPixmap>
#include <QFileInfo>
#include <QMouseEvent>
#include <QApplication>
class MyDisplayImgWidget : public QWidget
{
    Q_OBJECT
private:
    QPixmap m_pixmap;//仅缩放用

    QFileInfo m_fileInfo;

    QPoint m_dragStartPos;
public:
    explicit MyDisplayImgWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
    }
    ~MyDisplayImgWidget(){}
    QFileInfo fileInfo() const
    {
        return m_fileInfo;
    }
    void setFileInfo(const QFileInfo &newFileInfo)
    {
        m_fileInfo = newFileInfo;
        m_pixmap = QPixmap(m_fileInfo.absoluteFilePath());
        //更新图片后，需要手动立即更新update显示图片，否则有可还显示旧的图片
        update();
    }

    void setPixmap(const QPixmap &newPixmap)
    {
        m_pixmap = newPixmap;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);

        //画边框，表示Widget范围
        painter.setPen(QPen(Qt::black,5));
        painter.drawRect(rect());

        //缩放并显示图片
        if(!m_pixmap.isNull()){
            // 计算保持宽高比缩放后的尺寸，并居中绘制
            QRect targetRect = rect();
            QPixmap scaled = m_pixmap.scaled(targetRect.size(),
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation);
            int x = targetRect.x() + (targetRect.width() - scaled.width()) / 2;
            int y = targetRect.y() + (targetRect.height() - scaled.height()) / 2;
            painter.drawPixmap(x, y, scaled);
        }
    }
    //按下左键时，记录位置
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            m_dragStartPos = event->pos();
    }
    void mouseMoveEvent(QMouseEvent *event) override
    {
        //button() 用于区分是哪个按钮触发了本次事件（主要对按下/释放事件有意义）。
        //buttons() 用于查询事件发生时所有按钮的实时按下状态（适用于所有鼠标事件）。
        //由于移动事件中 button() 总是 Qt::NoButton，该条件永远为真，直接 return，导致后续的拖动处理永远不会执行。
        // if (event->button() != Qt::LeftButton)
        //     return;
        if (!(event->buttons() & Qt::LeftButton))
            return;

        // 防止微小移动就触发拖拽
        if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
            return;

        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;

        // 设置文件 URL（支持多个文件，这里只放一个）
        QList<QUrl> urls;
        urls.append(QUrl::fromLocalFile(m_fileInfo.absoluteFilePath()));
        mimeData->setUrls(urls);
        drag->setMimeData(mimeData);

        // 可选：设置拖拽时显示的缩略图
        QPixmap pixmap = m_pixmap;
        if (!pixmap.isNull()) {
            drag->setPixmap(pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            // 设置热点（鼠标在缩略图上的位置），默认是左上角
            drag->setHotSpot(QPoint(50, 25));
        }

        // QDrag文档：
        // On Windows, the Qt event loop is blocked during the operation.
        // However, QDrag::exec() on Windows causes processEvents() to be called frequently to keep the GUI responsive.
        // 在Windows上，Qt事件循环在操作过程中被阻止。
        // 但是，Windows上的QDrag::exec（）会导致频繁调用processEvents（）以保持GUI的响应。
        drag->exec(Qt::CopyAction);
    }
};








#endif // MYDISPLAYIMGWIDGET_H
