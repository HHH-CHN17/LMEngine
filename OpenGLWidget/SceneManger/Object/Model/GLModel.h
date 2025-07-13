#pragma once

#include <QObject>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <gl/GL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <QPoint>
#include <glm/gtc/type_ptr.hpp>
#include "Common/DataDefine.h"
#include "Common/ShaderProgram/GLShaderProgram.h"
#include "Mesh/GLMesh.h"

class CGLModel : public QObject, public QOpenGLExtraFunctions
{

public:
    explicit CGLModel(QObject* parent = nullptr);
    virtual ~CGLModel();

    void initialize(const QString& fileName);
    void draw(const glm::mat4& view, const glm::mat4& projection);
    void move(const QPoint& pos, const float& scale);

private:
    void loadModel(const QString& filePath);
    void calculateTBN();


private:
    CGLMesh* pMesh_ = nullptr;

    // vertices[i]��indices[i]�ֱ��ʾ һ������ �Ķ������ԣ�VBO����������EBO��
    QVector<VertexAttr> vertices_{};
    QVector<GLuint> indices_{};
};
