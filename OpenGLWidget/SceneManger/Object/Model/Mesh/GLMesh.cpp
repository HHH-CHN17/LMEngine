#include "GLMesh.h"
#include <QDebug>
#include <QImage>

CGLMesh::CGLMesh(QObject* parent)
	: QObject(parent)
{
	pShaderProg_ = new GLShaderProgram{ this };
	model_ = glm::mat4(1.0f);
	model_ = glm::translate(model_, glm::vec3(0.0f, 0.0f, -1.0f));
	model_ = glm::scale(model_, glm::vec3(0.33, 0.33 * 4 / 3, 0.33));
}

CGLMesh::~CGLMesh()
{
	
}

GLuint CGLMesh::loadTexture(const char* filePath)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	QImage imgContainer{ filePath };
	QImage rgba1 = imgContainer.rgbSwapped(); //qimage���ص���ɫͨ��˳���opengl��ʾ����ɫͨ��˳��һ��,����Rͨ����Bͨ�����һ��Զ����һ��alphaͨ��
	// ���ڵ�������ͼ������Ҫ��ת
	rgba1 = rgba1.mirrored();	// qimage��ԭ�������Ͻǣ�UV����ϵ��ԭ�������½�

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba1.width(), rgba1.height(),
		0, GL_RGBA, GL_UNSIGNED_BYTE, rgba1.bits());

	glGenerateMipmap(GL_TEXTURE_2D);

	return texture;
}

void CGLMesh::initialize(const QVector<VertexAttr>& vertices, const QVector<GLuint>& indices)
{
	initializeOpenGLFunctions();

	/*********************************** ��ʼ����ɫ��������� ***********************************/

	pShaderProg_->initialize(
		":/shaders/Resource/shaders/mesh.vs",
		":/shaders/Resource/shaders/mesh.fs"
	);

	/*********************************** ��ʼ��VAO,VBO,EBO,texture ***********************************/
	// ------------------------- VAO -------------------------
	GLuint VAO = 0;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// ------------------------- VBO -------------------------
	GLuint VBO = 0;
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	// �����ݴ�������
	// QVector<T> is one of Qt's generic container classes. It stores its items in adjacent memory locations and provides fast index-based access.
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexAttr), vertices.constData(), GL_STATIC_DRAW);

	// ����λ������
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)0);
	glEnableVertexAttribArray(0);

	// ���÷�������
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// ����uv����
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// ����tangent����
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	// ����bitangent����
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(11 * sizeof(float)));
	glEnableVertexAttribArray(4);

	// ------------------------- EBO -------------------------
	GLuint EBO = 0;
	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	eboSize_ = indices.size();
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, eboSize_ * sizeof(GLuint), indices.constData(), GL_STATIC_DRAW);

	// ------------------------- texture -------------------------
	normalTexID_ = loadTexture(":/models/Resource/models/monkey_normal.png");
	diffuseTexID_ = loadTexture(":/models/Resource/models/monkey_diffuse.png");
	specularTexID_ = loadTexture(":/models/Resource/models/monkey_specular.png");

	// ------------------------- ��texture -------------------------
	pShaderProg_->use();
	pShaderProg_->set1i("material.diffuse", 0);
	pShaderProg_->set1i("material.specular", 1);
	pShaderProg_->set1i("material.normal", 2);
	pShaderProg_->unuse();

	VAO_ = VAO;
	VBO_ = VBO;
	EBO_ = EBO;

}

void CGLMesh::draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightPos, const glm::vec3& viewPos)
{
	pShaderProg_->use();
	glBindVertexArray(VAO_);
	// ------------------------- �������������Ϣ -------------------------
	// ��������texture����0�Ų�����
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, diffuseTexID_);
	// �����淴��texture����1�Ų�����
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, specularTexID_);
	// ��������ͼtexture����2�Ų�����
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, normalTexID_);
	pShaderProg_->set1f("material.shininess", 32.0f);

	// ------------------------- ���õƹ���Ϣ -------------------------
	pShaderProg_->setVec3("light.position", lightPos);
	pShaderProg_->setVec3("light.ambient", { 0.2f, 0.2f, 0.2f });
	pShaderProg_->setVec3("light.diffuse", { 1.0f, 1.0f, 1.0f }); // �����յ�����һЩ�Դ��䳡��
	pShaderProg_->setVec3("light.specular", { 0.2f, 0.2f, 0.2f });

	// ------------------------- ���������λ�� -------------------------
	pShaderProg_->setVec3("viewPos", viewPos);

	// ------------------------- ����MVP���� -------------------------

    pShaderProg_->setMatrix4fv("Model", 1, GL_FALSE, glm::value_ptr(model_));
	pShaderProg_->setMatrix4fv("View", 1, GL_FALSE, glm::value_ptr(view));
	pShaderProg_->setMatrix4fv("Projection", 1, GL_FALSE, glm::value_ptr(projection));

	glDrawElements(GL_TRIANGLES, eboSize_, GL_UNSIGNED_INT, 0);

	glBindVertexArray(0);
	pShaderProg_->unuse();
}

void CGLMesh::move(QPointF pos, float scale)
{
	float x = 1.0f - pos.x() * 2.0 / 1920;
	float y = 1.0f - pos.y() * 2.0 / 1080;
	//qDebug() << "move pos: " << x << " " << y;
	scale = std::max(scale, 2.0f);

	model_ = glm::mat4(1.0f);
	model_ = glm::translate(model_, glm::vec3(x, y, -0.0f));
	model_ = glm::scale(model_, glm::vec3(0.1 * scale, 0.1 * 4 / 3 * scale, 0.1));
	//model_ = glm::scale(model_, glm::vec3(0.33, 0.33, 0.33));
}
