#include "GLMesh.h"
#include <QDebug>
#include <QImage>

CGLMesh::CGLMesh(QObject* parent)
	: QObject(parent)
{
	pShaderProg_ = new GLShaderProgram{ this };
}

CGLMesh::~CGLMesh()
{
	
}

GLuint CGLMesh::loadTexture(const char* filePath)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	QImage imgContainer{ filePath };
	QImage rgba1 = imgContainer.rgbSwapped(); //qimage���ص���ɫͨ��˳���opengl��ʾ����ɫͨ��˳��һ��,����Rͨ����Bͨ�����һ��Զ����һ��alphaͨ��
	// ���ڵ�������ͼ������Ҫ��ת
	//rgba1 = rgba1.mirrored();	// qimage��ԭ�������Ͻǣ�UV����ϵ��ԭ�������½�

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
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexAttr), vertices.constData(), GL_STATIC_DRAW);

	// ����λ������
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);
	glEnableVertexAttribArray(0);

	// ����uv����
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// ���÷�������
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// ����tangent����
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	// ����bitangent����
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(11 * sizeof(float)));
	glEnableVertexAttribArray(4);

	// ------------------------- ��texture -------------------------
	pShaderProg_->use();
	pShaderProg_->set1i("frameTexture", 0);

	VAO_ = VAO;
	VBO_ = VBO;


}

void CGLMesh::draw(const glm::mat4& view, const glm::mat4& projection)
{
	
}

void CGLMesh::move(QPointF pos, float scale)
{
	
}