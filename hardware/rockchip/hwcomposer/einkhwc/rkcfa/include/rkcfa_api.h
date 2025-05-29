//
/////////////////////////////////////////////////////////////////////////
// Copyright(c) 2024 by Rockchip Corp. All right reserved.
//
// This file is Rock-chip's property. It contains Rock-chip's trade secret,
// proprietary and confidential information.
// The information and code contained in this file is only for authorized
// Rock-chip employees to design, create, modify, or review.
// DO NOT DISTRIBUTE, DO NOT DUPLICATE OR TRANSMIT IN ANY FORM WITHOUT
// PROPER AUTHORIZATION.
// If you are not an intended recipient of this file, you must not copy,
// distribute, modify, or take any action in reliance on it.
// If you have received this file in error, please immediately notify
// Rock-chip and permanently delete the original and any copy of any file
// and any printout thereof.
//
//////////////////////////////////////////////////////////////////////////
//
// Last update on 2024-03-06

#ifndef _RKCFA_API_H_
#define _RKCFA_API_H_

#include <stdint.h>
#ifdef __linux__
#include <cstddef>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* forward declaration */
typedef void *rkcfa_context;

/* the CFA target platforms */
typedef enum _rkcfa_platform
{
    /* grayscale screen */
    RKCFA_PLAT_COMMON   = 0,  /* grayscale */
    /* corlor screen */
    RKCFA_PLAT_EC060KC1 = 1,    /* TBD */
    RKCFA_PLAT_EC060KH3 = 2,    /* TBD */
    RKCFA_PLAT_EC060KH4 = 3,    /* TBD */
    RKCFA_PLAT_EC070KC1 = 4,    /* TBD */
    RKCFA_PLAT_EC078KH3 = 5,    /* OK */
    RKCFA_PLAT_EC078KH6 = 6,    /* OK */
    RKCFA_PLAT_EC103KH3 = 7,    /* OK */
    RKCFA_PLAT_ED060KH6 = 8,    /* TBD */
} rkcfa_platform;

/* the CFA types */
typedef enum _rkcfa_type
{
    RKCFA_TYPE_DEFAULT  = 0x00, /* default */
    RKCFA_TYPE_REGAL    = 0x01, /* regal mode */
    RKCFA_TYPE_A2       = 0x02, /* fast dithering */
    RKCFA_TYPE_DU       = 0x04, /* reserved */
} rkcfa_type;

typedef enum _rkcfa_img_fmt {
    RKCFA_FMT_RGBA8888 = 0,     /* [31:0] A:B:G:R 8:8:8:8 */
    RKCFA_FMT_BGRA8888 = 1,     /* [31:0] A:R:G:B 8:8:8:8 */
    RKCFA_FMT_ARGB8888 = 2,     /* [31:0] B:G:R:A 8:8:8:8 */
    RKCFA_FMT_ABGR8888 = 3,     /* [31:0] R:G:B:A 8:8:8:8 */
    RKCFA_FMT_RGB888   = 4,     /* [23:0] B:G:R 8:8:8 */
    RKCFA_FMT_BGR888   = 5,     /* [23:0] R:G:B 8:8:8 */
    RKCFA_FMT_RGB565   = 6,     /* [15:0] R:G:B 5:6:5 */
    RKCFA_FMT_BGR565   = 7,     /* [15:0] B:G:R 5:6:5 */
    // RESERVED_TYPE_8 = 8,     /* reserved */
    // RESERVED_TYPE_9 = 9,     /* reserved */
    RKCFA_FMT_GRAY     = 10,    /* 8bit grayscale */
    RKCFA_FMT_PATTERN  = 11,    /* 8bit pattern color */
} rkcfa_img_fmt;

/* execution parameters */
typedef struct _rkcfa_proc_params
{
    // buffer info
    uint32_t        nFrameIdx;      // the frame index
    uint32_t        nImgWid;        // unit: pixel
    uint32_t        nImgHgt;        // unit: pixel
    uint32_t        nSrcWidStride;  // unit: byte
    uint32_t        nSrcHgtStride;  // unit: pixel
    uint32_t        nDstWidStride;  // unit: byte
    uint32_t        nDstHgtStride;  // unit: pixel
    uint8_t        *pSrcBuffer;     // the virtual address of the input  buffer
    uint8_t        *pDstBuffer;     // the virtual address of the output buffer
    int32_t         nSrcBufFd;      // for hardware input buffer
    int32_t         nDstBufFd;      // for hardware output buffer
    int32_t         nSrcBufFdIdx;   // for hardware input buffer
    int32_t         nDstBufFdIdx;   // for hardware output buffer

    // type & flags
    rkcfa_platform  ePlatform;      // target hardware platform
    rkcfa_type      eAlgoType;      // CFA algorithm type
    rkcfa_img_fmt   eImgFormat;     // input image format, one of the RGB family
    uint32_t        bDither;        // with dither ON or OFF
    uint32_t        nColorDepth;    // color depth,     range: [0, 128], default: 64
    uint32_t        nContrastGain;  // contrast gain,   range: [0, 128], default: 64
    uint32_t        nSaturationGain;// saturation gain, range: [0, 128], default: 64
    uint32_t        nLuminanceGain; // luminance gain,  range: [0, 128], default: 64

    uint32_t        aReserved[8];   // reserved data, for future use
} rkcfa_proc_params;

/**
 * @Function: rkcfa_init()
 * @Descrptn: rkcfa_context initialization, create a rkcfa_context instance.
 * @Params:
 *      rkcfa_context *pCtxPtr - a pointer of the pq handle of context
 * @Return: error code, 0 indicates everything is ok.
 */
int rkcfa_init(rkcfa_context *pCtxPtr, int flag = 0);

/**
 * @Function: rkcfa_proc()
 * @Descrptn: call a rkcfa_context instance to execute RKCFA
 * @Params:
 *      rkcfa_context ctx - the pq handle of context
 *      rkcfa_proc_params *pProcParam - a pointer of the API execution parameters
 * @Return: error code, 0 indicates everything is ok.
 */
int rkcfa_proc(rkcfa_context ctx, rkcfa_proc_params *pProcParam);

/**
 * @Function: rkcfa_deinit()
 * @Descrptn: release rkcfa_context resource created by rkcfa_init().
 * @Params:
 *      rkcfa_context ctx - the pq handle of context
 * @Return: error code, 0 indicates everything is ok.
 */
int rkcfa_deinit(rkcfa_context ctx);


#ifdef __cplusplus
}
#endif


#endif  //__RKCFA_API_H_
