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
	QImage rgba1 = imgContainer.rgbSwapped(); //qimage加载的颜色通道顺序和opengl显示的颜色通道顺序不一致,调换R通道和B通道，且会自动添加一个alpha通道
	// 本节的两张贴图均不需要反转
	rgba1 = rgba1.mirrored();	// qimage的原点在左上角，UV坐标系的原点在左下角

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
	// QVector<T> is one of Qt's generic container classes. It stores its items in adjacent memory locations and provides fast index-based access.
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexAttr), vertices.constData(), GL_STATIC_DRAW);

	// 设置位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)0);
	glEnableVertexAttribArray(0);

	// 设置法线坐标
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// 设置uv坐标
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	// 设置tangent坐标
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VertexAttr), (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	// 设置bitangent坐标
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

	// ------------------------- 绑定texture -------------------------
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
	// ------------------------- 设置物体材质信息 -------------------------
	// 将漫反射texture传给0号采样器
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, diffuseTexID_);
	// 将镜面反射texture传给1号采样器
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, specularTexID_);
	// 将法线贴图texture传给2号采样器
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, normalTexID_);
	pShaderProg_->set1f("material.shininess", 32.0f);

	// ------------------------- 设置灯光信息 -------------------------
	pShaderProg_->setVec3("light.position", lightPos);
	pShaderProg_->setVec3("light.ambient", { 0.2f, 0.2f, 0.2f });
	pShaderProg_->setVec3("light.diffuse", { 1.0f, 1.0f, 1.0f }); // 将光照调暗了一些以搭配场景
	pShaderProg_->setVec3("light.specular", { 0.2f, 0.2f, 0.2f });

	// ------------------------- 设置摄像机位置 -------------------------
	pShaderProg_->setVec3("viewPos", viewPos);

	// ------------------------- 设置MVP矩阵 -------------------------

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
