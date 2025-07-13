#include "GLFrame.h"
#include <QFrame>
#include <QDebug>

CGLFrame::CGLFrame(QObject* parent)
	: QObject(parent)
{
	pShaderProg_ = new GLShaderProgram{ this };
}
CGLFrame::~CGLFrame()
{
	
}

void CGLFrame::initialize()
{
	// 初始化
	initializeOpenGLFunctions();

	/*********************************** 初始化着色器程序对象 ***********************************/
	pShaderProg_->initialize(
		":/shaders/Resource/shaders/frame.vs",
		":/shaders/Resource/shaders/frame.fs"
	);

	/*********************************** 初始化VAO,VBO,EBO,texture ***********************************/
	// ------------------------- VAO -------------------------
	GLuint VAO = 0;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// ------------------------- VBO -------------------------
	GLuint VBO = 0;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// 将数据传至缓存
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);

	// 设置位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// 设置uv坐标
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// ------------------------- 绑定texture -------------------------
	pShaderProg_->use();
	pShaderProg_->set1i("frameTexture", 0);

	VAO_ = VAO;
	VBO_ = VBO;
}

void CGLFrame::draw(const glm::mat4& view, const glm::mat4& projection, const GLuint& FrameTexID)
{
	isMoving_ = true;
	pShaderProg_->use();
	pShaderProg_->setMatrix4fv("view", 1, GL_FALSE, glm::value_ptr(view));
	pShaderProg_->setMatrix4fv("projection", 1, GL_FALSE, glm::value_ptr(projection));

	// skybox cube
	glBindVertexArray(VAO_);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, FrameTexID);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindVertexArray(0);
}

void CGLFrame::move(const QPoint& pos, const float& scale)
{
	if (isMoving_)
	{
		// 这里之所以用1.0减去坐标，是因为计算后的x,y坐标值域∈[0, 2]，而我们需要将其映射到[-1, 1]的范围内，同时反转x，y轴
		float x = 1.0f - pos.x() * 2.0 / 1920;
		float y = 1.0f - pos.y() * 2.0 / 1080;
		qDebug() << "move pos: " << x << " " << y;
		pShaderProg_->use();
		pShaderProg_->set2f("pos", x, y);
	}
	
}