#pragma once

#include <QOpenGLExtraFunctions>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Common/YUVDataDefine.h"
#include "OpenGLWidget/SceneManger/Object/Frame/GLFrame.h"
#include "OpenGLWidget/SceneManger/Object/Model/GLModel.h"
#include "OpenGLWidget/SceneManger/Object/SkyBox/GLSkyBox.h"

class GLSceneManager : public QObject, public QOpenGLExtraFunctions
{
	Q_OBJECT
public:
	GLSceneManager(QObject* parent = nullptr);
	~GLSceneManager();

	void initialize();
	void draw(const glm::mat4& view, const glm::mat4& projection, const GLuint& FrameTexID);

private:
	CGLSkybox* pSkyBox_ = nullptr;
	CGLFrame* pFrame_ = nullptr;
	CGLModel* pModel_ = nullptr;
};

GLSceneManager::GLSceneManager(QObject* parent)
	: QObject(parent)
{
	pSkyBox_ = new CGLSkybox{ this };
	pFrame_ = new CGLFrame{ this };
	pModel_ = new CGLModel{ this };
}

GLSceneManager::~GLSceneManager()
{

}

void GLSceneManager::initialize()
{
	// 初始化
	initializeOpenGLFunctions();

	glEnable(GL_DEPTH_TEST);

	pFrame_->initialize();
	//pModel_->initialize(mainCtx_, ":/models/Resource/models/monkey.obj");
	pSkyBox_->initialize();
}

void GLSceneManager::draw(const glm::mat4& view, const glm::mat4& projection, const GLuint& FrameTexID)
{
	glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	pFrame_->draw(view, projection, FrameTexID);
	//pModel_->draw(view, projection);
	// 天空盒最后画
	pSkyBox_->draw(view, projection);
}