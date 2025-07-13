#include "GLYuvDraw.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <QDebug>
#include <QImage>
#include <QString>

CYuvDraw::CYuvDraw(QOpenGLContext* renderCtx, QOffscreenSurface* offScreenSurface, QObject* parent)
	: QObject(parent)
{
	if (!pRenderCtx_)
	{
		pRenderCtx_ = renderCtx;
	}

	// ������������GUI�߳��д�����GUI�߳������٣����ǿ�������Ⱦ�߳���ʹ�á�
	// https://doc.qt.io/archives/qt-5.15/qoffscreensurface.html#details
	if (!pRenderSurface_)
	{
		pRenderSurface_ = offScreenSurface;
	}
}

CYuvDraw::~CYuvDraw()
{
}

void CYuvDraw::initTexture()
{
	pRenderCtx_->makeCurrent(pRenderSurface_);

	auto f = pRenderCtx_->extraFunctions();

	if (!pYuvShaderProg_)
	{
		pYuvShaderProg_ = new GLShaderProgram{ pRenderCtx_ };
		pYuvShaderProg_->initialize(":/shaders/Resource/shaders/yuvOffsecreen.vs",
			":/shaders/Resource/shaders/yuvOffSecreen.fs");
	}

	f->glGenVertexArrays(1, &VAO_);
	f->glBindVertexArray(VAO_);

	f->glGenBuffers(1, &VBO_);
	f->glBindBuffer(GL_ARRAY_BUFFER, VBO_);
	// �����ݴ�������
	f->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);
	// ����λ������
	f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	f->glEnableVertexAttribArray(0);
	// ����uv����
	f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	f->glEnableVertexAttribArray(1);

	f->glGenBuffers(1, &EBO_);
	f->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
	f->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices_), indices_, GL_STATIC_DRAW);

	f->glGenTextures(3, yuvTexID_);

	initFrameBuffer();

	pRenderCtx_->doneCurrent();
}

void CYuvDraw::initFrameBuffer()
{
	pRenderCtx_->makeCurrent(pRenderSurface_);

	auto f = pRenderCtx_->extraFunctions();

	// ------------------------- FBO��֡������� -------------------------
	// ��������һ��֡�������֮������еĶ�ȡ��д��֡����Ĳ�������Ӱ�쵱ǰ�󶨵�֡����
	f->glGenFramebuffers(1, &FBO_);
	f->glBindFramebuffer(GL_FRAMEBUFFER, FBO_);

	// ------------------------- texture���������� -------------------------
	// ������һ��ͨ�����ݻ���(General Purpose Data Buffer)���ɶ���д

	// ��������һ��2D������
	f->glGenTextures(1, &TexID_);
	f->glBindTexture(GL_TEXTURE_2D, TexID_);
	// ����������data��nullptr����ʾ�����������ڴ��û�����������������������������Ⱦ��֡����֮�������У�Ҳ����˵����Ⱦ��֮�����ݻ���Ҫת��Ϊ�����ʽ
	f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// ���� ������ ���ӵ�֡����GL_FRAMEBUFFER�ϣ���ͨ��GL_COLOR_ATTACHMENT0ָ���� ���� ��һ����ɫ��������color attachment texture��
	f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TexID_, 0);

	// ------------------------- RBO����Ⱦ������󸽼��� -------------------------
	// RBO��ȻҲ��һ�����壬��RBO��ר�ű������Ϊ֡���帽��ʹ�õģ�ͨ������ֻд�ģ������ǲ���Ҫ����Щ�����в�����ʱ��ͨ��ѡ����Ⱦ�������
	// ��Ⱦ�������ֱ�ӽ����е���Ⱦ���ݴ��浽���Ļ����У��������κ���������ʽ��ת����������Ϊһ������Ŀ�д������ʡ�

	// ��������һ�� ��Ⱦ�������֮�����е���Ⱦ�������Ӱ�쵱ǰ��RBO���������ǲ���Ը�RBO����
	f->glGenRenderbuffers(1, &RBO_);
	f->glBindRenderbuffer(GL_RENDERBUFFER, RBO_);
	// ����һ����Ⱥ�ģ����Ⱦ������󣬴˴�ʹ�õ�����Ⱦ�������ͬʱ��Ϊ��Ȼ�������ģ�建�����
	f->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	// ���� ��Ⱦ������� ���ӵ�GL_FRAMEBUFFER�ϣ���ͨ��GL_DEPTH_STENCIL_ATTACHMENTָ������Ⱦ���������� ��Ȼ������ ���� ģ�建�����
	f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO_);

	// ���FBO�Ƿ�����
	if (f->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		qDebug() << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!";
	// ��֡����������Ĭ�ϻ������������ǰ֡�������
	f->glBindFramebuffer(GL_FRAMEBUFFER, pRenderCtx_->defaultFramebufferObject());

	pRenderCtx_->doneCurrent();
}

