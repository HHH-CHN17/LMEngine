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
 * @brief RTMP 推流器类
 *
 * 该类封装了使用 librtmp 进行 RTMP 推流的基本功能。
 * 支持 H.264 视频和 AAC 音频数据的发送。
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
     * @brief 发送 H.264 视频数据
     *
     * @param data 指向 H.264 NALU 数据的指针 (通常不含起始码 0x00000001/01)。
     *             对于 SPS/PPS，需要是包含 SPS 和 PPS 的完整单元。
     * @param len 数据长度
     * @param dts 解码时间戳 (毫秒)
     * @param is_keyframe 是否为关键帧 (IDR)
     * @return true 发送成功, false 发送失败
     */
    bool sendVideo(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe);

    /**
     * @brief 发送 AAC 音频数据
     *
     * @param data 指向 AAC 原始数据帧的指针 (不含 ADTS 头)。
     *             对于 AAC Sequence Header (AudioSpecificConfig)，需要特殊处理。
     * @param len 数据长度
     * @param dts 解码时间戳 (毫秒)
     * @param is_sequence_header 是否为 AAC Sequence Header
     * @return true 发送成功, false 发送失败
     */
    bool sendAudio(const uint8_t* data, size_t len, uint32_t dts, bool is_sequence_header);

private:
	/**
	 * @brief 内部辅助函数：发送 RTMPPacket
	 * @param packet 要发送的数据包
	 * @param queue 是否将数据包放入队列等待发送 (通常设为 1)
	 * @return true 发送成功, false 发送失败
	 */
    bool sendPacket(RTMPPacket* packet, int queue = 1);

    /**
     * @brief 内部辅助函数：处理并发送 SPS 和 PPS
     * 从传入的视频数据中提取 SPS 和 PPS，并发送 AVC Sequence Header 包。
     * @param data 包含 SPS 和 PPS 的原始数据
     * @param len 数据长度
     * @return true 处理并发送成功, false 失败
     */
    bool handleSpsPps(const uint8_t* data, size_t len);

    /**
     * @brief 内部辅助函数：创建 H.264 视频 RTMPPacket
     * @param data NALU 数据
     * @param len 数据长度
     * @param dts 时间戳
     * @param is_keyframe 是否为关键帧
     * @return 创建好的 RTMPPacket 指针 (需要调用者负责释放或发送后由 sendPacket 释放)
     */
    RTMPPacket* createVideoPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_keyframe);

    /**
     * @brief 内部辅助函数：创建 AAC 音频 RTMPPacket
     * @param data AAC 数据
     * @param len 数据长度
     * @param dts 时间戳
     * @param is_sequence_header 是否为 Sequence Header
     * @return 创建好的 RTMPPacket 指针 (需要调用者负责释放或发送后由 sendPacket 释放)
     */
    RTMPPacket* createAudioPacket(const uint8_t* data, size_t len, uint32_t dts, bool is_sequence_header);

private:
    std::unique_ptr<RTMP, decltype(&RTMP_Free)> rtmpPtr_; // 使用智能指针管理 RTMP 对象
    bool isConnected_;
    std::vector<uint8_t> sps_; // 缓存 SPS
    std::vector<uint8_t> pps_; // 缓存 PPS
    bool sps_pps_sent_;        // 标记是否已发送 SPS/PPS
};

#endif // RTMP_PUSH_H