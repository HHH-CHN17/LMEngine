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

	// 离屏表面须在GUI线程中创建，GUI线程中销毁，但是可以在渲染线程中使用。
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
	// 将数据传至缓存
	f->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);
	// 设置位置属性
	f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	f->glEnableVertexAttribArray(0);
	// 设置uv坐标
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

	// ------------------------- FBO（帧缓冲对象） -------------------------
	// 创建并绑定一个帧缓冲对象，之后的所有的读取和写入帧缓冲的操作将会影响当前绑定的帧缓冲
	f->glGenFramebuffers(1, &FBO_);
	f->glBindFramebuffer(GL_FRAMEBUFFER, FBO_);

	// ------------------------- texture（纹理附件） -------------------------
	// 纹理是一个通用数据缓冲(General Purpose Data Buffer)，可读可写

	// 创建并绑定一个2D纹理附件
	f->glGenTextures(1, &TexID_);
	f->glBindTexture(GL_TEXTURE_2D, TexID_);
	// 给纹理附件的data传nullptr，表示仅仅分配了内存而没有填充它。填充这个纹理将会在我们渲染到帧缓冲之后来进行，也就是说在渲染好之后，数据还需要转换为纹理格式
	f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// 将该 纹理附件 附加到帧缓冲GL_FRAMEBUFFER上，并通过GL_COLOR_ATTACHMENT0指明该 纹理 是一个颜色附件纹理（color attachment texture）
	f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, TexID_, 0);

	// ------------------------- RBO（渲染缓冲对象附件） -------------------------
	// RBO虽然也是一个缓冲，但RBO是专门被设计作为帧缓冲附件使用的，通常都是只写的，当我们不需要从这些缓冲中采样的时候，通常选择渲染缓冲对象
	// 渲染缓冲对象直接将所有的渲染数据储存到它的缓冲中，不会做任何针对纹理格式的转换，让它变为一个更快的可写储存介质。

	// 创建并绑定一个 渲染缓冲对象，之后所有的渲染缓冲操作影响当前的RBO，不过我们不会对该RBO采样
	f->glGenRenderbuffers(1, &RBO_);
	f->glBindRenderbuffer(GL_RENDERBUFFER, RBO_);
	// 创建一个深度和模板渲染缓冲对象，此处使用单个渲染缓冲对象同时作为深度缓冲对象和模板缓冲对象。
	f->glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
	// 将该 渲染缓冲对象 附加到GL_FRAMEBUFFER上，并通过GL_DEPTH_STENCIL_ATTACHMENT指明该渲染缓冲对象既是 深度缓冲对象 又是 模板缓冲对象
	f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO_);

	// 检查FBO是否完整
	if (f->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		qDebug() << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!";
	// 将帧缓冲对象绑定至默认缓冲区，即解绑当前帧缓冲对象
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

	// 1. 窗口大小改变后，记得更新texture和rbo大小，否则glDrawElements的时候，texture的空间不够
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
	// 2. 调整视口，使得widget中的图形比例始终不变
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
	// 3. 一定要记得glViewport;
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
