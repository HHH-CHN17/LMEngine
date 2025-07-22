#include "VideoCaptureThread.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <QDebug>
#include <QApplication>
#include "Facelandmark.h"

VideoCaptureThread::VideoCaptureThread(QOpenGLContext* mainCtx, QOffscreenSurface* offScreenSurface, QObject *parent)
	: QThread(parent)
{
	isRunning_ = false;

	if (!pRenderCtx_)
	{
		pRenderCtx_ = new QOpenGLContext();
		pRenderCtx_->setFormat(mainCtx->format());
		pRenderCtx_->setShareContext(mainCtx);
		pRenderCtx_->create();
		pRenderCtx_->moveToThread(this);
	}

	// 离屏表面须在GUI线程中创建，GUI线程中销毁，但是可以在渲染线程中使用。
	// https://doc.qt.io/archives/qt-5.15/qoffscreensurface.html#details
	if (!pRenderSurface_)
	{
		pRenderSurface_ = offScreenSurface;
	}

	if (!pYuvDraw_)
	{
		pYuvDraw_ = new CYuvDraw{ pRenderCtx_, pRenderSurface_, nullptr };
		pYuvDraw_->moveToThread(this);
	}

	connect(pYuvDraw_, &CYuvDraw::textureReady, this, [this](unsigned int texid) {
		emit signal_NewYuvTexture(texid);
	}, Qt::QueuedConnection);
}

VideoCaptureThread::~VideoCaptureThread()
{
	isRunning_ = false;
	wait();
}

void VideoCaptureThread::initOpenCV(int w, int h)
{
	width_ = w;
	height_ = h;

	videoCapture_.open(0);
	if (!videoCapture_.isOpened())
	{
		qDebug() << "Error,can't open camera device.";
	}

	isRunning_ = true;

    //QString filePath = "D:/1_Code/QtCreator/LMEngine/facemodel";
	//QString filePath = "D:/WorkSpace/Clion/GitHubProject/LMEngine/facemodel";
	QString filePath = QApplication::applicationDirPath() + "/facemodel";
	bool retValue = FACETRACKER_API_init_facetracker_resources((char*)filePath.toStdString().c_str());
	if (!retValue) {
		qDebug() << "init facetracker failed.....";
		return;
	}

}

void VideoCaptureThread::stopCapture()
{
	isRunning_ = false;
}

void VideoCaptureThread::updateWH(const int& w, const int& h)
{
	pYuvDraw_->updateWH(w, h);
}

void VideoCaptureThread::run()
{
	pRenderCtx_->makeCurrent(pRenderSurface_);
	initOpenCV(1920, 1080);
	pYuvDraw_->initTexture();

	while (isRunning_)
	{
		cv::Mat videoFrame;
		videoCapture_ >> videoFrame;

		if (!videoFrame.empty())
		{
			cv::resize(videoFrame, videoFrame, cv::Size(width_, height_));
			FACETRACKER_API_facetracker_obj_track(videoFrame);
			ofVec2f posVec2f = FACETRACKER_API_getPosition(videoFrame);
			float currScale = FACETRACKER_API_getScale(videoFrame);

			if (posVec2f.x != -1 && posVec2f.y != -1) {
				QPoint facePoint = QPoint(posVec2f.x, posVec2f.y);
				emit signal_NewFacePos(facePoint, currScale);
				// qDebug() << "Face Position: " << facePoint << ", Scale: " << currentScale;
			}

			cv::Mat bgrCVFrame(videoFrame);
			cv::Mat yuvCVFrame;

			if (1 == videoFrame.channels()) {
				// 这里是将videoFrame从GRAY转化为BGR格式存于bgrCVFrame
				cv::cvtColor(videoFrame, bgrCVFrame, CV_GRAY2BGR);
			}
			// 同上，将bgrCVFrame从BGR转化为YUV_I420格式并存于yuvCVFrame
			cv::cvtColor(bgrCVFrame, yuvCVFrame, CV_BGR2YUV_I420);

			int lumaSize = width_ * height_;
			// 这里m_videoWidth和m_videoHeight都需要加1再除2是因为考虑到一张图片的宽高可能为奇数，所以需要加1。
			int uv_stride = (width_ + 1) / 2;
			int uv_height = (height_ + 1) / 2;
			int chromaSize = uv_stride * uv_height;

			uint8_t* Y_data_Dst = yuvCVFrame.data;
			uint8_t* U_data_Dst = yuvCVFrame.data + lumaSize;
			uint8_t* V_data_Dst = yuvCVFrame.data + lumaSize + chromaSize;

			YUVFrame  yuvFrame{};

			yuvFrame.luma.dataBuffer = Y_data_Dst;
			yuvFrame.luma.length = lumaSize;

			yuvFrame.chromaB.dataBuffer = U_data_Dst;
			yuvFrame.chromaB.length = chromaSize;

			yuvFrame.chromaR.dataBuffer = V_data_Dst;
			yuvFrame.chromaR.length = chromaSize;

			yuvFrame.width = width_;
			yuvFrame.height = height_;

			// 离屏渲染结束后，直接把texID发送给OpenGLWidget进行渲染
			pYuvDraw_->updateTexture(&yuvFrame);
		}
	}

	pRenderCtx_->doneCurrent();
}
