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
    void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& lightPos, const glm::vec3& viewPos);
    void move(const QPoint& pos, const float& scale);

private:
    void loadModel(const QString& filePath);
    void calculateTBN();
    void readvi(const QVector<VertexAttr>& vertices, const QVector<GLuint>& indices);

private:
    CGLMesh* pMesh_ = nullptr;

    // vertices[i]和indices[i]分别表示 一个顶点 的顶点属性（VBO）和索引（EBO）
    // QVector<T> is one of Qt's generic container classes. It stores its items in adjacent memory locations and provides fast index-based access.
    QVector<VertexAttr> vertices_{};
    QVector<GLuint> indices_{};
};
