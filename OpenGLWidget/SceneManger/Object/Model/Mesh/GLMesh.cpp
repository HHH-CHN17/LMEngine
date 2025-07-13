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
	QImage rgba1 = imgContainer.rgbSwapped(); //qimage加载的颜色通道顺序和opengl显示的颜色通道顺序不一致,调换R通道和B通道，且会自动添加一个alpha通道
	// 本节的两张贴图均不需要反转
	//rgba1 = rgba1.mirrored();	// qimage的原点在左上角，UV坐标系的原点在左下角

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba1.width(), rgba1.height(),
		0, GL_RGBA, GL_UNSIGNED_BYTE, rgba1.bits());

	glGenerateMipmap(GL_TEXTURE_2D);

	return texture;
}

void CGLMesh::initialize(const QVector<VertexAttr>& vertices, const QVector<GLuint>& indices)
{
	initializeOpenGLFunctions();

	/*********************************** 初始化着色器程序对象 ***********************************/

	pShaderProg_->initialize(
		":/shaders/Resource/shaders/mesh.vs",
		":/shaders/Resource/shaders/mesh.fs"
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
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexAttr), vertices.constData(), GL_STATIC_DRAW);

	// 设置位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)0);
	glEnableVertexAttribArray(0);

	// 设置uv坐标
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// 设置法线坐标
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// 设置tangent坐标
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	// 设置bitangent坐标
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void*)(11 * sizeof(float)));
	glEnableVertexAttribArray(4);

	// ------------------------- 绑定texture -------------------------
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