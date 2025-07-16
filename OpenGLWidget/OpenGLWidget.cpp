#include "OpenGLWidget.h"

#include <qapplication.h>
#include <QDebug>
#include <QTextStream>
#include <QKeyEvent>
#include <QPoint>


/*
 * OpenGLWidget��һ������Ĺ����࣬�����������֣�
 * 1. ��ʼ��������������Ⱦ�����ģ�ʹ��VideoCaptureThread�ڲ�����Ƶ֮֡�󣬿���ֱ�����Ǹ��߳���������Ⱦ��texture�ϣ�
 *		Ȼ�󴫸����̣߳�OpenGLWidget������󷢸��Ӷ���YUVFrame��Ҳ���Ǳ�����OpenGLWidget�õ�������Ⱦ�̵߳������ģ�VideoCaptureThread�õ���������Ⱦ�����ġ�
 * 2. ����view��projection���󣬽��䴫�����Ӷ������ַ���
 * 3. ¼����Ƶ��ֱ��ʹ��FBO+˫PBO����
 * 4. �����ڴ�С�����仯ʱ���������ӿ��Լ���ز���
 */

OpenGLWidget::OpenGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
	//����Ĭ��׷����꣬�����ڴ�������ƶ�ʱ�������ȵ�һ�²���Ч
	this->setMouseTracking(true);
	// ���ý�����ԣ��� OpenGLWidget ��ȡ�����¼�
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
	// ��ʼ��
	initializeOpenGLFunctions();

	mainCtx_ = QOpenGLContext::currentContext();
	qDebug() << mainCtx_;

	/*********************************** ��ʼ�������Ͷ��߳�������Դ ***********************************/

	initOffScreenEnv();
	pGLSceneManager_->initialize();

	/*********************************** ��ʼ����Ļ��������ɫ��������� ***********************************/

	pMainShaderProg_->initialize(
		":/shaders/Resource/shaders/mainWidget.vs",
		":/shaders/Resource/shaders/mainWidget.fs"
	);

	glGenVertexArrays(1, &planeVAO_);
	glBindVertexArray(planeVAO_);

	glGenBuffers(1, &planeVBO_);
	glBindBuffer(GL_ARRAY_BUFFER, planeVBO_);
	// �����ݴ�������
	glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices_), planeVertices_, GL_STATIC_DRAW);
	// ����λ������
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// ����uv����
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &planeEBO_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeEBO_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeIndices_), planeIndices_, GL_STATIC_DRAW);

	pMainShaderProg_->use();
	// ��Ϊ���ǽ����������ӵ���֡���壬���е���Ⱦָ���д�뵽��������У�����������Ҫָ������Ԫ
	pMainShaderProg_->set1i("mainTexture", 0);
	pMainShaderProg_->unuse();

	initFrameBuffer();
	initPBOs();

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

void OpenGLWidget::initFrameBuffer()
{
	qDebug() << "Initializing FBO with size:" << width() << "x" << height();

	// ------------------------- FBO��֡������� -------------------------
	// ��������һ��֡�������֮������еĶ�ȡ��д��֡����Ĳ�������Ӱ�쵱ǰ�󶨵�֡����
	GLuint FBO;
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	
	// ------------------------- texture���������� -------------------------
	// ������һ��ͨ�����ݻ���(General Purpose Data Buffer)���ɶ���д

	// ��������һ��2D������
	glGenTextures(1, &planeTexID_);
	glBindTexture(GL_TEXTURE_2D, planeTexID_);
	// ����������data��nullptr����ʾ�����������ڴ��û�����������������������������Ⱦ��֡����֮�������У�Ҳ����˵����Ⱦ��֮�����ݻ���Ҫת��Ϊ�����ʽ
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width(), height(), 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// ���� ������ ���ӵ�֡����GL_FRAMEBUFFER�ϣ���ͨ��GL_COLOR_ATTACHMENT0ָ���� ���� ��һ����ɫ��������color attachment texture��
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, planeTexID_, 0);

	// ------------------------- RBO����Ⱦ������󸽼��� -------------------------
	// RBO��ȻҲ��һ�����壬��RBO��ר�ű������Ϊ֡���帽��ʹ�õģ�ͨ������ֻд�ģ������ǲ���Ҫ����Щ�����в�����ʱ��ͨ��ѡ����Ⱦ�������
	// ��Ⱦ�������ֱ�ӽ����е���Ⱦ���ݴ��浽���Ļ����У��������κ���������ʽ��ת����������Ϊһ������Ŀ�д������ʡ�

	// ��������һ�� ��Ⱦ�������֮�����е���Ⱦ�������Ӱ�쵱ǰ��RBO���������ǲ���Ը�RBO����
	glGenRenderbuffers(1, &planeRBO_);
	glBindRenderbuffer(GL_RENDERBUFFER, planeRBO_);
	// ����һ����Ⱥ�ģ����Ⱦ������󣬴˴�ʹ�õ�����Ⱦ�������ͬʱ��Ϊ��Ȼ�������ģ�建�����
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width(), height());
	// ���� ��Ⱦ������� ���ӵ�GL_FRAMEBUFFER�ϣ���ͨ��GL_DEPTH_STENCIL_ATTACHMENTָ������Ⱦ���������� ��Ȼ������ ���� ģ�建�����
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, planeRBO_);

	// ���FBO�Ƿ�����
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
		qDebug() << "ERROR::FRAMEBUFFER:: Framebuffer is not complete: " << hex << status;
	// ��֡����������Ĭ�ϻ������������ǰ֡�������
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());

	planeFBO_ = FBO;
}

