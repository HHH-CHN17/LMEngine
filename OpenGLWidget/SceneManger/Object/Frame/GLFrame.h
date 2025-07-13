#pragma once

#include <QObject>
#include <gl/GL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Common/ShaderProgram/GLShaderProgram.h"
#include "Common/DataDefine.h"

class CGLFrame : public QObject, public QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    explicit CGLFrame(QObject* parent = nullptr);
	~CGLFrame();

    void initialize();
    void draw(const glm::mat4& view, const glm::mat4& projection, const GLuint& FrameTexID);
    void move(const QPoint& pos, const float& scale);

private:
    GLShaderProgram* pShaderProg_ = nullptr;

    float vertices_[30] =
    {
		-1.0f, -1.0f, -0.5f,  0.0f, 0.0f,
        1.0f, -1.0f, -0.5f,  1.0f, 0.0f,
        1.0f,  1.0f, -0.5f,  1.0f, 1.0f,
        1.0f,  1.0f, -0.5f,  1.0f, 1.0f,
		-1.0f,  1.0f, -0.5f,  0.0f, 1.0f,
		-1.0f, -1.0f, -0.5f,  0.0f, 0.0f
    };

    //GLuint TexID_ = 0;
    GLuint VAO_ = 0;
    GLuint VBO_ = 0;
    GLuint EBO_ = 0;

	bool isMoving_ = false;
};
