#ifndef MYYUV420POPENGLWIDGET_H
#define MYYUV420POPENGLWIDGET_H

/**
 * 基于MyYUV420POpenGLWidget_2
 */

//OpenGL OpenGLWidgets
//
// Qt::OpenGL
// Qt::OpenGLWidgets
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QByteArray>
#include <QImage>
#include <QDebug>
class MyYUV420POpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
private:
    ///CPU 侧图像数据
    // 使用数组[3]是因为gl纹理需要写3次，才绘图1次，用for简化代码
    QByteArray m_yuvPlanes[3];             // y u v
    int m_yuvWidths[3] = {0, 0, 0};        // y u v
    int m_yuvHeights[3] = {0, 0, 0};       // y u v
    int m_frameWidth = 0;
    int m_frameHeight = 0;

    ///GPU 侧 OpenGL 资源
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;
    QOpenGLShaderProgram m_program;

    // 相当于QOpenGLTexture
    GLuint m_textureIds[3] = {0, 0, 0};//m_textureY,m_textureU,m_textureV
    int m_textureWidth = 0;
    int m_textureHeight = 0;

    ///状态记录
    bool m_frameDirty = false;
public:
    explicit MyYUV420POpenGLWidget(QWidget *parent = nullptr)
        : QOpenGLWidget(parent),
        m_vbo(QOpenGLBuffer::VertexBuffer)
    {
    }
    ~MyYUV420POpenGLWidget() override
    {
        //Qt文档：如果控件和OpenGL资源（如上下文）已成功初始化，则返回true。请注意，在控件显示之前，返回值始终为false。
        if (!isValid())
            return;

        makeCurrent();

        // if (m_textureId != 0) {
        //     glDeleteTextures(1, &m_textureId);
        //     m_textureId = 0;
        // }
        if (m_textureIds[0] != 0) {
            glDeleteTextures(3, m_textureIds);
        }
        m_vbo.destroy();
        m_vao.destroy();
        m_program.removeAllShaders();

        doneCurrent();
    }

    void setYuv420pFrame(const QByteArray &yPlane,
                         const QByteArray &uPlane,
                         const QByteArray &vPlane,
                         int width,
                         int height)
    {
        // 1. 基础有效性检查
        if (yPlane.isEmpty() || uPlane.isEmpty() || vPlane.isEmpty()
            || width <= 0 || height <= 0)
        {
            qDebug() << "setYuv420pFrame：参数无效 -"
                     << "width:" << width << "height:" << height
                     << "Y size:" << yPlane.size()
                     << "U size:" << uPlane.size()
                     << "V size:" << vPlane.size();
            return;
        }

        // 2. 校验平面大小是否符合 YUV420P 规范
        //    Y: width * height
        //    U: (width / 2) * (height / 2)  = width * height / 4
        //    V: 同 U
        const int expectedYSize = width * height;
        const int expectedUVSize = ((width + 1) / 2) * ((height + 1) / 2);

        if (yPlane.size() < expectedYSize
            || uPlane.size() < expectedUVSize
            || vPlane.size() < expectedUVSize)
        {
            qDebug() << "setYuv420pFrame：YUV 数据大小与宽高不匹配 -"
                     << "期望 Y:" << expectedYSize << "实际:" << yPlane.size()
                     << "期望 U:" << expectedUVSize << "实际:" << uPlane.size()
                     << "期望 V:" << expectedUVSize << "实际:" << vPlane.size();
            return;
        }

        m_yuvPlanes[0] = yPlane;
        m_yuvPlanes[1] = uPlane;
        m_yuvPlanes[2] = vPlane;

        m_yuvWidths[0] = width;
        m_yuvWidths[1] = ((width + 1) / 2);
        m_yuvWidths[2] = ((width + 1) / 2);

        m_yuvHeights[0] = height;
        m_yuvHeights[1] = ((height + 1) / 2);
        m_yuvHeights[2] = ((height + 1) / 2);

        m_frameWidth = width;
        m_frameHeight = height;

        //标记纹理内容已经过期，下次 paintGL() 时上传到 GPU。
        m_frameDirty = true;
        //请求 Qt 尽快重绘。之后 Qt 会回调 paintGL()。
        update();
    }

