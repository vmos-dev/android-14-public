#pragma once
#ifndef __RKHWPQ_UTILS_VOP_BASE_CFG_API_H_
#define __RKHWPQ_UTILS_VOP_BASE_CFG_API_H_

#ifdef __cplusplus
extern "C"
{
#endif
/* Run Mode */
#define RK_HWPQ_UTILS_RUN_MODE_VOP_BASE_CFG     0x30

/* Process Command */
typedef enum{
    RK_HWPQ_UTILS_VOP_BASE_CFG_MIN = 0x100, 
    RK_HWPQ_UTILS_VOP_BASE_CFG_RUN,
    RK_HWPQ_UTILS_VOP_BASE_CFG_MAX, 
} RK_HWPQ_UTILS_VOP_BASE_CFG_CMD;


/* Hwpq base config structure */
typedef struct RkHwpqVopBaseCfg_t {
    const char* p_vop_base_cfg_file; 
    void* hwpq_cfg_addr; 
    int reserve0[16];
} RkHwpqVopBaseCfg;


#ifdef __cplusplus
}
#endif
#endif //__RKHWPQ_UTILS_VOP_BASE_CFG_API_H_