#pragma once

#include "AudioEncoder/AudioEncoder.h"
#include "Common/LockFreeQueue.h"
#include "Common/SingletonBase.h"
#include "Muxer/Muxer.h"
#include "VideoEncoder/VideoEncoder.h"

extern "C" {

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <iostream>
#include <mutex>
#include <QQueue>
#include <QThread>
#include <QWaitCondition>
#include <QFile>
#include "AVRecorder/AudioCapturer/AudioCapturer.h"
#include "Common/DataDefine.h"


/**
 * @class CAVRecorder
 * @brief ��������Ƶ¼�ƿ�������
 *
 * �����������Ƶ�Ĳɼ�������ͻ��д���ȫ���̡�
 * �����ö��߳��첽�ܹ�����UI�̡߳������̺߳�д���߳̽��
 * ��ȷ�������ܺ�UI��������Ӧ��
 */
class CAVRecorder : public QObject, public Singleton_Lazy_Base<CAVRecorder>
{
    friend class Singleton_Lazy_Base<CAVRecorder>;
    Q_OBJECT
private:
    CAVRecorder();
    ~CAVRecorder();

public:
    /**
     * @brief ��ʼ��¼������������������
     * @param config ��������Ƶ���롢��ʽ������������Ϣ�Ľṹ�塣
     * @return ��ʼ���ɹ�����true�����򷵻�false��
     */
    bool initialize(AVConfig& config);

    /**
     * @brief �����첽¼�����̡�
     *
     * �ú������������к�̨�����̣߳����롢��ϣ�������ʼ�ɼ���Ƶ��
     * �˺�ϵͳ��׼���ý����� pushRGBA() �������Ƶ���ݡ�
     */
    void startRecording();

    /**
     * @brief ֹͣ�첽¼�����̡�
     *
     * �ú����ᷢ��ֹͣ�źŸ����к�̨�̣߳����ȴ��������ŵ��������ʣ�๤����
     * �������߳̽��������Ḻ������д���ļ�β����Դ��������
     */
    void stopRecording();

    /**
     * @brief [UI�̵߳���] ��һ֡��OpenGL��ȡ��ԭʼRGBA���ݷ����������С�
     *        �˺����Ƿ������ģ����������أ��Ա�֤UI�̵߳�������
     * @param rgbaData ָ���PBO����Դ��ȡ��RGBA�������ݵ�ָ�롣
     */
    void pushRGBA(const unsigned char* rgbaData);

    bool isRecording() const;

private:
    /**
     * @brief ��������ΪQAudioInput������Ƶ��ʽ��
     * @param config ������Ƶ��ʽ��Ϣ�����á�
     * @return ���úõ�QAudioFormat����
     */
    QAudioFormat setAudioFormat(const AudioFormat& config);

    // ------------------------- �첽�����̺߳��� -------------------------
    /**
     * @brief ��Ƶ�����̵߳�ִ���塣
     *
     * ѭ���ش�`rawVideoQueue_`��ȡ��ԭʼ��Ƶ֡�����б��룬
     * Ȼ�󽫱�����AVPacket����`encodedPktQueue_`��
     */
    void videoEncodingLoop();

    /**
     * @brief ��Ƶ�����̵߳�ִ���塣
     *
     * ѭ���ش�`audioCapturer_`����ȡPCM���ݣ����б��룬
     * Ȼ�󽫱�����AVPacket����`encodedPktQueue_`��
     */
    void audioEncodingLoop();

    /**
     * @brief ���д���̵߳�ִ���塣
     *
     * ѭ���ش�`encodedPktQueue_`��ȡ������õ�����Ƶ����
     * ��ͨ��CMuxerд�뵽���յ��ļ��С�
     */
    void muxingLoop();

    /**
     * @brief �������к�̨�����̡߳�
     */
    void startThreads();

    /**
     * @brief ֹͣ���к�̨�����̣߳����ȴ����ǽ�����
     */
    void stopThreads();

    /**
     * @brief ������������һ��AVPacket�б��װ��MediaPacket��������С�
     * @param packets �ӱ��������ص�AVPacket��ָ���б�
     * @param type ��Щ����ý������ (VIDEO, AUDIO, END_OF_STREAM)��
     */
    void sendVecPkt(const QVector<AVPacket*>& packets, const PacketType& type);
private:
    // ����������Դ
    void cleanup();

    // �������
    /// @brief ý�����������𽫱���������Ƶ��д��������ʽ����MP4����
    std::unique_ptr<CMuxer> muxer_;
    /// @brief ��Ƶ������������RGBAͼ�����ΪH.264�ȸ�ʽ��
    std::unique_ptr<CVideoEncoder> videoEncoder_;
    /// @brief ��Ƶ������������PCM��Ƶ����ΪAAC�ȸ�ʽ��
    std::unique_ptr<CAudioEncoder> audioEncoder_;
    /// @brief ��Ƶ�ɼ������������˷粶��ԭʼPCM��Ƶ���ݡ�
    std::unique_ptr<CAudioCapturer> audioCapturer_;

	QFile* h264File = nullptr; // ���ڵ��ԣ�����H264����
	QFile* aacFile = nullptr; // ���ڵ��ԣ�����AAC����

    // ¼�Ʋ���
    AVConfig config_;

    // ״̬����
    /// @brief ��־λ����ʾ¼�������Ƿ�����������UI�߳����ã������̶߳�ȡ��
    std::atomic<bool> isRecording_ = false;

    // ------------------------- �첽������ĳ�Ա -------------------------
    // ���������߳�
    /// @brief ִ����Ƶ����ѭ�����̶߳���
    std::thread videoEncoderThread_;
    /// @brief ִ����Ƶ����ѭ�����̶߳���
    std::thread audioEncoderThread_;
    /// @brief ִ�л��д��ѭ�����̶߳���
    std::thread muxerThread_;

    // �������Ķ���
    lock_free_queue<RGBAUPtr, 60> rawVideoQueue_; // ����Լ2���30fps��Ƶ֡
    lock_free_queue<MediaPacket, 300> encodedPktQueue_; // �������������Ƶ��

    // �߳����п��Ʊ�־
    /// @brief ȫ�����б�־��������Ϊfalseʱ���ر�����Ƶ�����̡߳�
    std::atomic<bool> isRunning_{ false };
};