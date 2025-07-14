#pragma once

#include <QObject>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <gl/GL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Camera/GLCamera.h"
#include "Common/DataDefine.h"
#include "Common/ShaderProgram/GLShaderProgram.h"

class CGLMesh : public QObject, public QOpenGLExtraFunctions
{
public:
    CGLMesh(QObject* parent);
    virtual ~CGLMesh();

public:
    void initialize(const QVector<VertexAttr>& vertices, const QVector<GLuint>& indices);
    void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightPos, const glm::vec3& viewPos);
    void move(QPointF pos, float scale);

private:
    GLuint loadTexture(const char* filePath);

private:
    GLShaderProgram* pShaderProg_;

    GLuint VAO_ = 0;
	GLuint VBO_ = 0;
	GLuint EBO_ = 0;
	GLuint eboSize_ = 0;
    GLuint normalTexID_ = 0;
    GLuint diffuseTexID_ = 0;
	GLuint specularTexID_ = 0;

    glm::mat4 model_{ 1.0f };

};
