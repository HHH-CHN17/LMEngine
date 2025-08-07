#include "RtmpPush.h"
#include <cstring> // For memcpy, memset
#include <iostream> // For optional logging, can be replaced with your logger
#include <QDebug>

// --- 辅助宏 ---
#define RTMP_CHANNEL_VIDEO 0x04
#define RTMP_CHANNEL_AUDIO 0x05
//#define RTMP_PACKET_SIZE_MEDIUM RTMP_PACKET_SIZE_MEDIUM
//#define RTMP_PACKET_SIZE_LARGE RTMP_PACKET_SIZE_LARGE

CRtmpPush::CRtmpPush(int log_level) : rtmpPtr_(nullptr, &RTMP_Free), isConnected_(false), sps_pps_sent_(false) {
    // 初始化 librtmp 日志级别
    RTMP_LogSetLevel(static_cast<RTMP_LogLevel>(log_level));
    // RTMP_LogSetOutput(stdout); // 可选：设置 librtmp 日志输出到 stdout
}

CRtmpPush::~CRtmpPush() {
    disconnect(); // 确保在销毁时断开连接
}

bool CRtmpPush::connect(const char* rtmp_url) {
    if (isConnected_) {
        qCritical() << "Already connected.";
        return true; // 或者返回 false，取决于策略
    }

    // 分配 RTMP 对象
    RTMP* rtmp_raw = RTMP_Alloc();
    if (!rtmp_raw) {
        qCritical() << "Failed to allocate RTMP object.";
        return false;
    }

    // 使用 unique_ptr 管理生命周期
    rtmpPtr_.reset(rtmp_raw);

    // 初始化 RTMP 对象
    RTMP_Init(rtmp_raw);
    rtmp_raw->Link.timeout = 10; // 设置超时时间

    // 设置推流 URL
    if (!RTMP_SetupURL(rtmp_raw, const_cast<char*>(rtmp_url))) {
        qCritical() << "Failed to setup RTMP URL: " << rtmp_url;
        rtmpPtr_.reset(); // 重置指针，触发 RTMP_Free
        return false;
    }

    // 启用写入模式 (推流)
    RTMP_EnableWrite(rtmp_raw);

    // 建立连接
    if (!RTMP_Connect(rtmp_raw, nullptr)) {
        qCritical() << "Failed to connect to RTMP server.";
        rtmpPtr_.reset();
        return false;
    }

    // 创建流
    if (!RTMP_ConnectStream(rtmp_raw, 0)) {
        qCritical() << "Failed to connect to RTMP stream.";
        RTMP_Close(rtmp_raw); // 关闭连接
        rtmpPtr_.reset();
        return false;
    }

    isConnected_ = true;
    sps_pps_sent_ = false; // 重置 SPS/PPS 发送状态
    sps_.clear();
    pps_.clear();
    qDebug() << "Connected to RTMP server: " << rtmp_url;
    return true;
}

void CRtmpPush::disconnect() {
    if (rtmpPtr_ && isConnected_) {
        RTMP_Close(rtmpPtr_.get());
        // rtmpPtr_.reset() 会在作用域结束或显式调用时调用 RTMP_Free
        qDebug() << "Disconnected from RTMP server.";
    }
    isConnected_ = false;
    sps_pps_sent_ = false;
    sps_.clear();
    pps_.clear();
}

bool CRtmpPush::isConnected() const {
    return isConnected_ && rtmpPtr_ && RTMP_IsConnected(rtmpPtr_.get());
}

