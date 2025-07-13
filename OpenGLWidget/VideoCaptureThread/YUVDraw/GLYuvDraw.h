#pragma once

#include <QObject>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QMutex>
#include <gl/GL.h>

#include "Common/ShaderProgram/GLShaderProgram.h"
#include "Common/DataDefine.h"

class CYuvDraw : public QObject, public QOpenGLExtraFunctions
{
	Q_OBJECT

public:
	CYuvDraw(QOpenGLContext* renderCtx, QOffscreenSurface* offScreenSurface, QObject* parent);
	~CYuvDraw();

	void updateWH(const int& w, const int& h);

public:
	void initTexture();
	void updateTexture(YUVFrame* yuvBuffer);
	void saveImage();

private:
	void initFrameBuffer();

signals:
	void textureReady(unsigned int texID);

private:
	QOpenGLContext* pRenderCtx_ = nullptr;
	QOffscreenSurface* pRenderSurface_ = nullptr;

	bool isRunning_ = false;

	// 顶点数组是一维的
	float vertices_[20] =
	{
		//	---- 位置 ----       ---- uv ----
		-1.0f, 1.0f, 1.0f,			0, 0,
		-1.0f,-1.0f, 1.0f,			0, 1,
		1.0f,-1.0f, 1.0f,			1, 1,
		1.0f, 1.0f, 1.0f,			1, 0
	};

	GLuint indices_[6] =
	{
		0, 1, 3, // 第一个三角形
		1, 2, 3  // 第二个三角形
	};
	GLuint VAO_ = 0;
	GLuint VBO_ = 0;
	GLuint EBO_ = 0;
	GLuint FBO_ = 0;
	GLuint RBO_ = 0;
	GLuint yuvTexID_[3];
	GLuint TexID_ = 0;
	
	GLShaderProgram* pYuvShaderProg_ = nullptr;

	QMutex mtx_;
	int width = 0;
	int height = 0;
};

