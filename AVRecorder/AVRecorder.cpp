#include "AVRecorder.h"
#include <sys/time.h>
#include <QDebug>

CAVRecorder* CAVRecorder::GetInstance()
{
    static CAVRecorder objAVRecorder{};

    return &objAVRecorder;
}


CAVRecorder::CAVRecorder()
{
    avFormatCtx = NULL;

    videoCodecCtx = NULL;
    audioCodecCtx = NULL;

    videoStream = NULL;
    audioStream = NULL;

    videoSwCtx_ = NULL;
    yuvFrame = NULL;

    m_filePath = "";

    videoInWidth_ = 640; videoInHeight_ = 360;
    videoOutWidth_ = 640; videoOutHeight_ = 360;

    m_videoOutBitrate = 4000000;

    m_audioInSamplerate = 44100;    m_audioInChannels = 1;
    m_audioOutSamplerate = 44100;    m_audioOutChannels = 1;

    m_audioOutBitrate = 64000;

    m_bRecording = false;
    startTimeStamp = 0;
    m_lastPts = 0;

    g_aacEncodeConfig = NULL;

    av_register_all();
    avcodec_register_all();

    g_aacEncodeConfig = initAudioEncodeConfiguration();
    if (g_aacEncodeConfig == NULL) {
	    std::cout << "initAudioEncodeConfiguration failed..." << endl;
        return;
    }

}

CAVRecorder::~CAVRecorder()
{
    ReleaseAccConfiguration();
}

void CAVRecorder::setInputWH(int w, int h)
{
    videoInWidth_ = w;
    videoInHeight_ = h;
}

void CAVRecorder::setOutputWH(int w, int h)
{
    videoOutWidth_ = w;
    videoOutHeight_ = h;
}

bool CAVRecorder::initOutputFile(const char* file)
{
    freeAll();

    avformat_alloc_output_context2(&avFormatCtx, NULL, NULL, file);
    if (avFormatCtx == NULL) {
	    std::cout << "avformat_alloc_output_context2 failed" << endl;
        return false;
    }

    m_filePath = file;
    m_audioFramePts = 0;

    addVideoStream();

    addAudioStream();

    if (avio_open(&avFormatCtx->pb, file, AVIO_FLAG_WRITE) != 0) {
	    std::cout << "avio_open failed" << endl;
        return false;
    }

    if (avformat_write_header(avFormatCtx, NULL) != 0) {
	    std::cout << "avformat_write_header failed" << endl;
        return false;
    }

    std::cout << "initOutputFile success..." << endl;

    m_bRecording = true;
    m_lastPts = startTimeStamp = getTickCount();

    return true;

}

void CAVRecorder::stopRecord()
{
    m_bRecording = false;

    {
        std::lock_guard<std::mutex> lg{ videoWriterMtx_ };
        endWriteMp4File();
        freeAll();
    }
}


bool CAVRecorder::recording(const unsigned char* rgbData)
{
    if (m_bRecording == false) {
        return false;
    }

    if (avFormatCtx == NULL || videoSwCtx_ == NULL || yuvFrame == NULL) {
        return false;
    }

    if (rgbData == NULL) {
        return false;
    }

    uint8_t* indata[AV_NUM_DATA_POINTERS] = { 0 };
    indata[0] = (uint8_t*)rgbData;

    int insize[AV_NUM_DATA_POINTERS] = { 0 };
    insize[0] = videoInWidth_ * 4;

    int ret = sws_scale(videoSwCtx_, indata, insize, 0, videoInHeight_, yuvFrame->data, yuvFrame->linesize);
    if (ret < 0) {
        return false;
    }

    //解码出来是倒置的，这里把yuv做一个转换.
    yuvFrame->data[0] += yuvFrame->linesize[0] * (videoCodecCtx->height - 1);
    yuvFrame->linesize[0] *= -1;
    yuvFrame->data[1] += yuvFrame->linesize[1] * (videoCodecCtx->height / 2 - 1);
    yuvFrame->linesize[1] *= -1;
    yuvFrame->data[2] += yuvFrame->linesize[2] * (videoCodecCtx->height / 2 - 1);
    yuvFrame->linesize[2] *= -1;


    unsigned long currentPts = getTickCount() - startTimeStamp;
    if (currentPts - m_lastPts <= 0) {
        currentPts = currentPts + 1;
    }
    yuvFrame->pts = currentPts;

    m_lastPts = currentPts;
    //编码
    if (avcodec_send_frame(videoCodecCtx, yuvFrame) != 0) {
        return false;
    }
    AVPacket packet;
    av_init_packet(&packet);

    int retValue = avcodec_receive_packet(videoCodecCtx, &packet);
    if (retValue != 0 || packet.size <= 0) {
        return false;
    }

    av_packet_rescale_ts(&packet, videoCodecCtx->time_base, videoStream->time_base);

    packet.stream_index = videoStream->index;

    writeFrame(&packet);

    av_free_packet(&packet);

    return true;
}


