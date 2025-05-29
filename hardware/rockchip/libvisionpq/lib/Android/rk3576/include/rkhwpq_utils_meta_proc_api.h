#pragma once
#ifndef __RKHWPQ_UTILS_META_PROC_API_H_
#define __RKHWPQ_UTILS_META_PROC_API_H_

#ifdef __cplusplus
extern "C"
{
#endif
/* Run mode */
#define RK_HWPQ_UTILS_RUN_MODE_META_PROC    0x10

/* Process Command */
typedef enum{
    RK_HWPQ_UTILS_META_PROC_MIN = 0x200, 
    RK_HWPQ_UTILS_META_PROC_SET_CFG, 
    RK_HWPQ_UTILS_META_PROC_QUERY_SIZE, 
    RK_HWPQ_UTILS_META_PROC_SET_DST_ADDR, 
    RK_HWPQ_UTILS_META_PROC_PACK_PAYLOAD, 
    RK_HWPQ_UTILS_META_PROC_UNPACK_PAYLOAD, 
    RK_HWPQ_UTILS_META_PROC_GET_PAYLOAD_DATA, 
    RK_HWPQ_UTILS_META_PROC_MAX, 
} RK_HWPQ_UTILS_META_PROC_CMD;

/* Meta data type */
typedef enum{
    RK_HWPQ_META_DATA_TYPE_NONE = 0,
    RK_HWPQ_META_DATA_TYPE_HIST, 
    RK_HWPQ_META_DATA_TYPE_PQCFG, 
    RK_HWPQ_META_DATA_TYPE_MAX
} RK_HWPQ_META_DATA_TYPE;

/* Meta data Size */
#define RK_HWPQ_META_SIZE_HIST 10256
#define RK_HWPQ_META_SIZE_PQCFG 256

/* Hwpq meta_gen data structure */
#define RK_HWPQ_META_PROC_MAX_PAYLOAD_NUM 16

typedef struct RkHwpqMetaProcPayloadInfo_t {
    RK_HWPQ_META_DATA_TYPE payload_type;
    void* payload_addr;
    int payload_size;
    int reserve0[4];
} RkHwpqMetaProcPayloadInfo;

typedef struct RkHwpqMetaProcCommonCfg_t {
    RK_HWPQ_META_DATA_TYPE payload_type_list[RK_HWPQ_META_PROC_MAX_PAYLOAD_NUM];
    int payload_num;
    int non_copy_flag;
    int reserve0[4];
} RkHwpqMetaProcCommonCfg;

typedef struct RkHwpqMetaProcQueryInfo_t {
    void* payload_addr_list[RK_HWPQ_META_PROC_MAX_PAYLOAD_NUM];
    int payload_offset_list[RK_HWPQ_META_PROC_MAX_PAYLOAD_NUM];
    int payload_size_list[RK_HWPQ_META_PROC_MAX_PAYLOAD_NUM];
    int total_meta_bytes;
    int reserve0[4];
} RkHwpqMetaProcQueryInfo;

enum MetaProcProcType {
    META_PROC_PARAM_TYPE_COM, 
    META_PROC_PARAM_TYPE_PAYLOAD, 
    META_PROC_PARAM_TYPE_QUERY, 
    META_PROC_PARAM_TYPE_META_ADDR, 
};
union MetaProcPorcConfig {
    RkHwpqMetaProcCommonCfg   common_cfg;
    RkHwpqMetaProcPayloadInfo curr_payload_info;
    RkHwpqMetaProcQueryInfo   query_ret_info;
    void*                    meta_addr;
};
typedef struct RkHwpqMetaProcCfg_t {
    enum MetaProcProcType ptype;
    union MetaProcPorcConfig param;
} RkHwpqMetaProcCfg;


/* Hwpq meta data header */
typedef struct RkHwpqMetaHeader_t {
    /* For transmission */
    unsigned short  magic;              /* magic word for checking overwrite error      */
    unsigned short  size;               /* total header+payload length including header */
    unsigned short  message_total;      /* total message count in current transmission  */
    unsigned short  message_index;      /* current message index in the transmission    */

    /* For payload identification */
    unsigned short  version;            /* payload structure version                    */
    unsigned short  payload_type;       /* payload type                                 */
    unsigned int    payload_size;       /* payload size in bytes                        */
    
    /* For extenstion usage */
    unsigned int    reserve[4];

    /* payload data aligned to 32bits */
    unsigned int    payload[];
} RkHwpqMetaHeader;


#ifdef __cplusplus
}
#endif
#endif //__RKHWPQ_UTILS_META_PROC_API_H_