protected:
    void initializeGL() override
    {
        // step4.1. 初始化 Qt 封装的 OpenGL 函数指针入口。
        initializeOpenGLFunctions();

        // step4.2. 设置清屏颜色。没有图像时，会显示这个背景色。
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);//QGBA（不透明度）

        // step4.3. 关闭深度测试（2D渲染），后绘制的直接覆盖先绘制的。
        // glDisable(GL_DEPTH_TEST);

        // 初始化VAO/VBO
        m_vao.create();
        m_vao.bind();
        m_vbo.create();
        m_vbo.bind();

        // 顶点布局：位置(x,y,z)[-1,1] + 纹理坐标(u,v)[0,1]，矩形由三角带绘制
        // 创建4个{x, y, z, u, v}顶点集合的空间
        m_vbo.allocate(4 * 5 * sizeof(float)); // 4个顶点集合，每个集合5个float
        // alloc后，初始化VBO
        updateVBO();

        // 创建 shader program
        // 1、编译链接着色器
        {
            // vertex shader 负责处理“矩形顶点在哪里”；
            // fragment shader 负责处理“每个像素显示什么颜色”。
            // 编译链接着色器
            bool vOk = m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texCoord;

out vec2 vTexCoord;

void main(){
    gl_Position = vec4(position, 1.0);
    vTexCoord = texCoord;
}
)");

            bool fOk = m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
#version 330 core
in vec2 vTexCoord;
out vec4 fragColor;

//uniform sampler2D texture1;
uniform sampler2D textureY;
uniform sampler2D textureU;
uniform sampler2D textureV;