bool CAVRecorder::WriteAudioFrameWithPCMData(unsigned char* audioData, int captureSize)
{
    if (!m_bRecording) {
        return false;
    }

    linearPCM2AAC(audioData, captureSize);

    return true;
}

bool CAVRecorder::writeFrame(AVPacket* packet)
{
    if (m_bRecording == false) {
        return false;
    }

    if (packet == NULL) {
        return false;
    }

    if (packet->data == NULL) {
        return false;
    }

    if (avFormatCtx == NULL || packet == NULL || packet->size <= 0) {
        return false;
    }
    int retValue = 0;
    {
        std::lock_guard<std::mutex> lg{ videoWriterMtx_ };
        retValue = av_interleaved_write_frame(avFormatCtx, packet);
    }
    if (retValue != 0) {
	    std::cout << "av_interleaved_write_frame failed :" << retValue << endl;
        return false;
    }

    return true;
}
bool CAVRecorder::addVideoStream()
{

    if (avFormatCtx == NULL) {
        return false;
    }

    AVCodec* videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (videoCodec == NULL) {
	    std::cout << "avcodec_find_encoder failed" << endl;
        return false;
    }

    videoCodecCtx = avcodec_alloc_context3(videoCodec);
    if (videoCodecCtx == NULL) {
	    std::cout << "avcodec_alloc_context3 failed" << endl;
        return false;
    }

    videoCodecCtx->width = videoOutWidth_;
    videoCodecCtx->height = videoOutHeight_;

    AVRational time_base;
    time_base.num = 1; time_base.den = 1000;
    videoCodecCtx->time_base = time_base;

    videoCodecCtx->gop_size = 50;
    videoCodecCtx->max_b_frames = 0;
    videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecCtx->codec_id = AV_CODEC_ID_H264;
    av_opt_set(videoCodecCtx->priv_data, "preset", "superfast", 0);
    videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(videoCodecCtx, videoCodec, NULL) != 0) {
        avcodec_free_context(&videoCodecCtx);
        std::cout << "avcodec_open2 failed" << endl;
        return false;
    }
    std::cout << "avcodec_open2 success..." << endl;

    videoStream = avformat_new_stream(avFormatCtx, NULL);
    if (videoStream == NULL) {
	    std::cout << "avformat_new_stream failed" << endl;
        return false;
    }
    videoStream->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(videoStream->codecpar, videoCodecCtx);

    av_dump_format(avFormatCtx, 0, m_filePath.c_str(), 1);

    videoSwCtx_ = sws_getCachedContext(videoSwCtx_, videoInWidth_, videoInHeight_, AV_PIX_FMT_RGBA,
        videoOutWidth_, videoOutHeight_, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    if (videoSwCtx_ == NULL) {
	    std::cout << "sws_getCachedContext failed" << endl;
        return false;
    }

    yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = videoOutWidth_;
    yuvFrame->height = videoOutHeight_;
    yuvFrame->pts = 0;
    if (av_frame_get_buffer(yuvFrame, 32) != 0) {
	    std::cout << "av_frame_get_buffer failed" << endl;
        return false;
    }

    return true;
}

