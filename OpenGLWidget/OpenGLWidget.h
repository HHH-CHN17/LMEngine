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
#include "SceneManger/GLSceneManager.h"

class OpenGLWidget: public QOpenGLWidget, public QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    OpenGLWidget(QWidget *parent = nullptr);
    ~OpenGLWidget() override;

public:
    void startRecord();
    void stopRecord();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

    void keyPressEvent(QKeyEvent* event) override; //���̰����¼�
    void keyReleaseEvent(QKeyEvent* event) override; //�����ɿ��¼�

    //����ƶ��¼�
    void mouseMoveEvent(QMouseEvent* ev) override;
    //��갴���¼�
    void mousePressEvent(QMouseEvent* ev) override;
    //����ͷ��¼�
    void mouseReleaseEvent(QMouseEvent* ev) override;

    void wheelEvent(QWheelEvent* event) override;

private:
    void initOffScreenEnv();
    void initFrameBuffer();
    void adjustFBO();
    void recordAV();

private:
    QOpenGLContext* mainCtx_ = nullptr;

    bool isRecording = false;

    // ------------------------- ����� -------------------------
    QDateTime lastTime_;
    qint64 deltaTime_ = 0;
    glm::vec3 cameraPos_{ 0.0f, 0.0f, 3.0f };
    QPoint lastPos_;
    bool isPressed_ = false;
    CGLCamera* pCamera_ = nullptr;

    // ------------------------- FBO��Ⱦ�������� -------------------------
    float planeVertices_[20] =
    {
		-1.0f, -1.0f, -1.0f,    0,  0,
		-1.0f, 1.0f, -1.0f,     0,  1,
		1.0f, -1.0f, -1.0f,     1,  0,
		1.0f, 1.0f, -1.0f,      1,  1 
    };

    GLuint planeIndices_[6] = {
        0, 1, 2,
        1, 2, 3
    };
    GLuint planeVAO_ = 0;
    GLuint planeVBO_ = 0;
    GLuint planeEBO_ = 0;
    GLuint planeFBO_ = 0;
    GLuint planeTexID_ = 0;
    GLuint planeRBO_ = 0;

    GLShaderProgram* pMainShaderProg_ = nullptr;

    // ------------------------- ��Ⱦ��FBO��texture�ϵ��������� -------------------------
    GLSceneManager* pGLSceneManager_ = nullptr;

    // ------------------------- ���߳�������Ⱦ���� -------------------------
    QOffscreenSurface* pRenderSurface_ = nullptr;
    // ��Ⱦ�̸߳���1. ������Ƶ֡��2. ����֡��Ⱦ��texid��
    VideoCaptureThread* pRenderThread_ = nullptr;

    GLuint FrameTexID_ = 0;
};

#endif // OPENGLWIDGET_H
