#include "RtmpPush.h"
#include <cstring> // For memcpy, memset
#include <iostream> // For logging, can be replaced with qDebug etc.
#include <algorithm> // For std::max
#include <QDebug>

// --- ���� RTMP ͨ���Ͱ���С ---
#define RTMP_CHANNEL_VIDEO 0x04
#define RTMP_CHANNEL_AUDIO 0x05

CRtmpPush::CRtmpPush(int log_level)
{
    // ��ʼ�� librtmp ��־����
    RTMP_LogSetLevel(static_cast<RTMP_LogLevel>(log_level));
}

CRtmpPush::~CRtmpPush() {
    disconnect(); // ȷ��������ʱ�Ͽ�����
}

bool CRtmpPush::connect(const char* rtmp_url) {
    if (isConnected_) {
        qCritical() << "Already connected.";
        return true;
    }

    // ���� RTMP ����
    RTMP* rtmp_raw = RTMP_Alloc();
    if (!rtmp_raw) {
        qCritical() << "Failed to allocate RTMP object.";
        return false;
    }

    // ʹ�� unique_ptr ������������
    rtmpPtr_.reset(rtmp_raw);

    // ��ʼ�� RTMP ����
    RTMP_Init(rtmp_raw);
    rtmp_raw->Link.timeout = 10; // ���ó�ʱʱ��
    rtmp_raw->Link.lFlags |= RTMP_LF_LIVE; // ����Ϊֱ��ģʽ

    // �������� URL
    if (!RTMP_SetupURL(rtmp_raw, const_cast<char*>(rtmp_url))) {
        qCritical() << "Failed to setup RTMP URL: " << rtmp_url;
        rtmpPtr_.reset(); // ����ָ�룬���� RTMP_Free
        return false;
    }

    // ����д��ģʽ (����)
    RTMP_EnableWrite(rtmp_raw);

    // ��������
    if (!RTMP_Connect(rtmp_raw, nullptr)) {
        qCritical() << "Failed to connect to RTMP server.";
        rtmpPtr_.reset();
        return false;
    }

    // ������
    if (!RTMP_ConnectStream(rtmp_raw, 0)) {
        qCritical() << "Failed to connect to RTMP stream.";
        RTMP_Close(rtmp_raw); // �ر�����
        rtmpPtr_.reset();
        return false;
    }

    isConnected_ = true;
    // ���÷���״̬���ȴ� initialize ����
    sps_pps_sent_ = false;
    asc_sent_ = false;
    qDebug() << "Connected to RTMP server: " << rtmp_url;
    return true;
}

void CRtmpPush::disconnect() {
    if (rtmpPtr_ && isConnected_) {
        RTMP_Close(rtmpPtr_.get());
        // rtmpPtr_.reset() �����������������ʽ����ʱ���� RTMP_Free
        qDebug() << "Disconnected from RTMP server.";
    }
    isConnected_ = false;
    sps_pps_sent_ = false;
    asc_sent_ = false;
    // ��ջ���
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

    // ��ʼ�����������ͣ��ȵ�һ�� IDR ֡����Ƶ֡ʱ�ٷ���
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

    // ����ǹؼ�֡�� SPS/PPS ��û���ͣ����ȷ���
    if (is_keyframe && !sps_pps_sent_ && !sps_.empty() && !pps_.empty()) {
        if (!sendVideoHeader()) {
            return false;
        }
        sps_pps_sent_ = true;
    }

    // ������������ͨ��Ƶ֡���ݰ�
    RTMPPacket* video_packet = createVideoPacket(data, len, dts, is_keyframe);
    if (video_packet) {
        bool res = sendPacket(video_packet);
        // sendPacket �ڲ������ RTMPPacket_Free �� free(packet)
        return res;
    }
    return false;
}

bool CRtmpPush::sendAudio(const uint8_t* data, size_t len, uint32_t dts) {
    if (!isConnected() || !data || len == 0) {
        return false;
    }

	qDebug() << "Sending Audio data, length:" << len << ", dts:" << dts;

    // ����� Sequence Header �һ�û���ͣ����ȷ���
    if (!asc_sent_ && !asc_.empty()) {
        if (!sendAudioHeader()) {
            return false;
        }
        asc_sent_ = true;
        return true; // Sequence Header �������
    }

    // ���� AAC ԭʼ����֡
    RTMPPacket* audio_packet = createAudioPacket(data, len, dts);
    if (audio_packet) {
        bool res = sendPacket(audio_packet);
        // sendPacket �ڲ������ RTMPPacket_Free �� free(packet)
        return res;
    }
    return false;
}

// --- Private Helper Functions ---

bool CRtmpPush::sendPacket(RTMPPacket* packet, int queue) {
    if (!packet || !isConnected()) {
        return false;
    }
    // RTMP_SendPacket ���� 1 ��ʾ�ɹ���0 ��ʾʧ��
    int result = RTMP_SendPacket(rtmpPtr_.get(), packet, queue);
    RTMPPacket_Free(packet); // ���ͺ������ͷ� packet �ڴ�
    free(packet);            // �ͷ� packet �ṹ�屾�� (�� createPacket ����)
    return result == 1;
}

bool CRtmpPush::sendVideoHeader() {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return false;
    RTMPPacket_Reset(packet);

    // ��������С
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
    body[i++] = 0xFF; // lengthSizeMinusOne (ͨ��Ϊ 3 bytes for length)

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
    packet->m_nBodySize = i; // ʵ��д��Ĵ�С
    packet->m_nChannel = RTMP_CHANNEL_VIDEO;
    packet->m_nTimeStamp = 0; // SPS/PPS ʱ���ͨ��Ϊ 0
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    bool res = sendPacket(packet); // sendPacket �ᴦ���ڴ��ͷ�
    // ע�⣺packet ָ���ڴ�֮����ʧЧ
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
    packet->m_nTimeStamp = 0; // ASC ʱ���ͨ��Ϊ 0
    packet->m_hasAbsTimestamp = 0;
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

    bool res = sendPacket(packet); // sendPacket �ᴦ���ڴ��ͷ�
    // ע�⣺packet ָ���ڴ�֮����ʧЧ
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
    // �˺�������Ӧ��ֻ���� Raw AAC Data����Ϊ Sequence Header �� sendAudioHeader ����

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