#pragma once
#ifndef __RKHWPQ_UTILS_API_H_
#define __RKHWPQ_UTILS_API_H_

#ifdef __cplusplus
extern "C"
{
#endif
#define RKHWPQ_CFG_IMPL_API_VERSION     00010000
#define RKHWPQ_CFG_IMPL_MAGIC_WORD      320  /* magic word for checking overwrite error 320 = H(72) + W(87) + P(80) + Q(81)*/

/* Error code */
typedef enum{
    RK_HWPQ_OK = 0,
    RK_HWPQ_ERR_INVALID_PAYLOAD_TYPE = -1,
    RK_HWPQ_ERR_INVALID_PAYLOAD_BUF = -2,
    RK_HWPQ_ERR_INVALID_PAYLOAD_IDX = -3,
    RK_HWPQ_ERR_INVALID_PAYLOAD_SIZE = -4,
    RK_HWPQ_ERR_FILE_OPEN_FAILED = -5,
    // TO ADD MORE ERROR CODES HERE

    RK_HWPQ_ERR_INVALID_META_BUF = -10,
    // TO ADD MORE ERROR CODES HERE

    RK_HWPQ_ERR_INVALID_RUN_MODE = -20,
    RK_HWPQ_ERR_INVALID_RUN_CMD = -21,

    RK_HWPQ_ERR_INVALID_VOP_CFG_ADDR = -30,
    RK_HWPQ_ERR_INVALID_VOP_CFG_JSON_FILE = -31,
    // TO ADD MORE ERROR CODES HERE

} RK_HWPQ_RET;

/* Hwpq utils structure */
/* Hwpq utils context*/
typedef void* RkHwpqUtilsCtx;

/* Function prototypes */
RK_HWPQ_RET hwpq_utils_init(
    RkHwpqUtilsCtx* p_hwpq_utils_ctx, 
    int hwpq_utils_run_mode
);

RK_HWPQ_RET hwpq_utils_process(
    RkHwpqUtilsCtx p_hwpq_utils_ctx, 
    void* p_hwpq_utils_proc_cfg, 
    int hwpq_utils_proc_cmd
);

RK_HWPQ_RET hwpq_utils_deinit(
    RkHwpqUtilsCtx p_hwpq_utils_ctx
);

#ifdef __cplusplus
}
#endif
#endif /* __RKHWPQ_UTILS_API_H_ */