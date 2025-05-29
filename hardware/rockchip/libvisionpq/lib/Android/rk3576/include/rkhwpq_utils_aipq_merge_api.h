#pragma once
#ifndef __RKHWPQ_UTILS_AIPQ_MERGE_API_H_
#define __RKHWPQ_UTILS_AIPQ_MERGE_API_H_

#ifdef __cplusplus
extern "C"
{
#endif
/* Run Mode */
#define RK_HWPQ_UTILS_RUN_MODE_AIPQ_CFG_CVT     0x20

/* Process Command */
typedef enum{
    RK_HWPQ_UTILS_AIPQ_MERGE_MIN = 0x100, 
    RK_HWPQ_UTILS_AIPQ_MERGE_RUN,
    RK_HWPQ_UTILS_AIPQ_MERGE_MAX, 
} RK_HWPQ_UTILS_AIPQ_MERGE_CMD;


/* Hwpq aipq merge config structure */
#define RK_HWPQ_AIPQ_MERGE_SCENE_TYPE_MAX 3
#define RK_HWPQ_AIPQ_MERGE_SCENE_CONTENT_MAX 5
typedef struct RkHwpqAisdInfo_t {
    float sd_curr_type_scores[RK_HWPQ_AIPQ_MERGE_SCENE_TYPE_MAX]; 
    float sd_curr_content_scores[RK_HWPQ_AIPQ_MERGE_SCENE_CONTENT_MAX]; 
    int reserve0[4];
} RkHwpqAisdInfo;

typedef struct RkHwpqAipqMergeCfg_t {
    const char* p_aipq_cfg_file; 
    void* hwpq_cfg_addr; 
    int reserve0[4];
    RkHwpqAisdInfo* p_aisd_info;
    int reserve1[4];
} RkHwpqAipqMergeCfg;


#ifdef __cplusplus
}
#endif
#endif //__RKHWPQ_UTILS_AIPQ_MERGE_API_H_