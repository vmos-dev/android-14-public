#ifndef VOICE_EQ_PREPROCESS_H_
#define VOICE_EQ_PREPROCESS_H_


#ifdef __cplusplus
extern "C" {
#endif
#define EQ_DRC_DUMP_AUDIO_DATA
#define CONFIG_FILE_INOTIFY

struct AUDIOPOST_STRUCT;
#define PARALEN          (4096)   /* Parameter number */

typedef int (*Start_Record)();
typedef int (*Stop_Record)();
typedef struct AUDIOPOST_STRUCT * (*AudioPost_Init)(char *, signed int);
typedef void (*AudioPost_Destroy)(struct AUDIOPOST_STRUCT *);
typedef void (*AudioPost_Process)(struct AUDIOPOST_STRUCT *, float *, float *, signed short int, signed int);
typedef void (*AudioPost_SetPara)(struct AUDIOPOST_STRUCT * ,float *, signed int);
typedef struct rk_eq_drc_api_ {
    AudioPost_Init pfAudioPost_Init;
    AudioPost_Destroy pfAudioPost_Destroy;
    AudioPost_Process pfAudioPost_Process;
    AudioPost_SetPara pfAudioPost_SetPara;
    Start_Record start_record;
    Stop_Record stop_record;
    int (*rk_eq_drc_process)(const void *in_buffer, void *out_buffer, unsigned long size, int pcm_channel,int bit_per_sample);
} rk_eq_drc_api;

rk_eq_drc_api* rk_eq_drc_create(const char *param_name, unsigned long size, unsigned long frame_size);
int rk_eq_drc_destory();
#ifdef __cplusplus
}
#endif

#endif

