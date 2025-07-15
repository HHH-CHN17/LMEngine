#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <QOpenGLFunctions>
#include <QOpenGLContext>
#include <gl/GL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <QOpenGLExtraFunctions>

class GLShaderProgram : public QObject, public QOpenGLExtraFunctions
{
	//Q_OBJECT
public:

	// 着色器程序对象ID
	GLuint ProgramID_ = 0;

	// 构造顶点着色器和片段着色器
	explicit GLShaderProgram(QObject* parent);
	~GLShaderProgram();

public:
	void initialize(const char* vertexPath, const char* fragmentPath);
	// 使用/激活程序
	void use();
	void unuse();
	// 设置uniform值
	void set1b(const std::string& name, bool value);
	void set1i(const std::string& name, int value);
	void set1f(const std::string& name, float value);
	void set4f(const std::string& name, float value1, float value2, float value3, float value4);
	void set2f(const std::string& name, float value1, float value2);
	void setMatrix4fv(const std::string& name, GLsizei count, GLboolean transpose, const GLfloat* value);
	void setVec3(const std::string& name, const glm::vec3& value);
};