void CYuvDraw::updateWH(const int& w, const int& h)
{
	QMutexLocker mlk{ &mtx_ };
	width = w;
	height = h;
}

void CYuvDraw::updateTexture(YUVFrame* yuvBuffer)
{
	pRenderCtx_->makeCurrent(pRenderSurface_);

	auto f = pRenderCtx_->extraFunctions();

	/*static int org_wh = width * height;

	// 1. ���ڴ�С�ı�󣬼ǵø���texture��rbo��С������glDrawElements��ʱ��texture�Ŀռ䲻��
	if (org_wh != width * height)
	{
		qDebug() << width << " " << height;
		f->glBindTexture(GL_TEXTURE_2D, TexID_);
		f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		f->glBindTexture(GL_TEXTURE_2D, 0);

		f->glBindRenderbuffer(GL_RENDERBUFFER, RBO_);
		f->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
		f->glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	org_wh = width * height;

	static int viewportX = 0;
	static int viewportY = 0;
	static int viewportWidth = 0;
	static int viewportHeight = 0;
	// 2. �����ӿڣ�ʹ��widget�е�ͼ�α���ʼ�ղ���
	if (width > height)
	{
		viewportX = (width - height) / 2;
		viewportY = 0;
		viewportWidth = height;
		viewportHeight = height;
	}
	else
	{
		viewportX = 0;
		viewportY = (height - width) / 2;
		viewportWidth = width;
		viewportHeight = width;
	}
	// 3. һ��Ҫ�ǵ�glViewport;
    //f->glViewport(viewportX, viewportY, viewportWidth, viewportHeight);*/

	{
		QMutexLocker mlk{ &mtx_ };
		f->glViewport(0, 0, width, height);
	}

	f->glBindFramebuffer(GL_FRAMEBUFFER, FBO_);
	f->glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	f->glClear(GL_COLOR_BUFFER_BIT);

	pYuvShaderProg_->use();
	f->glBindVertexArray(VAO_);

	pYuvShaderProg_->set1i("texY", 0);
	f->glActiveTexture(GL_TEXTURE0);
	f->glBindTexture(GL_TEXTURE_2D, yuvTexID_[0]);
	f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, yuvBuffer->width, yuvBuffer->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvBuffer->luma.dataBuffer);
	f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	pYuvShaderProg_->set1i("texU", 1);
	f->glActiveTexture(GL_TEXTURE1);
	f->glBindTexture(GL_TEXTURE_2D, yuvTexID_[1]);
	f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, (yuvBuffer->width + 1) / 2, (yuvBuffer->height + 1) / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvBuffer->chromaB.dataBuffer);
	f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	pYuvShaderProg_->set1i("texV", 2);
	f->glActiveTexture(GL_TEXTURE2);
	f->glBindTexture(GL_TEXTURE_2D, yuvTexID_[2]);
	f->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, (yuvBuffer->width + 1) / 2, (yuvBuffer->height + 1) / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuvBuffer->chromaR.dataBuffer);
	f->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	f->glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	f->glFinish();

	emit textureReady(TexID_);
    // saveImage();

	pRenderCtx_->doneCurrent();
}


void CYuvDraw::saveImage()
{

	auto f = pRenderCtx_->extraFunctions();
	static int     w = 0;
	static int     h = 0;
	{
		QMutexLocker mlk{ &mtx_ };
		w = width;
		h = height;
	}
	int size = w * h * 4;
	unsigned char* data = new unsigned char[size];
	memset(data, 0, w * h * 4);
	f->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);

	QString tempPath = "D:/1_Code/QtCreator/LMEngine/test.png";

	QImage image = QImage(data, w, h, QImage::Format_RGB32);
	if (image.isNull())
	{
		qDebug() << "Receive frame error, width:%d, height:%d." << w << h;

		return;
	}
    image = image.rgbSwapped();
	image = image.mirrored();
	image.save(tempPath, "PNG", 100);
}
