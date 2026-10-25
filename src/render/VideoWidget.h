#pragma once

#include "../core/Types.h"

#include <QOpenGLExtraFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QMutex>

namespace sc {

// YUV420P 三纹理 + 片元着色器做色彩空间转换，避免 CPU 端逐像素转换
class VideoWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override;

    // 线程安全：worker 线程解码出的帧直接丢进来
    void setFrame(Frame frame);
    void clearFrame();

    int frameWidth() const { return width_; }
    int frameHeight() const { return height_; }

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    void uploadTextures();
    void drawQuad();

    mutable QMutex mutex_;
    Frame pending_;
    bool hasPending_ = false;

    QOpenGLShaderProgram program_;
    GLuint texY_ = 0;
    GLuint texU_ = 0;
    GLuint texV_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLsizeiptr vboSize_ = 0;

    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
    bool loggedFirstFrame_ = false;
};

} // namespace sc
