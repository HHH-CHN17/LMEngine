QT       += core gui multimedia opengl widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += resources_big

CONFIG(release_with_debug, release|debug) {
    # 继承 release 配置
    CONFIG += release

    # 移除 Qt 对 DEBUG 宏的定义，使其行为更像一个真正的 release 版本
    DEFINES -= QT_DEBUG

    # 添加调试符号
    QMAKE_CXXFLAGS += -g
    QMAKE_CFLAGS += -g
    QMAKE_LFLAGS += -g

    # (可选) 调整优化级别
    # QMAKE_CXXFLAGS -= -O2
    # QMAKE_CXXFLAGS += -Og
}

LIBS += -lOpenGL32 -lglu32

INCLUDEPATH += $$PWD/lib/win32/libglew/include
LIBS += $$PWD/lib/win32/libglew/lib/glew32.lib

INCLUDEPATH += $$PWD/lib/win32/libOpenCV/include

LIBS += -L$$PWD/lib/win32/libOpenCV/lib \
    -llibopencv_core331 \
    -llibopencv_highgui331 \
    -llibopencv_imgcodecs331 \
    -llibopencv_imgproc331 \
    -llibopencv_features2d331 \
    -llibopencv_calib3d331 \
    -llibopencv_video331 \
    -llibopencv_videoio331 \
    -llibopencv_videostab331 \
    -llibopencv_objdetect331

INCLUDEPATH += $$PWD/lib/win32/libfacelandmark/include
LIBS += -L$$PWD/lib/win32/libfacelandmark/lib -lfacelandmark

LIBS += -L$$PWD/lib/win32/libx264/lib -lfdk-aac -lx264

INCLUDEPATH += $$PWD/lib/win32/libFFmpeg/include
LIBS +=     $$PWD/lib/win32/libFFmpeg/lib/libavformat.dll.a \
            $$PWD/lib/win32/libFFmpeg/lib/libavcodec.dll.a \
            $$PWD/lib/win32/libFFmpeg/lib/libavutil.dll.a \
            $$PWD/lib/win32/libFFmpeg/lib/libswresample.dll.a \
            $$PWD/lib/win32/libFFmpeg/lib/libswscale.dll.a \
            $$PWD/lib/win32/libFFmpeg/lib/libpostproc.dll.a \
            $$PWD/lib/win32/libFFmpeg/lib/libavfilter.dll.a

INCLUDEPATH += $$PWD/lib/win32/libfaac/include
LIBS += -L$$PWD/lib/win32/libfaac/lib -lfaac


SOURCES += \
    ./main.cpp \
    ./MainWidget.cpp \
    ./OpenGLWidget/OpenGLWidget.cpp \
    ./OpenGLWidget/SceneManger/GLSceneManager.cpp \
    ./OpenGLWidget/SceneManger/Object/Frame/GLFrame.cpp \
    ./OpenGLWidget/SceneManger/Object/Model/GLModel.cpp \
    ./OpenGLWidget/SceneManger/Object/Model/Mesh/GLMesh.cpp \
    ./OpenGLWidget/SceneManger/Object/SkyBox/GLSkyBox.cpp \
    ./OpenGLWidget/SceneManger/Object/Sun/GLSun.cpp \
    ./OpenGLWidget/VideoCaptureThread/VideoCaptureThread.cpp \
    ./OpenGLWidget/VideoCaptureThread/YUVDraw/GLYuvDraw.cpp \
    ./AVRecorder/AVRecorder.cpp \
    ./AVRecorder/Muxer/Muxer.cpp \
    ./AVRecorder/AudioEncoder/AudioEncoder.cpp \
    ./AVRecorder/VideoEncoder/VideoEncoder.cpp \
    ./AVRecorder/AudioCapturer/AudioCapturer.cpp \
    ./AVRecorder/AudioCapturer/IOBuffer/IOBuffer.cpp \
    ./Common/Camera/GLCamera.cpp \
    ./Common/ShaderProgram/GLShaderProgram.cpp

INCLUDEPATH += ./Common
INCLUDEPATH += ./Common/Camera
INCLUDEPATH += ./Common/ShaderProgram
INCLUDEPATH += ./OpenGLWidget
INCLUDEPATH += ./OpenGLWidget/VideoCaptureThread
INCLUDEPATH += ./OpenGLWidget/VideoCaptureThread/YUVDraw
INCLUDEPATH += ./OpenGLWidget/SceneManger
INCLUDEPATH += ./OpenGLWidget/SceneManger/Object
INCLUDEPATH += ./OpenGLWidget/SceneManger/Object/Frame
INCLUDEPATH += ./OpenGLWidget/SceneManger/Object/Model
INCLUDEPATH += ./OpenGLWidget/SceneManger/Object/Model/Mesh
INCLUDEPATH += ./OpenGLWidget/SceneManger/Object/SkyBox
INCLUDEPATH += ./OpenGLWidget/SceneManger/Object/Sun
INCLUDEPATH += ./AVRecorder
INCLUDEPATH += ./AVRecorder/Muxer
INCLUDEPATH += ./AVRecorder/AudioEncoder
INCLUDEPATH += ./AVRecorder/VideoEncoder
INCLUDEPATH += ./AVRecorder/AudioCapturer
INCLUDEPATH += ./AVRecorder/AudioCapturer/IOBuffer

HEADERS += \
    ./MainWidget.h \
    ./OpenGLWidget/OpenGLWidget.h \
    ./OpenGLWidget/SceneManger/GLSceneManager.h \
    ./OpenGLWidget/SceneManger/Object/Frame/GLFrame.h \
    ./OpenGLWidget/SceneManger/Object/Model/GLModel.h \
    ./OpenGLWidget/SceneManger/Object/Model/Mesh/GLMesh.h \
    ./OpenGLWidget/SceneManger/Object/SkyBox/GLSkyBox.h \
    ./OpenGLWidget/SceneManger/Object/Sun/GLSun.h \
    ./OpenGLWidget/VideoCaptureThread/VideoCaptureThread.h \
    ./OpenGLWidget/VideoCaptureThread/YUVDraw/GLYuvDraw.h \
    ./AVRecorder/AVRecorder.h \
    ./AVRecorder/Muxer/Muxer.h \
    ./AVRecorder/AudioEncoder/AudioEncoder.h \
    ./AVRecorder/VideoEncoder/VideoEncoder.h \
    ./AVRecorder/AudioCapturer/AudioCapturer.h \
    ./AVRecorder/AudioCapturer/IOBuffer/IOBuffer.h \
    ./Common/ShaderProgram/GLShaderProgram.h \
    ./Common/Camera/GLCamera.h \
    ./Common/DataDefine.h \
    ./Common/MsgQueue.h

FORMS += \
    ./MainWidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    ./LMEngine.qrc