bool CRtmpPush::sendVideo(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe) {
    if (!isConnected() || !data || len == 0) {
        return false;
    }

    // 简单判断是否为 SPS/PPS (假设在一个包里，且以 SPS 开头)
    // 更健壮的做法是检查 NALU 类型 (data[0] & 0x1F)
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) {
        // 查找第二个起始码来分离 SPS 和 PPS
        const uint8_t* sps_start = data;
        size_t sps_len = 0;
        const uint8_t* pps_start = nullptr;
        size_t pps_len = 0;

        // 查找第二个 0x00000001
        for (size_t i = 4; i < len - 3; ++i) {
            if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
                sps_len = i; // SPS 长度到第二个起始码为止
                pps_start = data + i + 4; // PPS 从第二个起始码之后开始
                pps_len = len - (i + 4);
                break;
            }
        }

        // 如果找到了 PPS
        if (pps_start && sps_len > 4 && pps_len > 0) {
            // 去掉起始码
            sps_.assign(sps_start + 4, sps_start + sps_len);
            pps_.assign(pps_start + 4, pps_start + pps_len);
            // qDebug() << "SPS and PPS extracted and cached.";
            // 这里可以选择立即发送 SPS/PPS，或者等第一个 IDR 帧再发
            // 为了简化，我们在这里处理，但实际应用中可能在收到 IDR 时发送
            // handleSpsPps(data, len); // 也可以调用这个函数处理
            return true; // SPS/PPS 缓存成功
        }
    }

    // 如果是关键帧且 SPS/PPS 还没发送，则先发送
    if (is_keyframe && !sps_pps_sent_ && !sps_.empty() && !pps_.empty()) {
        // 创建并发送 AVC Sequence Header (包含 SPS 和 PPS)
        RTMPPacket* sps_pps_packet = createVideoPacket(nullptr, 0, 0, true); // 特殊调用，data=nullptr 表示发送 SPS/PPS
        if (sps_pps_packet) {
            bool res = sendPacket(sps_pps_packet);
            if (res) {
                sps_pps_sent_ = true;
                // qDebug() << "SPS/PPS packet sent.";
            }
            // sendPacket 内部会调用 RTMPPacket_Free
            // free(sps_pps_packet); // 不需要，因为 sendPacket 已经处理
            if (!res) return false;
        }
    }

    // 创建并发送普通视频帧数据包
    RTMPPacket* video_packet = createVideoPacket(data, len, dts, is_keyframe);
    if (video_packet) {
        bool res = sendPacket(video_packet);
        // sendPacket 内部会调用 RTMPPacket_Free
        // free(video_packet); // 不需要
        return res;
    }
    return false;
}


bool CRtmpPush::sendAudio(const uint8_t* data, size_t len, uint32_t dts, bool is_sequence_header) {
    if (!isConnected() || !data || len == 0) {
        return false;
    }

    RTMPPacket* audio_packet = createAudioPacket(data, len, dts, is_sequence_header);
    if (audio_packet) {
        bool res = sendPacket(audio_packet);
        // sendPacket 内部会调用 RTMPPacket_Free
        // free(audio_packet); // 不需要
        return res;
    }
    return false;
}

// --- Private Helper Functions ---

bool CRtmpPush::sendPacket(RTMPPacket* packet, int queue) {
    if (!packet || !isConnected()) {
        return false;
    }
    // RTMP_SendPacket 返回 1 表示成功，0 表示失败
    int result = RTMP_SendPacket(rtmpPtr_.get(), packet, queue);
    RTMPPacket_Free(packet); // 发送后立即释放 packet 内存
    free(packet);            // 释放 packet 结构体本身 (由 createPacket 分配)
    return result == 1;
}


bool CRtmpPush::handleSpsPps(const uint8_t* data, size_t len) {
    // 此函数的逻辑已在 sendVideo 中实现
    // 这里保留是为了接口完整性，如果需要独立处理可以在此实现
    // ...
    return true;
}

