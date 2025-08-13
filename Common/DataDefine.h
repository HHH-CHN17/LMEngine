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

// ------------------------- 音视频录制所需数据 -------------------------

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
    int     bit_rate = 2000000; // 默认 2 Mbps
}VideoCodecCfg;

typedef struct AudioCodecCfg {
    int64_t bit_rate_ = 128000; // 默认 128 kbps
    int     sample_rate_ = 48000;  // 默认 48 kHz
    int     channels_ = 2;    // 默认立体声
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
    // 通用设置（录制时为文件路径，推流时为RTMP/RTSP路径）
    std::string     path_;

    // 视频编码器参数
    VideoCodecCfg   videoCodecCfg_;

    // 音频编码器参数
    AudioCodecCfg   audioCodecCfg_;

    // 录音设备参数
    AudioFormat    audioFmt_;
}AVConfig;

// ------------------------- 音视频录制/推流异步改造 -------------------------

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
        }
    }
};

/*
 *  AVPacket生命流程：
 *  1. 由Encoder创建。
 *	2. 所有权转移给MediaPacket结构体。
 *	3. 所有权随着MediaPacket被std::move转移到队列中。
 *	4. Muxer从队列中取出MediaPacket，再次获得所有权。
 *	5. Muxer使用完毕后，MediaPacket被销毁，其拥有的AVPacket也应随之销毁。
 *	在以上流程中，AVPacket同一时间只会被一个消费者持有，不会被拷贝，AVPacket内部的引用计数始终为1，所以使用unique_ptr进行管理。
 *	在后续改造中，如果对于每一个AVPacket而言既需要录制，又需要推流，则需要改为shared_ptr，同时使用av_packet_ref增加引用计数。
 */

using AVPacketUPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

struct RawVideoFrame {
    std::vector<uint8_t> rgba_data;
    int width;
    int height;
	bool is_end_of_stream = false; // 用于标记是否为结束帧
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