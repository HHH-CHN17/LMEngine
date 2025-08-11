#include "RtmpPush.h"
#include <cstring> // For memcpy, memset
#include <iostream> // For logging, can be replaced with qDebug etc.
#include <algorithm> // For std::max
#include <QDebug>

// --- 定义 RTMP 通道和包大小 ---
#define RTMP_CHANNEL_VIDEO 0x04
#define RTMP_CHANNEL_AUDIO 0x05

CRtmpPush::CRtmpPush(int log_level)
{
    // 初始化 librtmp 日志级别
    RTMP_LogSetLevel(static_cast<RTMP_LogLevel>(log_level));
}

CRtmpPush::~CRtmpPush() {
    disconnect(); // 确保在销毁时断开连接
}

bool CRtmpPush::connect(const char* rtmp_url) {
    if (isConnected_) {
        qCritical() << "Already connected.";
        return true;
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
    rtmp_raw->Link.lFlags |= RTMP_LF_LIVE; // 设置为直播模式

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
    // 重置发送状态，等待 initialize 调用
    sps_pps_sent_ = false;
    asc_sent_ = false;
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
    asc_sent_ = false;
    // 清空缓存
    sps_.clear();
    pps_.clear();
    asc_.clear();
}

bool CRtmpPush::isConnected() const {
    return isConnected_ && rtmpPtr_ && RTMP_IsConnected(rtmpPtr_.get());
}

bool CRtmpPush::setAVConfig(const uint8_t* sps, size_t sps_len,
    const uint8_t* pps, size_t pps_len,
    const uint8_t* asc, size_t asc_len) {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    if (sps && sps_len > 0) {
        sps_.assign(sps, sps + sps_len);
    }
    else {
        success = false;
        qCritical() << "Invalid SPS provided to initialize.";
    }

    if (pps && pps_len > 0) {
        pps_.assign(pps, pps + pps_len);
    }
    else {
        success = false;
        qCritical() << "Invalid PPS provided to initialize.";
    }

    if (asc && asc_len > 0) {
        asc_.assign(asc, asc + asc_len);
    }
    else {
        success = false;
        qCritical() << "Invalid ASC provided to initialize.";
    }

    // 初始化后不立即发送，等第一个 IDR 帧或音频帧时再发送
    sps_pps_sent_ = false;
    asc_sent_ = false;

    return success;
}


bool CRtmpPush::sendVideo(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe) {
    if (!isConnected() || !data || len == 0) {
        return false;
    }

    qDebug() << "Sending Video data, length:" << len << ", dts:" << dts
		<< ", is_keyframe:" << (is_keyframe ? "true" : "false");

    // 如果是关键帧且 SPS/PPS 还没发送，则先发送
    if (is_keyframe && !sps_pps_sent_ && !sps_.empty() && !pps_.empty()) {
        if (!sendVideoHeader()) {
            return false;
        }
        sps_pps_sent_ = true;
    }

    // 创建并发送普通视频帧数据包
    RTMPPacket* video_packet = createVideoPacket(data, len, dts, is_keyframe);
    if (video_packet) {
        bool res = sendPacket(video_packet);
        // sendPacket 内部会调用 RTMPPacket_Free 和 free(packet)
        return res;
    }
    return false;
}

bool CRtmpPush::sendAudio(const uint8_t* data, size_t len, uint32_t dts) {
    if (!isConnected() || !data || len == 0) {
        return false;
    }

	qDebug() << "Sending Audio data, length:" << len << ", dts:" << dts;

    // 如果是 Sequence Header 且还没发送，则先发送
    if (!asc_sent_ && !asc_.empty()) {
        if (!sendAudioHeader()) {
            return false;
        }
        asc_sent_ = true;
        return true; // Sequence Header 发送完毕
    }

    // 发送 AAC 原始数据帧
    RTMPPacket* audio_packet = createAudioPacket(data, len, dts);
    if (audio_packet) {
        bool res = sendPacket(audio_packet);
        // sendPacket 内部会调用 RTMPPacket_Free 和 free(packet)
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

bool CRtmpPush::sendVideoHeader() {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return false;
    RTMPPacket_Reset(packet);

    // 计算包体大小
    size_t body_size = 11 + sps_.size() + 1 + 2 + pps_.size() + 2;
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
    body[i++] = 0xFF; // lengthSizeMinusOne (通常为 3 bytes for length)

    // SPS
    body[i++] = 0xE1; // numOfSequenceParameterSets (1)
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
    packet->m_nTimeStamp = 0; // SPS/PPS 时间戳通常为 0
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    bool res = sendPacket(packet); // sendPacket 会处理内存释放
    // 注意：packet 指针在此之后已失效
    return res;
}


bool CRtmpPush::sendAudioHeader() {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return false;
    RTMPPacket_Reset(packet);

    size_t body_size = 2 + asc_.size(); // 2 bytes header + ASC
    RTMPPacket_Alloc(packet, body_size);

    uint8_t* body = reinterpret_cast<uint8_t*>(packet->m_body);
    int i = 0;
    body[i++] = 0xAF; // SoundFormat (10=AAC) + SoundRate + SoundSize + SoundType
    body[i++] = 0x00; // AACPacketType: 0=Sequence Header

    if (!asc_.empty()) {
        memcpy(body + i, asc_.data(), asc_.size());
        i += asc_.size();
    }

    packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet->m_nBodySize = i;
    packet->m_nChannel = RTMP_CHANNEL_AUDIO;
    packet->m_nTimeStamp = 0; // ASC 时间戳通常为 0
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    bool res = sendPacket(packet); // sendPacket 会处理内存释放
    // 注意：packet 指针在此之后已失效
    return res;
}


RTMPPacket* CRtmpPush::createVideoPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe) {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return nullptr;
    RTMPPacket_Reset(packet);

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

RTMPPacket* CRtmpPush::createAudioPacket(const uint8_t* data, size_t len, uint32_t dts) {
    // 此函数现在应该只处理 Raw AAC Data，因为 Sequence Header 由 sendAudioHeader 处理

    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return nullptr;
    RTMPPacket_Reset(packet);

    size_t body_size = 2 + len; // 2 bytes header (0xAF, 0x01) + Raw AAC data
    RTMPPacket_Alloc(packet, body_size);

    uint8_t* body = reinterpret_cast<uint8_t*>(packet->m_body);
    int i = 0;
    body[i++] = 0xAF; // SoundFormat (10=AAC) + SoundRate + SoundSize + SoundType
    body[i++] = 0x01; // AACPacketType: 1=Raw Data

    if (len > 0) {
        memcpy(body + i, data, len);
    }

    packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet->m_nBodySize = body_size; // i + len
    packet->m_nChannel = RTMP_CHANNEL_AUDIO;
    packet->m_nTimeStamp = dts;
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    return packet;
}