#pragma once

#include <stdint.h>
#include <stdio.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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