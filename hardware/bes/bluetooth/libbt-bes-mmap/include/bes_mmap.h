#ifndef __BES_MMAP_H__
#define __BES_MMAP_H__

#define MAXBUFSIZE (2048)
#define MMAP_TRANSFD_OP 1000
#define MMAP_WRITEDATA_OP 1001

typedef int(*t_bes_mmap_read_cb)(unsigned char * str, unsigned short length);
typedef struct __bes_mmap_context{
    int fd;
    int mmap_file_max_size;
    unsigned char * mmap_address;
    t_bes_mmap_read_cb readCb;
    bool service_started;
    unsigned long int thread_id;
    unsigned char mmap_path[256];
    unsigned char mmap_name[128];
    unsigned int reserved[4];
}bes_mmap_context_st;

#ifdef __cplusplus
extern "C" {
#endif
int bes_mmap_set_path(bes_mmap_context_st* context, unsigned char * str, unsigned short length);
int bes_mmap_set_name(bes_mmap_context_st* context, unsigned char * str, unsigned short length);
unsigned char* bes_mmap_get_path(bes_mmap_context_st* context);
unsigned char* bes_mmap_get_name(bes_mmap_context_st* context);

//write
int bes_mmap_write_init(bes_mmap_context_st* context);
void bes_mmap_write_close(bes_mmap_context_st* context);
int bes_mmap_write_start(bes_mmap_context_st* context, unsigned char * ram_data, unsigned int length);

//read
void bes_mmap_read_register_cb(bes_mmap_context_st* context,t_bes_mmap_read_cb func);
int bes_mmap_read_init(bes_mmap_context_st* context);
int bes_mmap_read_close(bes_mmap_context_st* context);
#ifdef __cplusplus
}
#endif
#endif