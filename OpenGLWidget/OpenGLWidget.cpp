#include "OpenGLWidget.h"

#include <qapplication.h>
#include <QDebug>
#include <QTextStream>
#include <QKeyEvent>
#include <QPoint>


/*
 * OpenGLWidget是一个整体的管理类，他负责三部分：
 * 1. 初始化，创建离屏渲染上下文，使得VideoCaptureThread在捕获视频帧之后，可以直接在那个线程里离屏渲染到texture上，
 *		然后传给主线程（OpenGLWidget），随后发给子对象（YUVFrame）也就是标明了OpenGLWidget用的是主渲染线程的上下文，VideoCaptureThread用的是离屏渲染上下文。
 * 2. 更新view，projection矩阵，将其传给其子对象做分发。
 * 3. 录制视频，直接使用FBO+双PBO即可
 * 4. 当窗口大小发生变化时，重设置视口以及相关参数
 */

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
	//设置默认追踪鼠标，否则在触发鼠标移动时，必须先点一下才有效
	this->setMouseTracking(true);
	// 设置焦点策略，让 OpenGLWidget 获取键盘事件
	this->setFocusPolicy(Qt::StrongFocus);
	this->setFocus();

	pCamera_ = new CGLCamera{ this, cameraPos_ };
	pGLSceneManager_ = new GLSceneManager{ this };
	pMainShaderProg_ = new GLShaderProgram{ this };
}

OpenGLWidget::~OpenGLWidget()
{
	pRenderThread_->quit();
	pRenderThread_->wait();
	delete pRenderThread_;
}

void OpenGLWidget::initializeGL()
{
	// 初始化
	initializeOpenGLFunctions();

	mainCtx_ = QOpenGLContext::currentContext();
	qDebug() << mainCtx_;

	/*********************************** 初始化场景和多线程离屏资源 ***********************************/

	initOffScreenEnv();
	pGLSceneManager_->initialize();

	/*********************************** 初始化屏幕缓冲区着色器程序对象 ***********************************/

	pMainShaderProg_->initialize(
		":/shaders/Resource/shaders/mainWidget.vs",
		":/shaders/Resource/shaders/mainWidget.fs"
	);

	glGenVertexArrays(1, &sceneVAO_);
	glBindVertexArray(sceneVAO_);

	glGenBuffers(1, &sceneVBO_);
	glBindBuffer(GL_ARRAY_BUFFER, sceneVBO_);
	// 将数据传至缓存
	glBufferData(GL_ARRAY_BUFFER, sizeof(sceneVertices_), sceneVertices_, GL_STATIC_DRAW);
	// 设置位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// 设置uv坐标
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &sceneEBO_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sceneEBO_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sceneIndices_), sceneIndices_, GL_STATIC_DRAW);

	pMainShaderProg_->use();
	// 因为我们将纹理附件附加到了帧缓冲，所有的渲染指令将会写入到这个纹理中，所以我们需要指定纹理单元
	pMainShaderProg_->set1i("mainTexture", 0);
	pMainShaderProg_->unuse();

	initSceneFrameBuffer();
	initRecordFrameBuffer();
	initRecordPBOs();

	lastTime_ = QDateTime::currentDateTime();
}

void OpenGLWidget::initOffScreenEnv()
{
	auto mainCtx = QOpenGLContext::currentContext();

	if (!pRenderSurface_)
	{
		pRenderSurface_ = new QOffscreenSurface(this->screen(), this);
		//pRenderSurface_ = new QOffscreenSurface(nullptr, this);
		pRenderSurface_->setFormat(mainCtx->format());
		pRenderSurface_->create();
	}

	if (!pRenderThread_)
	{
		pRenderThread_ = new VideoCaptureThread(mainCtx, pRenderSurface_, this);
		pRenderThread_->updateWH(width(), height());
		pRenderThread_->start();
		connect(pRenderThread_, &VideoCaptureThread::signal_NewYuvTexture, this, [this](unsigned int texid) {
			FrameTexID_ = texid;
			//qDebug() << "main: " << TexID;
			this->update();
		}, Qt::QueuedConnection);

		connect(pRenderThread_, &VideoCaptureThread::signal_NewFacePos, this, [this](const QPoint& pos, const float& scale) {
			pGLSceneManager_->moveFace(pos, scale);
		}, Qt::QueuedConnection);
	}
}