bool CAVRecorder::addAudioStream()
{
    if (avFormatCtx == NULL) {
        return false;
    }
    AVCodec* audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (audioCodec == NULL) {
	    std::cout << "avcodec_find_encoder failed" << endl;
        return false;
    }

    audioCodecCtx = avcodec_alloc_context3(audioCodec);
    if (audioCodecCtx == NULL) {
	    std::cout << " avcodec_alloc_context3 failed" << endl;
        return false;
    }

    audioCodecCtx->sample_rate = m_audioOutSamplerate;
    audioCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    audioCodecCtx->channels = m_audioOutChannels;
    audioCodecCtx->channel_layout = av_get_default_channel_layout(m_audioOutChannels);
    audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(audioCodecCtx, audioCodec, NULL) != 0) {
	    std::cout << " avcodec_open2 failed" << endl;
        return false;
    }

    audioStream = avformat_new_stream(avFormatCtx, NULL);
    if (audioStream == NULL) {
	    std::cout << " avformat_new_stream failed" << endl;
        return false;
    }

    audioStream->codecpar->codec_tag = 0;
    avcodec_parameters_from_context(audioStream->codecpar, audioCodecCtx);

    av_dump_format(avFormatCtx, 0, m_filePath.c_str(), 1);




    return true;
}

bool CAVRecorder::endWriteMp4File()
{
    if (avFormatCtx == NULL) {
        return false;
    }
    if (avFormatCtx->pb == NULL) {
        return false;
    }

    if (av_write_trailer(avFormatCtx) != 0) {
	    std::cout << "av_write_trailer failed" << endl;
        return false;
    }

    if (avio_closep(&avFormatCtx->pb) != 0) {
	    std::cout << "avio_close failed" << endl;
        return false;
    }

    std::cout << "endWriteMp4File success..." << endl;
    return true;

}

void CAVRecorder::freeAll()
{
    if (videoCodecCtx != NULL) {
        avcodec_free_context(&videoCodecCtx);
        videoCodecCtx = NULL;
    }

    if (audioCodecCtx != NULL) {
        avcodec_free_context(&audioCodecCtx);
        audioCodecCtx = NULL;
    }

    if (videoSwCtx_ != NULL) {
        sws_freeContext(videoSwCtx_);
        videoSwCtx_ = NULL;
    }

    if (yuvFrame != NULL) {
        av_frame_free(&yuvFrame);
        yuvFrame = NULL;
    }

    if (avFormatCtx != NULL) {
        avformat_close_input(&avFormatCtx);
        avFormatCtx = NULL;
    }


}

