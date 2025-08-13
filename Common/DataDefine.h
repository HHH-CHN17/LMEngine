#pragma once

#include <stdint.h>
#include <stdio.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <QDebug>
#include <QAudioFormat>
#include <memory>
#include <vector>

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/pixfmt.h>
#include <libavutil/samplefmt.h>
#include <libavcodec/avcodec.h>
}

// ------------------------- YUV��Ⱦ�������� -------------------------
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

// ------------------------- ģ����Ⱦ�������� -------------------------
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

// ------------------------- ����Ƶ¼���������� -------------------------

enum class avACT : uint8_t
{
    RECORD,
    RTMPPUSH,
    RTSPPUSH
};

typedef struct VideoCodecCfg {
    int     in_width_;
    int     in_height_;
    int     out_width_;
    int     out_height_;
    AVRational      framerate_;
    AVRational      time_base_;
    int     gop_size_;
    int     max_b_frames_;
    AVPixelFormat   pix_fmt_;
    int     flags_;
    AVCodecID       codec_id_;
    int     bit_rate = 2000000; // Ĭ�� 2 Mbps
}VideoCodecCfg;

typedef struct AudioCodecCfg {
    int64_t bit_rate_ = 128000; // Ĭ�� 128 kbps
    int     sample_rate_ = 48000;  // Ĭ�� 48 kHz
    int     channels_ = 2;    // Ĭ��������
    int64_t         channel_layout_;
    AVSampleFormat  sample_fmt_;
    AVRational      time_base_;
    int     flags_;
}AudioCodecCfg;

typedef struct AudioFormat {
    int     sample_rate_;
    int     channels_ = 2;
    int     sample_size_;
    QAudioFormat::SampleType    sample_fmt_;
    QAudioFormat::Endian        byte_order_;
    QString codec_;
}AudioFormat;

typedef struct AVConfig {
    // ͨ�����ã�¼��ʱΪ�ļ�·��������ʱΪRTMP/RTSP·����
    std::string     path_;

    // ��Ƶ����������
    VideoCodecCfg   videoCodecCfg_;

    // ��Ƶ����������
    AudioCodecCfg   audioCodecCfg_;

    // ¼���豸����
    AudioFormat    audioFmt_;
}AVConfig;

// ------------------------- ����Ƶ¼��/�����첽���� -------------------------

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }
};

/*
 *  AVPacket�������̣�
 *  1. ��Encoder������
 *	2. ����Ȩת�Ƹ�MediaPacket�ṹ�塣
 *	3. ����Ȩ����MediaPacket��std::moveת�Ƶ������С�
 *	4. Muxer�Ӷ�����ȡ��MediaPacket���ٴλ������Ȩ��
 *	5. Muxerʹ����Ϻ�MediaPacket�����٣���ӵ�е�AVPacketҲӦ��֮���١�
 *	�����������У�AVPacketͬһʱ��ֻ�ᱻһ�������߳��У����ᱻ������AVPacket�ڲ������ü���ʼ��Ϊ1������ʹ��unique_ptr���й���
 *	�ں��������У��������ÿһ��AVPacket���Լ���Ҫ¼�ƣ�����Ҫ����������Ҫ��Ϊshared_ptr��ͬʱʹ��av_packet_ref�������ü�����
 */

using AVPacketUPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct RawVideoFrame {
    std::vector<uint8_t> rgba_data;
    int width;
    int height;
	bool is_end_of_stream = false; // ���ڱ���Ƿ�Ϊ����֡
};

enum class PacketType : uint8_t
{
	VIDEO,
	AUDIO,
	END_OF_STREAM
};

struct MediaPacket {
    AVPacketUPtr pkt;
    PacketType type;
};