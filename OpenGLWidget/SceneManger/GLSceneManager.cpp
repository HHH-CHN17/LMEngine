#include "GLSceneManager.h"

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
	// ��ʼ��
	initializeOpenGLFunctions();

	glEnable(GL_DEPTH_TEST);

	pFrame_->initialize();
	pModel_->initialize(":/models/Resource/models/monkey.obj");
	pSkyBox_->initialize();
}

void GLSceneManager::draw(const glm::mat4& view, const glm::mat4& projection, const GLuint& FrameTexID)
{
	pFrame_->draw(view, projection, FrameTexID);
	pModel_->draw(view, projection);
	// ��պ����
	pSkyBox_->draw(view, projection);
}

void GLSceneManager::moveFace(const QPoint& pos, const float& scale)
{
	if (pModel_)
	{
		pModel_->move(pos, scale);
	}

	if (pFrame_)
	{
		pFrame_->move(pos, scale);
	}
}