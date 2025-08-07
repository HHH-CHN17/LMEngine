#include "RtmpPush.h"
#include <cstring> // For memcpy, memset
#include <iostream> // For optional logging, can be replaced with your logger
#include <QDebug>

// --- ������ ---
#define RTMP_CHANNEL_VIDEO 0x04
#define RTMP_CHANNEL_AUDIO 0x05
//#define RTMP_PACKET_SIZE_MEDIUM RTMP_PACKET_SIZE_MEDIUM
//#define RTMP_PACKET_SIZE_LARGE RTMP_PACKET_SIZE_LARGE

CRtmpPush::CRtmpPush(int log_level) : rtmpPtr_(nullptr, &RTMP_Free), isConnected_(false), sps_pps_sent_(false) {
    // ��ʼ�� librtmp ��־����
    RTMP_LogSetLevel(static_cast<RTMP_LogLevel>(log_level));
    // RTMP_LogSetOutput(stdout); // ��ѡ������ librtmp ��־����� stdout
}

CRtmpPush::~CRtmpPush() {
    disconnect(); // ȷ��������ʱ�Ͽ�����
}

bool CRtmpPush::connect(const char* rtmp_url) {
    if (isConnected_) {
        qCritical() << "Already connected.";
        return true; // ���߷��� false��ȡ���ڲ���
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
    sps_pps_sent_ = false; // ���� SPS/PPS ����״̬
    sps_.clear();
    pps_.clear();
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

    // ���ж��Ƿ�Ϊ SPS/PPS (������һ��������� SPS ��ͷ)
    // ����׳�������Ǽ�� NALU ���� (data[0] & 0x1F)
    if (len >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01) {
        // ���ҵڶ�����ʼ�������� SPS �� PPS
        const uint8_t* sps_start = data;
        size_t sps_len = 0;
        const uint8_t* pps_start = nullptr;
        size_t pps_len = 0;

        // ���ҵڶ��� 0x00000001
        for (size_t i = 4; i < len - 3; ++i) {
            if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
                sps_len = i; // SPS ���ȵ��ڶ�����ʼ��Ϊֹ
                pps_start = data + i + 4; // PPS �ӵڶ�����ʼ��֮��ʼ
                pps_len = len - (i + 4);
                break;
            }
        }

        // ����ҵ��� PPS
        if (pps_start && sps_len > 4 && pps_len > 0) {
            // ȥ����ʼ��
            sps_.assign(sps_start + 4, sps_start + sps_len);
            pps_.assign(pps_start + 4, pps_start + pps_len);
            // qDebug() << "SPS and PPS extracted and cached.";
            // �������ѡ���������� SPS/PPS�����ߵȵ�һ�� IDR ֡�ٷ�
            // Ϊ�˼򻯣����������ﴦ����ʵ��Ӧ���п������յ� IDR ʱ����
            // handleSpsPps(data, len); // Ҳ���Ե��������������
            return true; // SPS/PPS ����ɹ�
        }
    }

    // ����ǹؼ�֡�� SPS/PPS ��û���ͣ����ȷ���
    if (is_keyframe && !sps_pps_sent_ && !sps_.empty() && !pps_.empty()) {
        // ���������� AVC Sequence Header (���� SPS �� PPS)
        RTMPPacket* sps_pps_packet = createVideoPacket(nullptr, 0, 0, true); // ������ã�data=nullptr ��ʾ���� SPS/PPS
        if (sps_pps_packet) {
            bool res = sendPacket(sps_pps_packet);
            if (res) {
                sps_pps_sent_ = true;
                // qDebug() << "SPS/PPS packet sent.";
            }
            // sendPacket �ڲ������ RTMPPacket_Free
            // free(sps_pps_packet); // ����Ҫ����Ϊ sendPacket �Ѿ�����
            if (!res) return false;
        }
    }

    // ������������ͨ��Ƶ֡���ݰ�
    RTMPPacket* video_packet = createVideoPacket(data, len, dts, is_keyframe);
    if (video_packet) {
        bool res = sendPacket(video_packet);
        // sendPacket �ڲ������ RTMPPacket_Free
        // free(video_packet); // ����Ҫ
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
        // sendPacket �ڲ������ RTMPPacket_Free
        // free(audio_packet); // ����Ҫ
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


bool CRtmpPush::handleSpsPps(const uint8_t* data, size_t len) {
    // �˺������߼����� sendVideo ��ʵ��
    // ���ﱣ����Ϊ�˽ӿ������ԣ������Ҫ������������ڴ�ʵ��
    // ...
    return true;
}

RTMPPacket* CRtmpPush::createVideoPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe) {
    RTMPPacket* packet = static_cast<RTMPPacket*>(malloc(sizeof(RTMPPacket)));
    if (!packet) return nullptr;
    RTMPPacket_Reset(packet);

    // ������������� SPS/PPS
    if (data == nullptr && is_keyframe && !sps_.empty() && !pps_.empty()) {
        // ��������С
        size_t body_size = 11 + sps_.size() + 1 + 2 + pps_.size() + 2; // 11: header, sps_len, pps_len ��ռ�ֽ�
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
        packet->m_nBodySize = i; // ʵ��д��Ĵ�С
        packet->m_nChannel = RTMP_CHANNEL_VIDEO;
        packet->m_nTimeStamp = dts; // SPS/PPS ͨ��ʱ���Ϊ 0�������ﴫ��
        packet->m_hasAbsTimestamp = 0;
        packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM; // �� LARGE
        packet->m_nInfoField2 = rtmpPtr_->m_stream_id;

        return packet;
    }

    // ��ͨ��Ƶ֡
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