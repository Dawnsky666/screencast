#include "VideoWidget.h"

#include <QDebug>

#include <algorithm>

namespace sc {

namespace {

const char *kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char *kFragmentShader = R"(#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;
void main() {
    float y = texture(texY, vUV).r;
    float u = texture(texU, vUV).r - 0.5;
    float v = texture(texV, vUV).r - 0.5;

    // BT.709 limited range -> RGB
    float yy = (y - 16.0 / 255.0) * (255.0 / 219.0);
    float uu = u * (255.0 / 224.0);
    float vv = v * (255.0 / 224.0);

    vec3 rgb;
    rgb.r = yy + 1.5748 * vv;
    rgb.g = yy - 0.1873 * uu - 0.4681 * vv;
    rgb.b = yy + 1.8556 * uu;
    fragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";

} // namespace

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(320, 180);
    setAutoFillBackground(false);
}

VideoWidget::~VideoWidget()
{
    makeCurrent();
    if (texY_)
        glDeleteTextures(1, &texY_);
    if (texU_)
        glDeleteTextures(1, &texU_);
    if (texV_)
        glDeleteTextures(1, &texV_);
    if (vbo_)
        glDeleteBuffers(1, &vbo_);
    if (vao_)
        glDeleteVertexArrays(1, &vao_);
    doneCurrent();
}

void VideoWidget::setFrame(Frame frame)
{
    QMutexLocker locker(&mutex_);
    pending_ = std::move(frame);
    hasPending_ = true;
    update();
}

void VideoWidget::clearFrame()
{
    QMutexLocker locker(&mutex_);
    hasPending_ = false;
    pending_.reset();
    update();
}

void VideoWidget::initializeGL()
{
    initializeOpenGLFunctions();

    qInfo() << "GL_VERSION:" << reinterpret_cast<const char *>(glGetString(GL_VERSION))
            << "GL_RENDERER:" << reinterpret_cast<const char *>(glGetString(GL_RENDERER));

    if (!program_.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)) {
        qWarning() << "vertex shader:" << program_.log();
        return;
    }
    if (!program_.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader)) {
        qWarning() << "fragment shader:" << program_.log();
        return;
    }
    if (!program_.link()) {
        qWarning() << "link:" << program_.log();
        return;
    }

    // 全屏四边形：两个三角形，带 UV
    const GLfloat verts[] = {
        -1.f, -1.f, 0.f, 1.f,
         1.f, -1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 0.f,
    };
    vboSize_ = sizeof(verts);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vboSize_, verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          reinterpret_cast<void *>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          reinterpret_cast<void *>(2 * sizeof(GLfloat)));

    glGenTextures(1, &texY_);
    glGenTextures(1, &texU_);
    glGenTextures(1, &texV_);
    for (GLuint tex : { texY_, texU_, texV_ }) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glDisable(GL_DEPTH_TEST);
    initialized_ = true;
}

void VideoWidget::resizeGL(int w, int h)
{
    // Qt 会在每次 paintGL 前把 viewport 设为整个 FBO，这里不要用逻辑像素再设一次。
    // 之前这里 glViewport(0,0,w,h) 在 150% 缩放下只覆盖左下角 2/3。
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void VideoWidget::uploadTextures()
{
    if (!hasPending_)
        return;

    Frame frame;
    {
        QMutexLocker locker(&mutex_);
        frame = std::move(pending_);
        hasPending_ = false;
    }
    if (frame.empty())
        return;

    width_ = frame.width;
    height_ = frame.height;
    if (!loggedFirstFrame_) {
        qInfo() << "first frame:" << width_ << "x" << height_ << "bytes"
                << static_cast<int>(frame.data.size());
        loggedFirstFrame_ = true;
    }

    const int ySize = width_ * height_;
    const int cW = width_ / 2;
    const int cH = height_ / 2;
    const int cSize = cW * cH;
    if (static_cast<int>(frame.data.size()) < ySize + cSize * 2)
        return;

    const uint8_t *y = frame.data.data();
    const uint8_t *u = y + ySize;
    const uint8_t *v = u + cSize;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, texY_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width_, height_, 0, GL_RED, GL_UNSIGNED_BYTE, y);

    glBindTexture(GL_TEXTURE_2D, texU_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cW, cH, 0, GL_RED, GL_UNSIGNED_BYTE, u);

    glBindTexture(GL_TEXTURE_2D, texV_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cW, cH, 0, GL_RED, GL_UNSIGNED_BYTE, v);
}

void VideoWidget::drawQuad()
{
    program_.bind();
    program_.setUniformValue("texY", 0);
    program_.setUniformValue("texU", 1);
    program_.setUniformValue("texV", 2);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texY_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texU_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texV_);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    program_.release();
}

void VideoWidget::paintGL()
{
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (!initialized_)
        return;

    uploadTextures();

    if (width_ <= 0 || height_ <= 0)
        return;

    // 保持宽高比：把 viewport 限制在中间，两侧留黑边。
    // 必须用设备像素：QOpenGLWidget 的 FBO 是 width()*DPR，用逻辑像素会只画左下角。
    const qreal dpr = devicePixelRatioF();
    const int pixelWidth = std::max(1, qRound(this->width() * dpr));
    const int pixelHeight = std::max(1, qRound(this->height() * dpr));

    const float widgetAspect = float(pixelWidth) / float(pixelHeight);
    const float videoAspect = float(width_) / float(height_);

    int vx = 0, vy = 0, vw = pixelWidth, vh = pixelHeight;
    if (widgetAspect > videoAspect) {
        vw = static_cast<int>(pixelHeight * videoAspect);
        vx = (pixelWidth - vw) / 2;
    } else {
        vh = static_cast<int>(pixelWidth / videoAspect);
        vy = (pixelHeight - vh) / 2;
    }

    glViewport(vx, vy, vw, vh);
    drawQuad();
}

} // namespace sc