RTMPPacket* CRtmpPush::createVideoPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe) {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return nullptr;
    RTMPPacket_Reset(packet);

    // 特殊情况：发送 SPS/PPS
    if (data == nullptr && is_keyframe && !sps_.empty() && !pps_.empty()) {
        // 计算包体大小
        size_t body_size = 11 + sps_.size() + 1 + 2 + pps_.size() + 2; // 11: header, sps_len, pps_len 各占字节
        RTMPPacket_Alloc(packet, body_size);

        uint8_t* body = reinterpret_cast<uint8_t*>(packet->m_body);
        int i = 0;
        body[i++] = 0x17; // FrameType (1=KeyFrame) + CodecID (7=AVC)
        body[i++] = 0x00; // AVCPacketType = 0 (Sequence Header)
        body[i++] = 0x00; body[i++] = 0x00; body[i++] = 0x00; // CompositionTime = 0

        // AVCDecoderConfigurationRecord
        body[i++] = 0x01; // configurationVersion
        body[i++] = sps_[1]; // AVCProfileIndication
        body[i++] = sps_[2]; // profile_compatibility
        body[i++] = sps_[3]; // AVCLevelIndication
        body[i++] = 0xFF; // lengthSizeMinusOne (3 bytes for length)

        // SPS
        body[i++] = 0xE1; // numOfSequenceParameterSets (high 3 bits reserved, low 5 bits = 1)
        body[i++] = (sps_.size() >> 8) & 0xFF; // sequenceParameterSetLength high
        body[i++] = sps_.size() & 0xFF;        // sequenceParameterSetLength low
        memcpy(body + i, sps_.data(), sps_.size());
        i += sps_.size();

        // PPS
        body[i++] = 0x01; // numOfPictureParameterSets
        body[i++] = (pps_.size() >> 8) & 0xFF; // pictureParameterSetLength high
        body[i++] = pps_.size() & 0xFF;        // pictureParameterSetLength low
        memcpy(body + i, pps_.data(), pps_.size());
        i += pps_.size();

        packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
        packet->m_nBodySize = i; // 实际写入的大小
        packet->m_nChannel = RTMP_CHANNEL_VIDEO;
        packet->m_nTimeStamp = dts; // SPS/PPS 通常时间戳为 0，但这里传入
        packet->m_hasAbsTimestamp = 0;
        packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM; // 或 LARGE
        packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

        return packet;
    }

    // 普通视频帧
    size_t body_size = 9 + len; // 9 bytes for FLV VideoTagHeader + NALU length header
    RTMPPacket_Alloc(packet, body_size);

    uint8_t* body = reinterpret_cast<uint8_t*>(packet->m_body);
    int i = 0;
    body[i++] = is_keyframe ? 0x17 : 0x27; // FrameType + CodecID
    body[i++] = 0x01; // AVCPacketType = 1 (NALU)
    body[i++] = 0x00; body[i++] = 0x00; body[i++] = 0x00; // CompositionTime = 0

    // NALU Length (Big Endian)
    body[i++] = (len >> 24) & 0xFF;
    body[i++] = (len >> 16) & 0xFF;
    body[i++] = (len >> 8) & 0xFF;
    body[i++] = len & 0xFF;

    // NALU Data
    if (len > 0) {
        memcpy(body + i, data, len);
    }

    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nBodySize = body_size;
    packet->m_nChannel = RTMP_CHANNEL_VIDEO;
    packet->m_nTimeStamp = dts;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    return packet;
}


RTMPPacket* CRtmpPush::createAudioPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_sequence_header) {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return nullptr;
    RTMPPacket_Reset(packet);

    size_t body_size = (is_sequence_header ? 2 : 1) + len; // 2 bytes header for sequence, 1 for raw data
    RTMPPacket_Alloc(packet, body_size);

    uint8_t* body = reinterpret_cast<uint8_t*>(packet->m_body);
    int i = 0;
    body[i++] = 0xAF; // SoundFormat (10=AAC) + SoundRate (3=44kHz) + SoundSize (1=16-bit) + SoundType (1=Stereo)
    body[i++] = is_sequence_header ? 0x00 : 0x01; // AACPacketType: 0=Sequence Header, 1=Raw Data

    if (len > 0) {
        memcpy(body + i, data, len);
    }

    packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet->m_nBodySize = body_size;
    packet->m_nChannel = RTMP_CHANNEL_AUDIO;
    packet->m_nTimeStamp = dts;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    return packet;
}