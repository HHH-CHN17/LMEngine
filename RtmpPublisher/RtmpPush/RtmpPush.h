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
     * @brief ��ʼ�������������� SPS/PPS �� AudioSpecificConfig
     *
     * �˷���Ӧ�� connect ֮�󣬿�ʼ����ý������֮ǰ���á�
     *
     * @param sps ָ�� H.264 SPS ���ݵ�ָ��
     * @param sps_len SPS ���ݳ���
     * @param pps ָ�� H.264 PPS ���ݵ�ָ��
     * @param pps_len PPS ���ݳ���
     * @param asc ָ�� AAC AudioSpecificConfig ���ݵ�ָ��
     * @param asc_len AudioSpecificConfig ���ݳ���
     * @return true ��ʼ���ɹ�, false ʧ��
     */
    bool setAVConfig(
        const uint8_t* sps, size_t sps_len,
        const uint8_t* pps, size_t pps_len,
        const uint8_t* asc, size_t asc_len);

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
     * @brief �ڲ��������������� AVC Sequence Header (���� SPS �� PPS)
     * @return true ���ͳɹ�, false ����ʧ��
     */
    bool sendVideoHeader();

    /**
     * @brief �ڲ��������������� AAC Sequence Header (���� AudioSpecificConfig)
     * @return true ���ͳɹ�, false ����ʧ��
     */
    bool sendAudioHeader();

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
    bool isConnected_ = false;
    std::vector<uint8_t> sps_{}; // ���� SPS
    std::vector<uint8_t> pps_{}; // ���� PPS
    std::vector<uint8_t> asc_{}; // ���� AAC AudioSpecificConfig
    bool sps_pps_sent_ = false;     // ����Ƿ��ѷ��� SPS/PPS
    bool asc_sent_ = false;         // ����Ƿ��ѷ��� AudioSpecificConfig
};

#endif // RTMP_PUSH_H