void OpenGLWidget::initPBOs()
{
	// 2. ���� 2 �����ػ��������󣬳����˳�ʱ��Ҫɾ�����ǡ�
	// 2.1 ʹ��nullptr���� glBufferData () ��Ԥ���ڴ�ռ䡣
	glGenBuffers(2, PBOIds_);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOIds_[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, width() * height() * 4, nullptr, GL_STREAM_READ);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOIds_[1]);
	glBufferData(GL_PIXEL_PACK_BUFFER, width() * height() * 4, nullptr, GL_STREAM_READ);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void OpenGLWidget::paintGL()
{
	QDateTime currtime = QDateTime::currentDateTime();
	deltaTime_ = lastTime_.msecsTo(currtime);
	lastTime_ = currtime;
	pCamera_->checkInputandUpdateCamera(deltaTime_);

	// ģ�;����ɸ��Ե�object����

	// �����۲����
	glm::mat4 view{ 1.0f };
	//view = glm::translate(view, glm::vec3{ 0.0f, 0.0f, -3.0f });
	view = pCamera_->getLookAt();

	// ����ͶӰ����
	glm::mat4 projection{ 1.0f };
	int width = geometry().width();
	int height = geometry().height();
	projection = glm::perspective(glm::radians(pCamera_->zoom_), static_cast<float>(width / height), 0.1f, 1000.0f);

	// ------------------------- FBO������Ⱦ -------------------------
	glBindFramebuffer(GL_FRAMEBUFFER, planeFBO_);
	glEnable(GL_DEPTH_TEST);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::vec3 lightPos(3.5f, 3.0f, 3.3f);
	static int interval_ = 0;
	interval_ = ++interval_ % 100;	// 1000ָ����һ��ѭ��Ϊ1000
	double degree = 2.0 * 3.1415926535 * interval_ / 100;	// ��ʱdegree��ֵ��Ϊ[0, 2*pi]
	lightPos = glm::vec3(2.0f * cos(degree), 1.0f, 2.0f * sin(degree));
	pGLSceneManager_->draw(view, projection, FrameTexID_, lightPos, pCamera_->position_);

	if (isRecording_) {
        usePBOs();
	}

	// ------------------------- ��ǰ��Ļ��Ⱦ����������Ⱦ������ͼ����Ⱦ����ǰ��Ļ�ϣ� -------------------------
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
	glDisable(GL_DEPTH_TEST);
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	pMainShaderProg_->use();
	glBindVertexArray(planeVAO_);
	glBindTexture(GL_TEXTURE_2D, planeTexID_);
	//glBindTexture(GL_TEXTURE_2D, FrameTexID_);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	pMainShaderProg_->unuse();
}

