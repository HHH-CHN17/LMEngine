#include "GLSkyBox.h"

#include <QImage>

CGLSkybox::CGLSkybox(QObject* parent)
	: QObject(parent)
{
	pShaderProg_ = new GLShaderProgram{ this };
}

CGLSkybox::~CGLSkybox()
{
	
}

void CGLSkybox::initialize()
{
	// 初始化
	initializeOpenGLFunctions();

	/*********************************** 初始化着色器程序对象 ***********************************/
	pShaderProg_->initialize(
		":/shaders/Resource/shaders/skyBox.vs",
		":/shaders/Resource/shaders/skyBox.fs"
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

	// 设置位置属性
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// ------------------------- 加载天空盒 -------------------------
	// 正x -> 负x -> 正y -> 负y -> 正z -> 负z
	QStringList texList{
		":/images/Resource/images/skyBox_right.jpg",
		":/images/Resource/images/skyBox_left.jpg",
		":/images/Resource/images/skyBox_top.jpg",
		":/images/Resource/images/skyBox_bottom.jpg",
		":/images/Resource/images/skyBox_front.jpg",
		":/images/Resource/images/skyBox_back.jpg"
	};
	skyBoxTexID_ = loadskyBoxTexture(texList);

	pShaderProg_->use();
	pShaderProg_->set1i("skybox", 0);	// 绑定纹理单元0
	pShaderProg_->unuse();

	VAO_ = VAO;
	VBO_ = VBO;
}

void CGLSkybox::draw(const glm::mat4& view, const glm::mat4& projection)
{
	pShaderProg_->use();
	glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
	
	glm::mat4 skyBoxView = glm::mat4(glm::mat3(view)); // remove translation from the view matrix
	pShaderProg_->setMatrix4fv("view", 1, GL_FALSE, glm::value_ptr(skyBoxView));
	pShaderProg_->setMatrix4fv("projection", 1, GL_FALSE, glm::value_ptr(projection));

	// skybox cube
	glBindVertexArray(VAO_);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyBoxTexID_);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

	glDepthFunc(GL_LESS); // set depth function back to default
	pShaderProg_->unuse();
}

GLuint CGLSkybox::loadskyBoxTexture(QStringList& fileList)
{
	GLuint texture = 0;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (int i = 0; i < fileList.size(); i++)
	{
		QImage imgContainer{ fileList[i] };
		QImage rgba1 = imgContainer.rgbSwapped(); //qimage加载的颜色通道顺序和opengl显示的颜色通道顺序不一致,调换R通道和B通道，且会自动添加一个alpha通道
		//rgba1 = rgba1.mirrored();	// 天空盒的原点在左上角，qimage的原点也在左上角，所以不需要翻转

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, rgba1.width(), rgba1.height(),
			0, GL_RGBA, GL_UNSIGNED_BYTE, rgba1.bits());

		//glGenerateMipmap(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i);
	}
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	return texture;
}