void OpenGLWidget::initSceneFrameBuffer()
{
	qDebug() << "Initializing FBO with size:" << width() << "x" << height();

	// ------------------------- FBO（帧缓冲对象） -------------------------
	// 创建并绑定一个帧缓冲对象，之后的所有的读取和写入帧缓冲的操作将会影响当前绑定的帧缓冲
	GLuint FBO;
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	
	// ------------------------- texture（纹理附件） -------------------------
	// 纹理是一个通用数据缓冲(General Purpose Data Buffer)，可读可写

	// 创建并绑定一个2D纹理附件
	glGenTextures(1, &sceneTexID_);
	glBindTexture(GL_TEXTURE_2D, sceneTexID_);
	// 给纹理附件的data传nullptr，表示仅仅分配了内存而没有填充它。填充这个纹理将会在我们渲染到帧缓冲之后来进行，也就是说在渲染好之后，数据还需要转换为纹理格式
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width(), height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// 将该 纹理附件 附加到帧缓冲GL_FRAMEBUFFER上，并通过GL_COLOR_ATTACHMENT0指明该 纹理 是一个颜色附件纹理（color attachment texture）
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneTexID_, 0);

	// ------------------------- RBO（渲染缓冲对象附件） -------------------------
	// RBO虽然也是一个缓冲，但RBO是专门被设计作为帧缓冲附件使用的，通常都是只写的，当我们不需要从这些缓冲中采样的时候，通常选择渲染缓冲对象
	// 渲染缓冲对象直接将所有的渲染数据储存到它的缓冲中，不会做任何针对纹理格式的转换，让它变为一个更快的可写储存介质。

	// 创建并绑定一个 渲染缓冲对象，之后所有的渲染缓冲操作影响当前的RBO，不过我们不会对该RBO采样
	glGenRenderbuffers(1, &sceneRBO_);
	glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO_);
	// 创建一个深度和模板渲染缓冲对象，此处使用单个渲染缓冲对象同时作为深度缓冲对象和模板缓冲对象。
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width(), height());
	// 将该 渲染缓冲对象 附加到GL_FRAMEBUFFER上，并通过GL_DEPTH_STENCIL_ATTACHMENT指明该渲染缓冲对象既是 深度缓冲对象 又是 模板缓冲对象
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneRBO_);

	// 检查FBO是否完整
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		qDebug() << "ERROR::FRAMEBUFFER:: Framebuffer is not complete: " << hex << status;
	// 将帧缓冲对象绑定至默认缓冲区，即解绑当前帧缓冲对象
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	sceneFBO_ = FBO;
}

void OpenGLWidget::initRecordFrameBuffer()
{
	glGenFramebuffers(1, &recordFBO_);
	glBindFramebuffer(GL_FRAMEBUFFER, recordFBO_);

	glGenTextures(1, &recordTexID_);
	glBindTexture(GL_TEXTURE_2D, recordTexID_);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, recordW_, recordH_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, recordTexID_, 0);

	// 检查FBO是否完整
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		qDebug() << "ERROR::FRAMEBUFFER:: Framebuffer is not complete: " << Qt::hex << status;

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
}

