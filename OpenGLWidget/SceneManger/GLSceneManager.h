#pragma once

#include <QOpenGLExtraFunctions>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Common/DataDefine.h"
#include "OpenGLWidget/SceneManger/Object/Frame/GLFrame.h"
#include "OpenGLWidget/SceneManger/Object/Model/GLModel.h"
#include "OpenGLWidget/SceneManger/Object/SkyBox/GLSkyBox.h"

class GLSceneManager : public QObject, public QOpenGLExtraFunctions
{
	Q_OBJECT
public:
	GLSceneManager(QObject* parent = nullptr);
	~GLSceneManager();

public:
	void initialize();
	void draw(const glm::mat4& view, const glm::mat4& projection, const GLuint& FrameTexID, const glm::vec3& lightPos, const glm::vec3& viewPos);
	void moveFace(const QPoint& pos, const float& scale);

private:
	CGLSkybox* pSkyBox_ = nullptr;
	CGLFrame* pFrame_ = nullptr;
	CGLModel* pModel_ = nullptr;
};

