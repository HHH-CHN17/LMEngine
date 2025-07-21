#pragma once

#include <stdint.h>
#include <stdio.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <libavutil/log.h>
#include <QDebug>

// ------------------------- YUV渲染所需数据 -------------------------
#pragma pack(push, 1)
typedef struct framePlaneDef
{
    unsigned int    length;
    unsigned char* dataBuffer;

}FramePlane;

typedef struct  YUVDataDef
{
    unsigned int    width;
    unsigned int    height;
    FramePlane       luma;
    FramePlane       chromaB;
    FramePlane       chromaR;

}YUVFrame;

typedef struct  YUVBuffDef
{
    unsigned int    width;
    unsigned int    height;
    unsigned int    ylength;
    unsigned int    ulength;
    unsigned int    vlength;
    unsigned char*  buffer;

}YUVBuffer;
#pragma pack(pop)

// ------------------------- 模型渲染所需数据 -------------------------
#pragma pack(push, 4)
typedef struct vec3f
{
    float x;
    float y;
    float z;
}vec3f;

typedef struct vec2f
{
    float x;
    float y;
}vec2f;

typedef struct VertexAttr
{
    vec3f pos;
    vec3f norm;
    vec2f uv;
    vec3f tan;
    vec3f bitan;

}VertexAttr;
static_assert(std::is_pod_v<VertexAttr>, "VertexAttr is not POD!!");
#pragma pack(pop)

// ------------------------- 视频录制所需数据 -------------------------
typedef struct VideoConfig {
    int inWidth;
    int inHeight;
    int outWidth;
    int outHeight;
    int framerate;
    int bitrate = 2000000; // 默认 2 Mbps
}VideoConfig;

typedef struct AudioConfig {
    int audio_sample_rate = 48000;  // 默认 48 kHz
    int audio_channel_count = 2;    // 默认立体声
    int audio_bitrate = 128000; // 默认 128 kbps
}AudioConfig;

typedef struct AVConfig {
    // 通用设置
    std::string filePath;

    // 视频参数
    VideoConfig videoCfg;

    // 音频参数 (新增)
    AudioConfig audioCfg;
}AVConfig;