void main(){
    //fragColor = texture(texture1, vTexCoord);
    // 分别采样三个平面（UV 共用一个坐标，因为 YUV420 中 UV 分辨率是 Y 的一半，
    // 这里假设已经通过纹理尺寸或外部计算处理好了坐标，直接用 vTexCoord 采样）
    float y = texture(textureY, vTexCoord).r;
    float u = texture(textureU, vTexCoord).r - 0.5;
    float v = texture(textureV, vTexCoord).r - 0.5;

    // ITU-R BT.601 全范围转换矩阵（适用于大多数视频）
    float r = y + 1.402   * v;
    float g = y - 0.34414 * u - 0.71414 * v;
    float b = y + 1.772   * u;

    fragColor = vec4(r, g, b, 1.0);
}
)");

            if (!vOk || !fOk) {
                qDebug() << "着色器编译失败:" << m_program.log();
                return;
            }
            if (!m_program.link()) {
                qDebug() << "着色器链接失败:" << m_program.log();
                return;
            }
            m_program.bind();
        }
        // 2、配置顶点属性
        m_vbo.bind(); // updateVBO release 过，这里重新绑定vbo，才可以配置属性
        const int stride = 5 * sizeof(float);
        // 位置属性
        m_program.setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);
        m_program.enableAttributeArray(0);
        // 纹理坐标属性
        m_program.setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 2, stride);
        m_program.enableAttributeArray(1);

        // //为GLuint m_textureId创建纹理，相当于：m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        // {
        //     glGenTextures(1, &m_textureId);//m_texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        //     glBindTexture(GL_TEXTURE_2D, m_textureId);// m_texture->bind();
        //
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);//m_texture->setMinificationFilter(QOpenGLTexture::Linear);
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);//m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);//m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        //     glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        //
        //     glBindTexture(GL_TEXTURE_2D, 0);//m_texture->release();
        // }
        glGenTextures(3, m_textureIds);
        for (GLuint textureId : m_textureIds) {
            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        /** 解绑资源 **
         * 为什么每个 OpenGL 对象都要先绑定再解绑？
         * ────────────────────────────────
         * OpenGL 是一个状态机，操作（如 glBufferData、glVertexAttribPointer）
         * 总是作用于“当前绑定”的对象，而不是直接通过句柄操作。
         *
         * 初始化阶段我们配置 VAO、VBO、Shader 时，必须先把它们绑定到
         * 对应的目标（如 GL_ARRAY_BUFFER），配置完成后再解绑（绑定到 0），
         * 目的是：
         *   1) 避免这些对象的绑定状态泄露，影响后续代码；
         *   2) 防止后续代码不小心修改这些资源；
         *   3) 保持 OpenGL 上下文干净，避免“幽灵状态”导致的隐藏 Bug。
         *
         * 这不同于传统的 new / delete —— OpenGL 的资源生命周期由
         * create() / destroy() 控制，而 bind / release 只是“激活/停用”
         * 该资源，并不分配或释放 GPU 内存。
         */
        m_vao.release();
        m_vbo.release();
        m_program.release();
    }
    void resizeGL(int w, int h) override
    {
        glViewport(0, 0, w, h);
    }
    void paintGL() override
    {
        // 清空当前 framebuffer，避免上一帧残留。
        glClear(GL_COLOR_BUFFER_BIT);

        // 检测图片更新，在渲染上下文内安全更新纹理
        if (m_frameDirty && !m_yuvPlanes[0].isEmpty()) {
            updateTexture();
            m_frameDirty = false;
        }

        // 纹理未初始化时不绘制
        if (m_textureIds[0] == 0 || m_textureWidth <= 0 || m_textureHeight <= 0)
            return;

        m_vao.bind();
        m_program.bind();

        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, m_textureIds[i]);
        }
        // m_program.setUniformValue("texture1", 0);
        m_program.setUniformValue("textureY", 0);
        m_program.setUniformValue("textureU", 1);
        m_program.setUniformValue("textureV", 2);

        //保持宽高比
        // 计算保持宽高比缩放后的尺寸，并居中绘制
        // 等于pixmap.scaled()
        updateVBO();
        // 等于painter.drawPixmap(x, y, scaled);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // glBindTexture(GL_TEXTURE_2D, 0);// m_texture->release();
        for (int i = 0; i < 3; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        m_program.release();
        m_vao.release();
    }
