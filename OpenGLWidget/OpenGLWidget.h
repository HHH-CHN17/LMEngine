#ifndef OPENGLWIDGET_H
#define OPENGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLExtraFunctions>
#include <QOffscreenSurface>
#include <QString>
#include <QDateTime>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "ShaderProgram/GLShaderProgram.h"
#include "Common/DataDefine.h"
#include "Common/Camera/GLCamera.h"
#include "OpenGLWidget/VideoCaptureThread/YUVDraw/GLYuvDraw.h"
#include "OpenGLWidget/VideoCaptureThread/VideoCaptureThread.h"
#include "AVRecorder/AVRecorder.h"
#include "SceneManger/GLSceneManager.h"
#include "RtmpPublisher/RtmpPublisher.h"

class OpenGLWidget: public QOpenGLWidget, public QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget() override;

public:
    // 仅初始化MP4文件，写入MP4头，真正开启视频录制在渲染循环的recordAV()中
    void startRecord(avACT action);
    // 写入MP4尾，
    void stopRecord(avACT action);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void keyPressEvent(QKeyEvent* event) override; //键盘按下事件
    void keyReleaseEvent(QKeyEvent* event) override; //键盘松开事件

    //鼠标移动事件
    void mouseMoveEvent(QMouseEvent* ev) override;
    //鼠标按下事件
    void mousePressEvent(QMouseEvent* ev) override;
    //鼠标释放事件
    void mouseReleaseEvent(QMouseEvent* ev) override;

    void wheelEvent(QWheelEvent* event) override;

private:
    // initializeGL()中使用
    void initOffScreenEnv();
    void initSceneFrameBuffer();
    void initRecordFrameBuffer();
    void initRecordPBOs();
    // resizeGL(int w, int h)中使用
    void reallocSceneFrameBuffer(const int& w, const int& h);
    void adjustViewPort(const int& w, const int& h);
    // 使用双PBO记录视频/推流
    void useRecordPBOs();
    void recordAV(GLubyte* ptr);
    void rtmpPush(GLubyte* ptr);
    void rtspPush(GLubyte* ptr);
    void saveImage(GLubyte* ptr);

private:
    QOpenGLContext* mainCtx_ = nullptr;

    // 用于判断需要推流还是录制视频
    bool needPBO = false;
    bool isRecording_ = false;
    bool isRtmpPush_ = false;
    bool isRtspPush_ = false;

    // ------------------------- 摄像机 -------------------------
    QDateTime lastTime_;
    qint64 deltaTime_ = 0;
    glm::vec3 cameraPos_{ 0.0f, 0.0f, 3.0f };
    QPoint lastPos_;
    bool isPressed_ = false;
    CGLCamera* pCamera_ = nullptr;

    // ------------------------- 渲染数据，这些数据仅用于渲染 -------------------------
    float sceneVertices_[20] =
    {
		-1.0f, -1.0f, -1.0f,    0,  0,
		-1.0f, 1.0f, -1.0f,     0,  1,
		1.0f, -1.0f, -1.0f,     1,  0,
		1.0f, 1.0f, -1.0f,      1,  1 
    };

    GLuint sceneIndices_[6] = {
        0, 1, 2,
        1, 2, 3
    };
    GLuint sceneVAO_ = 0;
    GLuint sceneVBO_ = 0;
    GLuint sceneEBO_ = 0;
    GLuint sceneFBO_ = 0;
    GLuint sceneTexID_ = 0;
    GLuint sceneRBO_ = 0;

    GLShaderProgram* pMainShaderProg_ = nullptr;

    // ------------------------- 录屏数据，这些数据仅用于录屏，需要先将sceneFBO转换成recordFBO，保证分辨率统一 -------------------------
    GLuint recordFBO_;                      // 用于录制的FBO
    GLuint recordTexID_;                    // 录制FBO绑定的纹理
    GLuint recordPBOIds_[2] = { 0, 0 };
    int recordW_ = 1920;                    // 录制的目标宽度
    int recordH_ = 1080;                    // 录制的目标高度

    // ------------------------- 渲染到FBO的texture上的所需数据 -------------------------
    GLSceneManager* pGLSceneManager_ = nullptr;

    // ------------------------- 多线程离屏渲染数据 -------------------------
    QOffscreenSurface* pRenderSurface_ = nullptr;
    // 渲染线程负责：1. 捕获视频帧。2. 将该帧渲染到texid上
    VideoCaptureThread* pRenderThread_ = nullptr;

    GLuint FrameTexID_ = 0;
};

#endif // OPENGLWIDGET_H