void OpenGLWidget::reallocFrameBuffer(const int& w, const int& h)
{
	glBindTexture(GL_TEXTURE_2D, planeTexID_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindRenderbuffer(GL_RENDERBUFFER, planeRBO_);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void OpenGLWidget::reallocPBOs(const int& w, const int& h)
{
	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOIds_[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, w * h * 4, nullptr, GL_STREAM_READ);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOIds_[1]);
	glBufferData(GL_PIXEL_PACK_BUFFER, w * h * 4, nullptr, GL_STREAM_READ);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void OpenGLWidget::adjustViewPort(const int& w, const int& h)
{
	int width_ = width();
	int height_ = height();

	static int viewportX = 0;
	static int viewportY = 0;
	static int viewportWidth = 0;
	static int viewportHeight = 0;
	// 2. �����ӿڣ�ʹ��widget�е�ͼ�α���ʼ�ղ���
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
	// 3. һ��Ҫ�ǵ�glViewport;
	glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
}

void OpenGLWidget::resizeGL(int w, int h)
{
	// ����������Ⱦ���ԣ�ÿ�λ�ͼ����Ҫ���ݴ��ڴ�С��̬����FBO�е�texture��rbo�Ĵ�С
	static int org_wh = w * h;

	// ���ڴ�С�ı�󣬼ǵø���texture��rbo��С������glDrawElements��ʱ��texture�Ŀռ䲻��
	if (org_wh != w * h)
	{
		qDebug() << w << " " << h;
		reallocFrameBuffer(w, h);
		reallocPBOs(w, h);
		adjustViewPort(w, h);
	}
	org_wh = w * h;
}

void OpenGLWidget::startRecord()
{
	// initialize�������paintGL�м�¼
	QDateTime dateTime = QDateTime::currentDateTime();
	QString pathStr = qApp->applicationDirPath() + "/" + dateTime.toString("yyyyMMddhhmmss") + ".mp4";
	qDebug() << "start record video to: " << pathStr;

	// todo ��Ҫ���������й̶����ڵĿ��
	CAVRecorder::GetInstance()->setInputWH(width(), height());
	//CAVRecorder::GetInstance()->setOutputWH(640, 480);
	CAVRecorder::GetInstance()->setOutputWH(width(), height());
	Q_ASSERT(CAVRecorder::GetInstance()->initOutputFile(pathStr.toUtf8().data()));

	isRecording_ = true;
}

void OpenGLWidget::usePBOs()
{
	// dmaָ����ֱ���ڴ���ʼ�����Ӳ����ͨ��CPU������ͨ��DMA��������ֱ�ӷ��ʣ��޸ģ������ڴ���Դ��е����ݡ�
	static int dma = 0;
	static int read = 0;

	/*********************************** USES PBO (Streaming Texture Uploads ��ʽ�������) ***********************************/

	// https://learn.microsoft.com/zh-cn/windows/win32/opengl/glreadbuffer
	// https://blog.csdn.net/heyuchang666/article/details/69523417
	// QOpenGLWidgetĬ��ʹ��˫���壬����ɫ������ָ��Ϊ���� glReadPixels �� glCopyPixels �����Դ
	glReadBuffer(GL_FRONT);

	// ------------------------- pack PBO -------------------------
	// ��ʹ�� glBindBuffer() ��PBO�󶨵�GL_PIXEL_PACK_BUFFER�Ϻ�
	// �������е�pack�������� glReadPixels(), glGetTexImage() �Ⱥ���
	// �Ὣ�������ݴ� ֡������ ���� ����ͼ�� ���䵽 PBO��Ӧ�����ػ�����
	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOIds_[dma]);
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glReadPixels(0, 0, width(), height(), GL_RGBA, GL_UNSIGNED_BYTE, 0);

	// ------------------------- update PBO -------------------------
	// �����󶨵�PBO�����ڸ�����������
	glBindBuffer(GL_PIXEL_PACK_BUFFER, PBOIds_[read]);
	//glBufferData(GL_PIXEL_PACK_BUFFER, DATA_SIZE, 0, GL_STREAM_DRAW);
	//ptr��ʾGPU�ϵ�PBO���ڴ�ռ��ӳ���ַ�����ǲ���ptr��ʵ������ֱ�Ӳ�����GPU�е�PBO
	GLubyte* ptr = static_cast<GLubyte*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER,
		0, width() * height() * 4, GL_MAP_READ_BIT));
	if (ptr)
	{
		recordAV(ptr, width(), height(), 4);
		//saveImage(ptr);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	}

	// ʹ�����ǵ��ͷŰ�
	// glBindBuffer()һ���󶨵� 0���������ز����ͻ��Գ��淽ʽ���С�
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	std::swap(dma, read);
}

void OpenGLWidget::saveImage(GLubyte* ptr)
{

	QString tempPath = "D:/1_Code/QtCreator/PboPack.png";
	QImage image = QImage(ptr, width(), height(), QImage::Format_ARGB32);
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

	CAVRecorder::GetInstance()->stopRecord();
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
	// ���������������   
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
