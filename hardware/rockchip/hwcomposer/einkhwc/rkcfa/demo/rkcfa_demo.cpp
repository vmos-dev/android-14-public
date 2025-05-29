//
/////////////////////////////////////////////////////////////////////////
// Copyright(c) Rock-chip, All right reserved.
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
// Last update 2024-01-11 for librkcfa v0.1.0


#include "rkcfa_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_HARDWARE_BUFFER 0   /* use DRM like hardware buffers with fd values valid */

int main(void)
{
    int ret = 0;

    /* step 1. create a rkcfa_context to init */
    rkcfa_context ctx = 0;
    ret = rkcfa_init(&ctx);
    if (ret)
    {
        printf("Failed to init the rkcfa_context! %d\n", ret);
        return ret;
    }

    /* step 2. prepare the rkcfa_proc_params */
    rkcfa_proc_params param;
    memset(&param, 0, sizeof(rkcfa_proc_params));
    param.nImgWid = 1404;                   // unit: pixel
    param.nImgHgt = 1872;                   // unit: pixel
    param.nSrcWidStride = 1404 * 4;         // unit: byte,  x4 for RGBA8888 buffer
    param.nSrcHgtStride = 1872;             // unit: pixel, no padding on the input buffer in this case
    param.nDstWidStride = 1404 * 1;         // unit: byte,  x1 for gray buffer
    param.nDstHgtStride = 1872;             // unit: pixel, no padding on the output buffer in this case
    param.ePlatform = RKCFA_PLAT_EC078KH6;  // target hardware platform
    param.eAlgoType = RKCFA_TYPE_DEFAULT;   // CFA algorithm type
    param.eImgFormat = RKCFA_FMT_RGBA8888;  // input image format, one of the RGB family
    param.bDither = 1;                      // with dither ON or OFF
    param.nColorDepth = 64;                 // color depth,     range: [0, 128], default: 64
    param.nContrastGain = 64;               // contrast gain,   range: [0, 128], default: 64
    param.nSaturationGain = 64;             // saturation gain, range: [0, 128], default: 64
    param.nLuminanceGain = 64;              // luminance gain,  range: [0, 128], default: 64

    /* step 3. create & set the buffer info */
    uint8_t *pSrcBuf = nullptr;
    uint8_t *pDstBuf = nullptr;
#if USE_HARDWARE_BUFFER
    /* use the hardware buffers to enable zero-copy feature. Recommend */
    int fdSrc = 0, fdDst = 0;           // create hardware buffer then import through fd value
    int fdSrcIdx = 0, fdDstIdx = 0;     // create hardware buffer then import through fd value

    /* create hardware buffers to make 'fdSrc' & 'fdDst' valid... */
    /* ... */

    /* set the fd values to rkcfa_proc_params */
    param.nSrcBufFd = fdSrc;        // file descriptor of the input  hardware buffer
    param.nDstBufFd = fdDst;        // file descriptor of the output hardware buffer
    param.nSrcBufFdIdx = fdSrcIdx;  // unique identifier of the input  hardware buffer
    param.nDstBufFdIdx = fdDstIdx;  // unique identifier of the output hardware buffer
#else
    /* or malloc buffers directly, zero-copy will be disabled in this case. */
    pSrcBuf = (uint8_t*)malloc(param.nSrcWidStride * param.nSrcHgtStride);
    pDstBuf = (uint8_t*)malloc(param.nDstWidStride * param.nDstHgtStride);
#endif
    param.pSrcBuffer = pSrcBuf;
    param.pDstBuffer = pDstBuf;

    /* step 3. run the program after the parameter set */
    uint32_t frameIdx = 0;
    while (frameIdx < 5)
    {
        param.nFrameIdx = frameIdx++;

        /* update the input data */
        // fread(pSrcBuf, sizeof(uint8_t), param.nSrcWidStride * param.nSrcHgtStride, fpIn);

        /* run the program */
        ret = rkcfa_proc(ctx, &param);
        if (ret)
        {
            printf("Failed to run rkcfa_proc! %d\n", ret);
            break;
        }

        /* deal with the output data */
        // fwrite(pDstBuf, sizeof(uint8_t), param.nDstWidStride * param.nDstHgtStride, fpOut);
    }

    /* step 4. free the resource */
    ret = rkcfa_deinit(ctx);
    if (ret)
    {
        printf("Failed to release the rkcfa_context! %d\n", ret);
    }

#if USE_HARDWARE_BUFFER
    /* free hardware buffer here */
    /* ... */
#else
    /* free the buffers that you allocated */
    if (pSrcBuf != nullptr)
    {
        free(pSrcBuf);
        pSrcBuf = nullptr;
    }
    if (pDstBuf != nullptr)
    {
        free(pDstBuf);
        pDstBuf = nullptr;
    }
#endif

    return ret;
}