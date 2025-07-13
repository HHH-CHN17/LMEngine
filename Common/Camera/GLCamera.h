#pragma once
#include <QVector3D>
#include <QKeyEvent>
#include <QObject>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement : uint8_t  {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

// Default camera values
constexpr float YAW = -90.0f;
constexpr float PITCH = 0.0f;
constexpr float SPEED = 10.0f;
constexpr float SENSITIVITY = 0.5f;
constexpr float ZOOM = 60.0f;

class CGLCamera : QObject
{
public:
    CGLCamera(QObject* parent, glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f) );
    ~CGLCamera();

    glm::mat4 getLookAt();
    void setMouseMove(float xoffset, float yoffset, bool constraintPitch = true);
    void setMouseScroll(float yoffset);
    void checkInputandUpdateCamera(qint64 dt);

    glm::vec3 position_;
    glm::vec3 cameraWorldUp;
    glm::vec3 cameraFront;

    glm::vec3 cameraUp_;
    glm::vec3 cameraRight;

    //Eular Angles
    float pitch_;
    float yaw_;

    //CGLCamera options
    float movementSpeed;
    float mouseSensitivity;
    float zoom_;

    //Keyboard multi-touch
    bool keys[1024] = { false };
private:
    void updateCamera();
    void setKeyPress(Camera_Movement direction, qint64 deltaTime);
};

