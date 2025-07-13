#include "GLCamera.h"
#include<QDebug>
#include "math.h"
CGLCamera::CGLCamera(QObject* parent, glm::vec3 position) :
	QObject(parent),
    position_(position),
    cameraWorldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
    cameraFront(glm::vec3(0.0f, 0.0f, -1.0f)),
    cameraUp_(cameraWorldUp),
    pitch_(PITCH),
    yaw_(YAW),
    movementSpeed(SPEED),
    mouseSensitivity(SENSITIVITY),
    zoom_(ZOOM)
{
    this->updateCamera();

}

CGLCamera::~CGLCamera()
{

}

// Returns the view matrix calculated using Euler Angles and the LookAt Matrix
glm::mat4 CGLCamera::getLookAt()
{
    glm::mat4 view;
    return glm::lookAt(this->position_, this->position_ + this->cameraFront, this->cameraUp_);
}

// Processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
void CGLCamera::setKeyPress(Camera_Movement direction, qint64 deltaTime)
{
    float sensitivity = this->movementSpeed * (deltaTime == 0 ? 1 : deltaTime) / 1000;
    if (direction == FORWARD)
        this->position_ += this->cameraFront * sensitivity;
    if (direction == BACKWARD)
        this->position_ -= this->cameraFront * sensitivity;
    if (direction == LEFT)
        this->position_ -= this->cameraRight * sensitivity;
    if (direction == RIGHT)
        this->position_ += this->cameraRight * sensitivity;
    if (direction == UP)
        this->position_ += this->cameraWorldUp * sensitivity;
    if (direction == DOWN)
        this->position_ -= this->cameraWorldUp * sensitivity;
}

// 其中yaw_指的是 摄像机镜头的方向 与x轴正方向的夹角，pitch_指的是 摄像机镜头的方向 与z轴负方向的夹角
void CGLCamera::setMouseMove(float xoffset, float yoffset, bool constraintPitch)
{
    xoffset *= this->mouseSensitivity;
    yoffset *= this->mouseSensitivity;

    yaw_ += xoffset;
    pitch_ += yoffset;

    if (constraintPitch) {
        pitch_ = std::min(pitch_, 89.0f);
        pitch_ = std::max(pitch_, -89.0f);
    }

    this->updateCamera();
}

// Processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
void CGLCamera::setMouseScroll(float yoffset)
{
    if (this->zoom_ >= 1.0f && this->zoom_ <= 120.0f)
        this->zoom_ -= yoffset;

    zoom_ = std::min(zoom_, 120.0f);
    zoom_ = std::max(zoom_, 1.0f);
    qDebug() << zoom_;
}

void CGLCamera::checkInputandUpdateCamera(qint64 dt)
{

    if (keys[Qt::Key_W])
        setKeyPress(FORWARD, dt);
    if (keys[Qt::Key_S])
        setKeyPress(BACKWARD, dt);
    if (keys[Qt::Key_A])
        setKeyPress(LEFT, dt);
    if (keys[Qt::Key_D])
        setKeyPress(RIGHT, dt);
    if (keys[Qt::Key_E])
        setKeyPress(UP, dt);
    if (keys[Qt::Key_Q])
        setKeyPress(DOWN, dt);
}

void CGLCamera::updateCamera()
{
    // Calculate the new Front vector
    glm::vec3 front;
    front.x = cos(glm::radians(pitch_)) * cos(glm::radians(yaw_));
    front.y = sin(glm::radians(pitch_));
    front.z = cos(glm::radians(pitch_)) * sin(glm::radians(yaw_));

    // 移动鼠标的时候，会改变摄像机坐标系的方向
    this->cameraFront = glm::normalize(front);
    this->cameraRight = glm::normalize(glm::cross(cameraFront, cameraWorldUp));
    this->cameraUp_ = glm::normalize(glm::cross(cameraRight, cameraFront));
}
