#ifndef __BES_SND_IO_H__
#define __BES_SND_IO_H__

typedef struct bes_tiny_mixer_para_st {
    char * value_name;
    char * value;
}bes_tiny_mixer_para_t;

typedef struct bes_tinyalsa_para_st{
    unsigned int card;
    unsigned int device;
    unsigned int channels;
    unsigned int rate;
    unsigned int bits;
    unsigned int period_size;
    unsigned int period_count;
}bes_tinyalsa_para_t;

bool bes_set_snd_path(void);
bool bes_snd_tinyalsa_open(bes_tinyalsa_para_t * alsa_param, bool pcm_out);
int bes_snd_pcm_play(char * buffer, uint32_t buffer_size);
int bes_snd_pcm_cap(char * buffer, uint32_t buffer_size);
void bes_snd_tinyalsa_close(bool pcm_out);
#endif
