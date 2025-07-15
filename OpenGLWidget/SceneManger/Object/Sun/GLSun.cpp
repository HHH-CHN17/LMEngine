#include "GLSun.h"
#include <QFrame>
#include <QDebug>

#include "OpenGLWidget/SceneManger/Object/Frame/GLFrame.h"

CGLSun::CGLSun(QObject* parent)
	: QObject(parent)
{
	pShaderProg_ = new GLShaderProgram{ this };
}
CGLSun::~CGLSun()
{

}

void CGLSun::initialize()
{
	// ��ʼ��
	initializeOpenGLFunctions();

	/*********************************** ��ʼ����ɫ��������� ***********************************/
	pShaderProg_->initialize(
		":/shaders/Resource/shaders/sun.vs",
		":/shaders/Resource/shaders/sun.fs"
	);

	/*********************************** ��ʼ��VAO,VBO,EBO,texture ***********************************/

	// ------------------------- lightVAO -------------------------
	GLuint lightVAO = 0;
	glGenVertexArrays(1, &lightVAO);
	glBindVertexArray(lightVAO);

	// ------------------------- lightVBO -------------------------
	GLuint lightVBO = 0;
	glGenBuffers(1, &lightVBO);
	glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
	// �����ݴ�������
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);

	// ����λ������
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	VAO_ = lightVAO;
	VBO_ = lightVBO;
}

void CGLSun::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightPos)
{
	pShaderProg_->use();
	pShaderProg_->setMatrix4fv("view", 1, GL_FALSE, glm::value_ptr(view));
	pShaderProg_->setMatrix4fv("projection", 1, GL_FALSE, glm::value_ptr(projection));

	glBindVertexArray(VAO_);

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::translate(model, lightPos);
	model = glm::scale(model, glm::vec3(0.2f));

	pShaderProg_->setMatrix4fv("Model", 1, GL_FALSE, glm::value_ptr(model));
	pShaderProg_->setMatrix4fv("View", 1, GL_FALSE, glm::value_ptr(view));
	pShaderProg_->setMatrix4fv("Projection", 1, GL_FALSE, glm::value_ptr(projection));
	glDrawArrays(GL_TRIANGLES, 0, 36);

	glBindVertexArray(0);
	pShaderProg_->unuse();
}