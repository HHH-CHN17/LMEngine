#pragma once

#include <opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <QPoint>
#include <QThread>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QMutex>

#include "ShaderProgram/GLShaderProgram.h"
#include "Common/DataDefine.h"
#include "YUVDraw/GLYuvDraw.h"

class VideoCaptureThread  : public QThread
{
	Q_OBJECT

public:
	VideoCaptureThread(QOpenGLContext* mainCtx, QOffscreenSurface* offScreenSurface, QObject *parent);
	~VideoCaptureThread();

public:
	void initOpenCV(int w, int h);
	void stopCapture();
	void updateWH(const int& w, const int& h);

protected:
	// 捕获了video后，将其渲染到texture上
	void run() override;

signals:
	//void signal_NewYUVFrame(YUVFrame* yuv);
	void signal_NewFacePos(QPoint pos, float scale);
	void signal_NewYuvTexture(unsigned int texID);

private:
	int					width_ = 0;
	int					height_ = 0;

	cv::VideoCapture	videoCapture_{};
	bool				isRunning_ = false;

	QOpenGLContext* pRenderCtx_ = nullptr;
	QOffscreenSurface* pRenderSurface_ = nullptr;

    CYuvDraw* pYuvDraw_ = nullptr;
};