void OpenGLWidget::initRecordPBOs()
{
	// 2. 创建 2 个像素缓冲区对象，程序退出时需要删除它们。
	// 2.1 使用nullptr调用 glBufferData () 仅预留内存空间。
	glGenBuffers(2, recordPBOIds_);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, recordPBOIds_[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, recordW_ * recordH_ * 4, nullptr, GL_STREAM_READ);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, recordPBOIds_[1]);
	glBufferData(GL_PIXEL_PACK_BUFFER, recordW_ * recordH_ * 4, nullptr, GL_STREAM_READ);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void OpenGLWidget::paintGL()
{
	QDateTime currtime = QDateTime::currentDateTime();
	deltaTime_ = lastTime_.msecsTo(currtime);
	lastTime_ = currtime;
	pCamera_->checkInputandUpdateCamera(deltaTime_);

	// 模型矩阵由各自的object创建

	// 创建观察矩阵
	glm::mat4 view{ 1.0f };
	//view = glm::translate(view, glm::vec3{ 0.0f, 0.0f, -3.0f });
	view = pCamera_->getLookAt();

	// 创建投影矩阵
	glm::mat4 projection{ 1.0f };
	int width = this->width();
	int height = this->height();
	projection = glm::perspective(glm::radians(pCamera_->zoom_), static_cast<float>(width / height), 0.1f, 1000.0f);

	// ------------------------- FBO离屏渲染 -------------------------
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO_);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::vec3 lightPos(3.5f, 3.0f, 3.3f);
	static int interval_ = 0;
	interval_ = ++interval_ % 100;	// 1000指的是一个循环为1000
	double degree = 2.0 * 3.1415926535 * interval_ / 100;	// 此时degree的值域为[0, 2*pi]
	lightPos = glm::vec3(2.0f * cos(degree), 1.0f, 2.0f * sin(degree));
	pGLSceneManager_->draw(view, projection, FrameTexID_, lightPos, pCamera_->position_);

	if (isRecording_) {
		// YUV420P要求分辨率必须为偶数！！！否则avcodec_open2打开h.264编码器失败
		// 所以这里需要对视频帧做转换

		glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFBO_);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, recordFBO_);

		glBlitFramebuffer(
			0, 0, width, height,		// 源: 整个场景FBO
			0, 0, recordW_, recordH_,	// 目标: 整个录制FBO
			GL_COLOR_BUFFER_BIT,
			GL_LINEAR
		);

		// 解绑，防止后续操作影响
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, recordFBO_);
        useRecordPBOs();
	}

	// ------------------------- 当前屏幕渲染（将离屏渲染的纹理图像渲染到当前屏幕上） -------------------------
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glDisable(GL_DEPTH_TEST);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	pMainShaderProg_->use();
	glBindVertexArray(sceneVAO_);
	glBindTexture(GL_TEXTURE_2D, sceneTexID_);
	//glBindTexture(GL_TEXTURE_2D, FrameTexID_);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	pMainShaderProg_->unuse();
}

