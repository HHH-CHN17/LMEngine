#include "GLShaderProgram.h"
#include <QDebug>
#include <QFile>
#include <QByteArray>
#include <QString>


GLShaderProgram::GLShaderProgram(QObject* parent)
	: QObject(parent)
{
    
}

void GLShaderProgram::initialize(const char* vertexPath, const char* fragmentPath)
{
    initializeOpenGLFunctions();

    std::string vertexCode{};
    std::string fragmentCode{};
    QFile vShaderFile{ vertexPath };
    QFile fShaderFile{ fragmentPath };
    if (!vShaderFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "ERROR::VSHADER::FILE_NOT_SUCCESFULLY_READ";
    }
    vertexCode = vShaderFile.readAll().toStdString();

    if (!fShaderFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "ERROR::VSHADER::FILE_NOT_SUCCESFULLY_READ";
    }
    fragmentCode = fShaderFile.readAll().toStdString();

    auto strVertexShaderSource = vertexCode.c_str();
    GLuint vertexShader = 0;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &strVertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        qDebug() << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog;
    }

    auto strFragmentShaderSource = fragmentCode.c_str();
    GLuint fragmentShader = 0;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &strFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    int success1;
    char infoLog1[512];
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success1);
    if (!success1)
    {
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog1);
        qDebug() << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << QString(infoLog1);
    }

    GLuint program = 0;
    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    int success2;
    char infoLog2[512];
    glGetProgramiv(program, GL_LINK_STATUS, &success2);
    if (!success2) {
        glGetProgramInfoLog(program, 512, nullptr, infoLog2);
        qDebug() << "ERROR::PROGRAM::LINK_FAILED\n" << QString(infoLog2);
    }

    ProgramID_ = program;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

GLShaderProgram::~GLShaderProgram()
{
    glDeleteProgram(ProgramID_);
}

void GLShaderProgram::use()
{
    glUseProgram(ProgramID_);
}
// 设置uniform值
void GLShaderProgram::set1b(const std::string& name, bool value)
{
    // 1.在着色器程序对象中查询对应uniform的位置（查询时不要求使用过着色器程序）
    GLint loc = glGetUniformLocation(ProgramID_, name.c_str());
    // 2.根据idx设置uniform的值（设置时要求必须已经使用过着色器程序）
    glUniform1i(loc, value);
}
void GLShaderProgram::set1i(const std::string& name, int value)
{
    GLint loc = glGetUniformLocation(ProgramID_, name.c_str());
    glUniform1i(loc, value);
}
void GLShaderProgram::set1f(const std::string& name, float value)
{
    GLint loc = glGetUniformLocation(ProgramID_, name.c_str());
    glUniform1f(loc, value);
}
void GLShaderProgram::set4f(const std::string& name, float value1, float value2, float value3, float value4)
{
    GLint loc = glGetUniformLocation(ProgramID_, name.c_str());
    glUniform4f(loc, value1, value2, value3, value4);
}

void GLShaderProgram::set2f(const std::string& name, float value1, float value2)
{
    GLint loc = glGetUniformLocation(ProgramID_, name.c_str());
    glUniform2f(loc, value1, value2);
}

void GLShaderProgram::setMatrix4fv(const std::string& name, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    GLint loc = glGetUniformLocation(ProgramID_, name.c_str());
    glUniformMatrix4fv(loc, count, transpose, value);
}