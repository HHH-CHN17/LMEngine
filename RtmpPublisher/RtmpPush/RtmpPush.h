#ifndef RTMP_PUSH_H
#define RTMP_PUSH_H

#ifdef _WIN32
#include <winsock2.h> // librtmp on Windows might need this before librtmp.h
#endif

extern "C" {
#include <lib/win32/librtmp/include/rtmp.h>
#include <lib/win32/librtmp/include/log.h>
}

#include <string>
#include <vector>
#include <memory> // For smart pointers
#include <cstdint> // For fixed-width integer types

/**
 * @brief RTMP ��������
 *
 * �����װ��ʹ�� librtmp ���� RTMP �����Ļ������ܡ�
 * ֧�� H.264 ��Ƶ�� AAC ��Ƶ���ݵķ��͡�
 */
class CRtmpPush {
public:
    explicit CRtmpPush(int log_level = RTMP_LOGINFO);

    ~CRtmpPush();

    CRtmpPush(const CRtmpPush&) = delete;
    CRtmpPush& operator=(const CRtmpPush&) = delete;

    bool connect(const char* rtmp_url);

    void disconnect();

    bool isConnected() const;

    /**
     * @brief ���� H.264 ��Ƶ����
     *
     * @param data ָ�� H.264 NALU ���ݵ�ָ�� (ͨ��������ʼ�� 0x00000001/01)��
     *             ���� SPS/PPS����Ҫ�ǰ��� SPS �� PPS ��������Ԫ��
     * @param len ���ݳ���
     * @param dts ����ʱ��� (����)
     * @param is_keyframe �Ƿ�Ϊ�ؼ�֡ (IDR)
     * @return true ���ͳɹ�, false ����ʧ��
     */
    bool sendVideo(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe);

    /**
     * @brief ���� AAC ��Ƶ����
     *
     * @param data ָ�� AAC ԭʼ����֡��ָ�� (���� ADTS ͷ)��
     *             ���� AAC Sequence Header (AudioSpecificConfig)����Ҫ���⴦��
     * @param len ���ݳ���
     * @param dts ����ʱ��� (����)
     * @param is_sequence_header �Ƿ�Ϊ AAC Sequence Header
     * @return true ���ͳɹ�, false ����ʧ��
     */
    bool sendAudio(const uint8_t* data, size_t len, uint32_t dts, bool is_sequence_header);

private:
	/**
	 * @brief �ڲ��������������� RTMPPacket
	 * @param packet Ҫ���͵����ݰ�
	 * @param queue �Ƿ����ݰ�������еȴ����� (ͨ����Ϊ 1)
	 * @return true ���ͳɹ�, false ����ʧ��
	 */
    bool sendPacket(RTMPPacket* packet, int queue = 1);

    /**
     * @brief �ڲ������������������� SPS �� PPS
     * �Ӵ������Ƶ��������ȡ SPS �� PPS�������� AVC Sequence Header ����
     * @param data ���� SPS �� PPS ��ԭʼ����
     * @param len ���ݳ���
     * @return true �������ͳɹ�, false ʧ��
     */
    bool handleSpsPps(const uint8_t* data, size_t len);

    /**
     * @brief �ڲ��������������� H.264 ��Ƶ RTMPPacket
     * @param data NALU ����
     * @param len ���ݳ���
     * @param dts ʱ���
     * @param is_keyframe �Ƿ�Ϊ�ؼ�֡
     * @return �����õ� RTMPPacket ָ�� (��Ҫ�����߸����ͷŻ��ͺ��� sendPacket �ͷ�)
     */
    RTMPPacket* createVideoPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe);

    /**
     * @brief �ڲ��������������� AAC ��Ƶ RTMPPacket
     * @param data AAC ����
     * @param len ���ݳ���
     * @param dts ʱ���
     * @param is_sequence_header �Ƿ�Ϊ Sequence Header
     * @return �����õ� RTMPPacket ָ�� (��Ҫ�����߸����ͷŻ��ͺ��� sendPacket �ͷ�)
     */
    RTMPPacket* createAudioPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_sequence_header);

private:
    std::unique_ptr<RTMP, decltype(&RTMP_Free)> rtmpPtr_; // ʹ������ָ����� RTMP ����
    bool isConnected_;
    std::vector<uint8_t> sps_; // ���� SPS
    std::vector<uint8_t> pps_; // ���� PPS
    bool sps_pps_sent_;        // ����Ƿ��ѷ��� SPS/PPS
};

#endif // RTMP_PUSH_H