unsigned long CAVRecorder::getTickCount()
{

    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0)
        return 0;

    return (tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

int CAVRecorder::linearPCM2AAC(unsigned char* pData, int captureSize)
{
    if (pData == NULL) {
        return -1;
    }
    if ((captureSize > m_pcmBufferSize) || (captureSize <= 0)) {
        return -1;
    }

    int nRet = 0;
    int copyLength = 0;

    if (m_pcmBufferRemainSize > captureSize) {
        copyLength = captureSize;
    }
    else {
        copyLength = m_pcmBufferRemainSize;
    }

    memcpy((&g_aacEncodeConfig->pcmBuffer[0]) + m_pcmWriteRemainSize, pData, copyLength);
    m_pcmBufferRemainSize -= copyLength;
    m_pcmWriteRemainSize += copyLength;

    if (m_pcmBufferRemainSize > 0) {
        return 0;
    }


    nRet = faacEncEncode(g_aacEncodeConfig->hEncoder, (int*)(g_aacEncodeConfig->pcmBuffer), g_aacEncodeConfig->nInputSamples, g_aacEncodeConfig->aacBuffer, g_aacEncodeConfig->nMaxOutputBytes);

    memset(g_aacEncodeConfig->pcmBuffer, 0, m_pcmBufferSize);
    m_pcmWriteRemainSize = 0;
    m_pcmBufferRemainSize = m_pcmBufferSize;


    AVPacket* pkt = av_packet_alloc();
    av_init_packet(pkt);

    pkt->stream_index = audioStream->index;//音频流的索引
    pkt->data = g_aacEncodeConfig->aacBuffer;
    pkt->size = nRet;
    pkt->pts = m_audioFramePts;
    pkt->dts = pkt->pts;
    AVRational rat = (AVRational){ 1,audioCodecCtx->sample_rate };

    m_audioFramePts += av_rescale_q(m_samples, rat, audioCodecCtx->time_base);

    writeFrame(pkt);

    av_packet_free(&pkt);

    memset(g_aacEncodeConfig->pcmBuffer, 0, m_pcmBufferSize);
    if ((captureSize - copyLength) > 0) {
        memcpy((&g_aacEncodeConfig->pcmBuffer[0]), pData + copyLength, captureSize - copyLength);
        m_pcmWriteRemainSize = captureSize - copyLength;
        m_pcmBufferRemainSize = m_pcmBufferSize - (captureSize - copyLength);
    }

    return nRet;

}
// 初始化音频.

AACEncodeConfig* CAVRecorder::initAudioEncodeConfiguration()
{
    AACEncodeConfig* aacConfig = NULL;

    faacEncConfigurationPtr pConfiguration;

    int nRet = 0;
    m_pcmBufferSize = 0;

    aacConfig = (AACEncodeConfig*)malloc(sizeof(AACEncodeConfig));

    aacConfig->nSampleRate = 44100;
    aacConfig->nChannels = 1;
    aacConfig->nPCMBitSize = 16;
    aacConfig->nInputSamples = 0;
    aacConfig->nMaxOutputBytes = 0;

    aacConfig->hEncoder = faacEncOpen(aacConfig->nSampleRate, aacConfig->nChannels, (unsigned long*)&aacConfig->nInputSamples, (unsigned long*)&aacConfig->nMaxOutputBytes);
    if (aacConfig->hEncoder == NULL)
    {
        printf("failed to call faacEncOpen()\n");
        return NULL;
    }

    m_pcmBufferSize = (int)(aacConfig->nInputSamples * (aacConfig->nPCMBitSize / 8));
    m_pcmBufferRemainSize = m_pcmBufferSize;

    aacConfig->pcmBuffer = (unsigned char*)malloc(m_pcmBufferSize * sizeof(unsigned char));
    memset(aacConfig->pcmBuffer, 0, m_pcmBufferSize);

    aacConfig->aacBuffer = (unsigned char*)malloc(aacConfig->nMaxOutputBytes * sizeof(unsigned char));
    memset(aacConfig->aacBuffer, 0, aacConfig->nMaxOutputBytes);


    pConfiguration = faacEncGetCurrentConfiguration(aacConfig->hEncoder);

    pConfiguration->inputFormat = FAAC_INPUT_16BIT;
    pConfiguration->outputFormat = 0;
    pConfiguration->aacObjectType = LOW;


    nRet = faacEncSetConfiguration(aacConfig->hEncoder, pConfiguration);

    return aacConfig;
}

void CAVRecorder::ReleaseAccConfiguration()
{
    if (g_aacEncodeConfig == NULL) {
        return;
    }

    if (g_aacEncodeConfig->hEncoder != NULL)
    {
        faacEncClose(g_aacEncodeConfig->hEncoder);
        g_aacEncodeConfig->hEncoder = NULL;
    }

    if (g_aacEncodeConfig->pcmBuffer != NULL)
    {
        free(g_aacEncodeConfig->pcmBuffer);
        g_aacEncodeConfig->pcmBuffer = NULL;
    }

    if (g_aacEncodeConfig->aacBuffer != NULL)
    {
        free(g_aacEncodeConfig->aacBuffer);
        g_aacEncodeConfig->aacBuffer = NULL;
    }

    if (g_aacEncodeConfig != NULL) {
        free(g_aacEncodeConfig);
        g_aacEncodeConfig = NULL;
    }

}
