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
    // ����ʼ��MP4�ļ���д��MP4ͷ������������Ƶ¼������Ⱦѭ����recordAV()��
    void startRecord(avACT action);
    // д��MP4β��
    void stopRecord(avACT action);

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
    // initializeGL()��ʹ��
    void initOffScreenEnv();
    void initSceneFrameBuffer();
    void initRecordFrameBuffer();
    void initRecordPBOs();
    // resizeGL(int w, int h)��ʹ��
    void reallocSceneFrameBuffer(const int& w, const int& h);
    void adjustViewPort(const int& w, const int& h);
    // ʹ��˫PBO��¼��Ƶ/����
    void useRecordPBOs();
    void recordAV(GLubyte* ptr);
    void rtmpPush(GLubyte* ptr);
    void rtspPush(GLubyte* ptr);
    void saveImage(GLubyte* ptr);

private:
    QOpenGLContext* mainCtx_ = nullptr;

    // �����ж���Ҫ��������¼����Ƶ
    bool needPBO = false;
    bool isRecording_ = false;
    bool isRtmpPush_ = false;
    bool isRtspPush_ = false;

    // ------------------------- ����� -------------------------
    QDateTime lastTime_;
    qint64 deltaTime_ = 0;
    glm::vec3 cameraPos_{ 0.0f, 0.0f, 3.0f };
    QPoint lastPos_;
    bool isPressed_ = false;
    CGLCamera* pCamera_ = nullptr;

    // ------------------------- ��Ⱦ���ݣ���Щ���ݽ�������Ⱦ -------------------------
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

    // ------------------------- ¼�����ݣ���Щ���ݽ�����¼������Ҫ�Ƚ�sceneFBOת����recordFBO����֤�ֱ���ͳһ -------------------------
    GLuint recordFBO_;                      // ����¼�Ƶ�FBO
    GLuint recordTexID_;                    // ¼��FBO�󶨵�����
    GLuint recordPBOIds_[2] = { 0, 0 };
    int recordW_ = 1920;                    // ¼�Ƶ�Ŀ����
    int recordH_ = 1080;                    // ¼�Ƶ�Ŀ��߶�

    // ------------------------- ��Ⱦ��FBO��texture�ϵ��������� -------------------------
    GLSceneManager* pGLSceneManager_ = nullptr;

    // ------------------------- ���߳�������Ⱦ���� -------------------------
    QOffscreenSurface* pRenderSurface_ = nullptr;
    // ��Ⱦ�̸߳���1. ������Ƶ֡��2. ����֡��Ⱦ��texid��
    VideoCaptureThread* pRenderThread_ = nullptr;

    GLuint FrameTexID_ = 0;
};

#endif // OPENGLWIDGET_H