void OpenGLWidget::reallocSceneFrameBuffer(const int& w, const int& h)
{
	glBindTexture(GL_TEXTURE_2D, sceneTexID_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void OpenGLWidget::adjustViewPort(const int& w, const int& h)
{
	int width_ = width();
	int height_ = height();

	static int viewportX = 0;
	static int viewportY = 0;
	static int viewportWidth = 0;
	static int viewportHeight = 0;
	// 2. 调整视口，使得widget中的图形比例始终不变
	if (width_ > height_)
	{
		viewportX = (width_ - height_) / 2;
		viewportY = 0;
		viewportWidth = height_;
		viewportHeight = height_;
	}
	else
	{
		viewportX = 0;
		viewportY = (height_ - width_) / 2;
		viewportWidth = width_;
		viewportHeight = width_;
	}
	// 3. 一定要记得glViewport;
	glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
}

void OpenGLWidget::resizeGL(int w, int h)
{
	// 对于离屏渲染而言，每次绘图都需要根据窗口大小动态调整FBO中的texture和rbo的大小
	static int org_wh = w * h;

	// 窗口大小改变后，记得更新texture和rbo大小，否则glDrawElements的时候，texture的空间不够
	if (org_wh != w * h)
	{
		qDebug() << w << " " << h;
		reallocSceneFrameBuffer(w, h);
		adjustViewPort(w, h);
	}
	org_wh = w * h;
}

void OpenGLWidget::startRecord()
{
	// initialize，随后在paintGL中记录
	QDateTime dateTime = QDateTime::currentDateTime();
	QString pathStr = qApp->applicationDirPath() + "/" + dateTime.toString("yyyyMMddhhmmss") + ".mp4";
	qDebug() << "start record video to: " << pathStr;

	AVConfig config{
		pathStr.toStdString(),
		VideoConfig{recordW_, recordH_, 640, 480, 30, 2000000},
		AudioConfig{48000, 2, 128000}
	};
	Q_ASSERT(CAVRecorder::GetInstance()->initialize(config));

	isRecording_ = true;
}

void OpenGLWidget::useRecordPBOs()
{
	// dma指的是直接内存访问技术，硬件不通过CPU，而是通过DMA控制器，直接访问，修改，传输内存或显存中的数据。
	static int dma = 0;
	static int read = 0;

	/*********************************** USES PBO (Streaming Texture Uploads 流式纹理更新) ***********************************/

	// ------------------------- pack PBO -------------------------
	// 当使用 glBindBuffer() 将PBO绑定到GL_PIXEL_PACK_BUFFER上后，
	// 后续所有的pack操作：如 glReadPixels(), glGetTexImage() 等函数
	// 会将像素数据从 帧缓冲区 或者 纹理图像 传输到 PBO对应的像素缓冲区
	glBindBuffer(GL_PIXEL_PACK_BUFFER, recordPBOIds_[dma]);
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glReadPixels(0, 0, recordW_, recordH_, GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// ------------------------- update PBO -------------------------
	// 更换绑定的PBO，用于更新像素数据
	glBindBuffer(GL_PIXEL_PACK_BUFFER, recordPBOIds_[read]);
	//glBufferData(GL_PIXEL_PACK_BUFFER, DATA_SIZE, 0, GL_STREAM_DRAW);
	//ptr表示GPU上的PBO在内存空间的映射地址，我们操作ptr，实际上是直接操作的GPU中的PBO
	GLubyte* ptr = static_cast<GLubyte*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, recordW_ * recordH_ * 4, GL_MAP_READ_BIT));
	if (ptr)
	{
		recordAV(ptr, recordW_, recordH_, 4);
		//saveImage(ptr);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}
	else
	{
		qDebug() << "no ptr!";
	}

	// 使用完后记得释放绑定
	// glBindBuffer()一旦绑定到 0，所有像素操作就会以常规方式运行。
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	std::swap(dma, read);
}

void OpenGLWidget::saveImage(GLubyte* ptr)
{

	QString tempPath = "D:/WorkSpace/Clion/GitHubProject/PboPack.png";
	QImage image = QImage(ptr, recordW_, recordH_, QImage::Format_ARGB32);
	if (image.isNull()) {
		return;
	}
	image = image.rgbSwapped();
	image = image.mirrored();
	if (!image.save(tempPath, "PNG", 100))
		qDebug() << "save image error";
}

void OpenGLWidget::recordAV(GLubyte* ptr, int w, int h, int channel)
{
	if (!isRecording_)
	{
		qDebug() << "can't record video!";
	}
	Q_ASSERT(CAVRecorder::GetInstance()->recording(ptr));
}

void OpenGLWidget::stopRecord()
{
	isRecording_ = false;

	CAVRecorder::GetInstance()->startRecording();
}

void OpenGLWidget::keyPressEvent(QKeyEvent* event)
{
	if (event->key() >= 0 && event->key() < 1024)
	{
		pCamera_->keys[event->key()] = true;
	}
}

void OpenGLWidget::keyReleaseEvent(QKeyEvent* event)
{
	if (event->key() >= 0 && event->key() < 1024 && !event->isAutoRepeat())
	{
		pCamera_->keys[event->key()] = false;
	}
}

void OpenGLWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (isPressed_)
	{
		int currx = event->pos().x();
		int curry = event->pos().y();

		float offestx = static_cast<float>(currx - lastPos_.x());
		float offesty = static_cast<float>(lastPos_.y() - curry);
		lastPos_ = event->pos();

		pCamera_->setMouseMove(offestx, offesty, true);
	}
}

void OpenGLWidget::mousePressEvent(QMouseEvent* event)
{
	// 如果是鼠标左键按下   
	if (event->button() == Qt::LeftButton) {
		isPressed_ = true;
		lastPos_ = event->pos();
	}

}

void OpenGLWidget::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		isPressed_ = false;
	}

}

void OpenGLWidget::wheelEvent(QWheelEvent* event)
{
	QPoint offset = event->angleDelta();
	pCamera_->setMouseScroll(offset.y() / 20.0f);
}