private:
    // 保持宽高比（图片始终完整显示在 widget 内；多出来的区域留成黑边。）
    // 这里不再像 QPainter 那样先 scaled() 出一张新图，而是改变矩形顶点坐标(x,y,z)。
    void updateVBO()
    {
        if (!m_vbo.isCreated())
            return;

        //顶点布局：位置(x,y,z)[-1,1] + 纹理坐标(u,v)[0,1]
        float halfWidth = 1.0f;
        float halfHeight = 1.0f;

        if (!m_yuvPlanes[0].isEmpty() && width() > 0 && height() > 0) {
            const float widgetAspect = static_cast<float>(width()) / static_cast<float>(height());
            const float imageAspect = static_cast<float>(m_frameWidth) / static_cast<float>(m_frameHeight);

            if (imageAspect > widgetAspect) {
                // 图片比 widget 更“宽”：宽度铺满，高度缩小，上下留黑边。
                halfHeight = widgetAspect / imageAspect;
            } else {
                // 图片比 widget 更“高”：高度铺满，宽度缩小，左右留黑边。
                halfWidth = imageAspect / widgetAspect;
            }
        }

        /** 纹理垂直坐标反转 **
         * 四个顶点按 GL_TRIANGLE_STRIP 顺序排列：
         *   左下 → 右下 → 左上 → 右上
         *
         * 每个顶点包含 5 个浮点数：
         *   [0-2] : 位置 (x, y, z)  范围 [-1, 1] (NDC 归一化设备坐标)
         *   [3-4] : 纹理坐标 (u, v) 范围 [0, 1]
         *
         * ── 位置坐标 (x, y) ──
         *   x = -1 表示屏幕最左边，+1 表示最右边
         *   y = -1 表示屏幕最下边，+1 表示最上边
         *
         * ── 纹理坐标 (u, v) ──
         *   u = 0 表示纹理左边，1 表示纹理右边
         *   v = 0 表示纹理底部，1 表示纹理顶部 (这是 OpenGL 的约定)
         *
         * ── 为什么这里 v 是“上下颠倒”的？ ──
         *   QImage 存储图像时，第 0 行位于图片**顶部**；
         *   而 OpenGL 纹理坐标系统中，v = 0 对应纹理的**底部**。
         *   为了让图像显示不颠倒，需要把：
         *     - 屏幕下方的顶点 (y = -1) 映射到纹理顶部 v = 1
         *     - 屏幕上方的顶点 (y = +1) 映射到纹理底部 v = 0
         *   也就是将纹理 V 坐标整体翻转。
         *
         *   具体对应关系：
         *     左下顶点 (屏幕下方) → (u=0, v=1)  ← 纹理左上角
         *     右下顶点            → (u=1, v=1)  ← 纹理右上角
         *     左上顶点 (屏幕上方) → (u=0, v=0)  ← 纹理左下角
         *     右上顶点            → (u=1, v=0)  ← 纹理右下角
         *
         *   这样绘制后，图像就以正确的方向显示在屏幕上。
         */
        const float vertices[] = {
            -halfWidth, -halfHeight, 0.0f, 0.0f, 1.0f,//左下顶点，纹理左上角
            halfWidth, -halfHeight, 0.0f, 1.0f, 1.0f,//右下顶点，纹理右上角
            -halfWidth,  halfHeight, 0.0f, 0.0f, 0.0f,//左上顶点，纹理左下角
            halfWidth,  halfHeight, 0.0f, 1.0f, 0.0f//右上顶点 ，纹理右下角
        };

        m_vbo.bind();
        m_vbo.write(0, vertices, static_cast<int>(sizeof(vertices)));
        m_vbo.release();
    }
    // 内部纹理更新：仅在paintGL()中调用，保证OpenGL上下文有效
    void updateTexture()
    {
        if (m_yuvPlanes[0].isEmpty())
            return;

        // glBindTexture(GL_TEXTURE_2D, m_textureId);// m_texture->bind();
        // glGenTextures(3, m_textureIds);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);// 设置对齐为1字节（默认4 Byte）

        const bool recreateTexture =
            m_textureWidth != m_frameWidth ||
            m_textureHeight != m_frameHeight;
        for (int i = 0; i < 3; ++i) {
            glBindTexture(GL_TEXTURE_2D, m_textureIds[i]);
            {
                //尺寸变化时。重新new纹理
                if (recreateTexture) {
                    m_textureWidth = m_frameWidth;
                    m_textureHeight = m_frameHeight;

                    //glTexImage2D —— 分配纹理存储并上传数据，典型场景：
                    //首次创建纹理。
                    //纹理尺寸发生变化，需要重新分配内存。
                    glTexImage2D(GL_TEXTURE_2D,
                                 0,
                                 GL_R8,
                                 m_yuvWidths[i],
                                 m_yuvHeights[i],
                                 0,
                                 GL_RED,
                                 GL_UNSIGNED_BYTE,
                                 m_yuvPlanes[i].constData());
                }else{
                    //glTexSubImage2D —— 更新纹理的部分或全部数据，典型场景：
                    //视频帧连续刷新，纹理尺寸不变，仅替换内容。
                    glTexSubImage2D(GL_TEXTURE_2D,
                                    0,
                                    0,
                                    0,
                                    m_yuvWidths[i],
                                    m_yuvHeights[i],
                                    GL_RED,
                                    GL_UNSIGNED_BYTE,
                                    m_yuvPlanes[i].constData());
                }
            }
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);// 恢复默认（可选，但建议）

        glBindTexture(GL_TEXTURE_2D, 0);//m_texture->release();
    }
};



#endif // MYYUV420POPENGLWIDGET_H
