#ifndef PSP2_MP4REC_H
#define PSP2_MP4REC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define SCE_MP4REC_ERROR_INVALID_ARG 0x80108301
#define SCE_MP4REC_ERROR_NO_MEMORY 0x80108302
#define SCE_MP4REC_ERROR_INVALID_MODE 0x80108303
#define SCE_MP4REC_ERROR_SMALL_MEMORY 0x80108304
#define SCE_MP4REC_ERROR_INVALID_STATE 0x80108305
#define SCE_MP4REC_ERROR_LIMIT_EXCEEDED 0x80108306
#define SCE_MP4REC_ERROR_DISABLED 0x80108308
#define SCE_MP4REC_ERROR_NOT_PERMITTED 0x801083e3

#define SCE_MP4REC_VIDEO_TIMESCALE 30000
#define SCE_MP4REC_VIDEO_SAMPLE_DURATION 1000

#define SCE_MP4REC_AUDIO_TIMESCALE 48000
#define SCE_MP4REC_AUDIO_SAMPLE_DURATION 1024

#define SCE_MP4REC_COMMON_DENOM_TIMESCALE 240000

#define SCE_MP4REC_AUDIO_BUFFER_SIZE 4096

#define SCE_MP4REC_PIXELFORMAT_A8B8G8R8 0
#define SCE_MP4REC_PIXELFORMAT_YUV420_PACKED 0x20

    typedef struct SceMp4RecRecorder
    {
        uint32_t size;
        void* base;
        uint32_t base_size;
        void* context;
    } SceMp4RecRecorder;

    typedef struct SceMp4RecInitParam
    {
        uint32_t size;
        uint32_t mode;
        void* encoder_mem;
        uint32_t encoder_size;
        void* av_mem;
        uint32_t av_size;
        uint32_t affinity;
        uint32_t priority;
    } SceMp4RecInitParam;

    typedef struct SceMp4RecFrame
    {
        uint32_t size;
        uint32_t pixelformat;
        uint32_t stride;
        uint32_t width;
        uint32_t height;
        uint32_t reserved[4];
        void* buffer;
        uint32_t reserved2;
    } SceMp4RecFrame;

    typedef struct SceMp4RecTermParam
    {
        uint32_t size;
        uint32_t discard;
        uint8_t reserved[0x200];
        void* metadata;
    } SceMp4RecTermParam;

    int sceMp4RecCreateRecorder(SceMp4RecRecorder* recorder);
    int sceMp4RecDeleteRecorder(SceMp4RecRecorder* recorder);
    int sceMp4RecQueryPhysicalMemSize(SceMp4RecRecorder* recorder, int mode, uint32_t* encoder_size,
                                      uint32_t* av_size);
    int sceMp4RecInit(SceMp4RecRecorder* recorder, SceMp4RecInitParam* param);
    int sceMp4RecTerm(SceMp4RecRecorder* recorder, SceMp4RecTermParam* param);
    int sceMp4RecCsc(SceMp4RecFrame* dst, SceMp4RecFrame* src);
    int sceMp4RecAddVideoSample(SceMp4RecRecorder* recorder, void* buffer, uint32_t size);
    int sceMp4RecAddAudioSample(SceMp4RecRecorder* recorder, void* buffer, uint32_t size);

    int sceMp4RecRecorderInit(SceMp4RecRecorder* recorder);
    int sceMp4RecRecorderEnd(SceMp4RecRecorder* recorder);

#ifdef __cplusplus
}
#endif

#endif
