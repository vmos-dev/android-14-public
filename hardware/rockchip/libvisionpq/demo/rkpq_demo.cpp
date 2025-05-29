//
/////////////////////////////////////////////////////////////////////////
// Copyright(c) 2024 by Rockchip Electronics Co., Ltd. All Rights Reserved.
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
// Last update 2024-04-10 for librkswpq.so v0.1.0


#include "rkpq_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USE_HARDWARE_BUFFER 0   /* use DRM like hardware buffers with fd values valid */

void optionalQueryContextInfo(rkpq_context ctx)
{
    rkpq_version_info verInfo;
    rkpq_imgfmt_info srcFmtInfo, dstFmtInfo;
    rkpq_clrspc_info clrSpcInfo;
    rkpq_module_info pqModuleInfo, roiModuleInfo;
    uint32_t isRknnSupport, imgAlignment, bufAlignment;
    rkpq_query(0, RKPQ_QUERY_SDK_VERSION, sizeof(rkpq_version_info), &verInfo);
    rkpq_query(0, RKPQ_QUERY_IMG_FMT_INPUT_SUPPORT, sizeof(rkpq_imgfmt_info), &srcFmtInfo);
    rkpq_query(0, RKPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT, sizeof(rkpq_imgfmt_info), &dstFmtInfo);
    rkpq_query(0, RKPQ_QUERY_IMG_COLOR_SPACE_SUPPORT, sizeof(rkpq_clrspc_info), &clrSpcInfo);
    /* NOTE: call ABOVE querys any time you need */
    /* NOTE: a valid context is needed for BELOW querys */
    rkpq_query(ctx, RKPQ_QUERY_MODULES_SUPPORT, sizeof(rkpq_module_info), &pqModuleInfo);
    rkpq_query(ctx, RKPQ_QUERY_MODULES_ROI_SUPPORT, sizeof(rkpq_module_info), &roiModuleInfo);
    rkpq_query(ctx, RKPQ_QUERY_IMG_ALIGNMENT_OCL, sizeof(uint32_t), &imgAlignment);
    rkpq_query(ctx, RKPQ_QUERY_BUF_ALIGNMENT_OCL, sizeof(uint32_t), &bufAlignment);
    rkpq_query(ctx, RKPQ_QUERY_RKNN_SUPPORT, sizeof(uint32_t), &isRknnSupport);
    printf("SDK version: %d.%d.%d, %s\n", verInfo.nVerMajor, verInfo.nVerMinor, verInfo.nVerRvson, verInfo.sVerInfo);
    printf("Support input/output image format num: %d/%d\n", srcFmtInfo.nValidFmtNum, dstFmtInfo.nValidFmtNum);
    printf("Support color space num: %d\n", clrSpcInfo.nValidSpcNum);
    printf("Support PQ module num: %d\n", pqModuleInfo.nValidModNum);
    printf("OpenCL image alignment: %u\n", imgAlignment);
    printf("OpenCL buffer alignment: %u\n", bufAlignment);
    printf("Is support RKNN: %u\n", isRknnSupport);
}

/* Input -> AI_DM -> Output */
int setPipelineDemoDM(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam);
/* Input -> CSC -> AI_SR -> Output */
int setPipelineDemoSR(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam);
/* Input -> [CSC] -> DCI -> ACM -> ZME -> [CSC] -> Output */
int setPipelineDemoZME(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam);
/* Inpu -> AI_DFC -> Output */
int setPipelineDemoDFC(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam);


//////////////////////////////////////////////////////////////////////////
////---- main
int main(int argc, const char *argv[])
{
    int ret = 0;
    int demoCase = -1;
    int logLevel = 4;
    if (argc > 1) {
        demoCase = atoi(argv[1]);
    }
    if (argc > 2) {
        logLevel = atoi(argv[2]);
    }
    printf("Demo case: %d\n", demoCase);
    printf("logLevel: %d\n", logLevel);

    rkpq_set_loglevel(nullptr, logLevel);

    /* step 1. create a 'rkpq_init_params' and call rkpq_init() to init a rkpq_context */
    rkpq_init_params initParams = {0};
    initParams.nInitFlag = RKPQ_FLAG_DEFAULT;
    // initParams.nInitFlag |= RKPQ_FLAG_FIX_RES_IMG_IO;   // set this IF the IO image resolutions are fixed
    // initParams.nInitFlag |= RKPQ_FLAG_CIRCULAR_BUF_IN;  // set this IF the input (dma) buffer is on a circular pool. Recommend
    // initParams.nInitFlag |= RKPQ_FLAG_CIRCULAR_BUF_OUT; // set this IF the output (dma) buffer is on a circular pool. Recommend
    // initParams.nInitFlag |= RKPQ_FLAG_ASYNC_MODE;       // set this IF want to use async mode. Reserved
    initParams.nExtenFlag = 0;  // reserved

    // choose a pipeline demo setting below
    switch (demoCase)
    {
    case 0:
        setPipelineDemoDM(nullptr, &initParams, nullptr);   // demo that use AI_DM to reduce mosquito noise
        break;
    case 1:
        setPipelineDemoSR(nullptr, &initParams, nullptr);   // demo that use AI_SR to do super resolution
        break;
    case 2:
        setPipelineDemoDFC(nullptr, &initParams, nullptr);  // demo that use AI_DFC to do de-false-coloring
        break;
    default:
        setPipelineDemoZME(nullptr, &initParams, nullptr);  // demo that use ZME to do arbitrary resolution scaling
        break;
    }

    rkpq_context context;
    ret = rkpq_init(&context, &initParams); // the arguments are two pointers
    if (ret)
    {
        printf("Failed to call rkpq_init() %d, Please check the API parameters!\n", ret);
        return ret;
    }

    /* query infos if needed */
    optionalQueryContextInfo(context);


    /* step 2. create a 'rkpq_proc_params' object and set the basic image buffer info */
    rkpq_proc_params procParams = {0};

    // choose a pipeline demo setting below
    switch (demoCase)
    {
    case 0:
        setPipelineDemoDM(context, nullptr, &procParams);   // demo that use AI_DM to reduce mosquito noise
        break;
    case 1:
        setPipelineDemoSR(context, nullptr, &procParams);   // demo that use AI_SR to do super resolution
        break;
    case 2:
        setPipelineDemoDFC(context, nullptr, &procParams);  // demo that use AI_DFC to do de-false-coloring
        break;
    default:
        setPipelineDemoZME(context, nullptr, &procParams);  // demo that use ZME to do arbitrary resolution scaling
        break;
    }

    /* 3.1 set the buffer info for single-input-output modules */
    rkpq_imgbuf_info &stSrcImgInfo = procParams.stSrcImgInfo;
    rkpq_imgbuf_info &stDstImgInfo = procParams.stDstImgInfo;
    size_t srcBufSize = 0, dstBufSize = 0;
#if USE_HARDWARE_BUFFER
    // set buffer info with imported hardware buffer of input & output. Recommend
    int fdSrc = 0, fdDst = 0;           // create hardware buffer then import through fd value
    int fdSrcIdx = 0, fdDstIdx = 0;     // create hardware buffer then import through fd value
    /* create hardware buffers to make 'fdSrc' & 'fdDst' valid... */
    /* ... */
    // set the fd values
    stSrcImgInfo.nFdValue = fdSrc;
    stDstImgInfo.nFdValue = fdDst;
    stSrcImgInfo.nFdIndex = fdSrcIdx;   // unique identifier of the device buffer
    stDstImgInfo.nFdIndex = fdDstIdx;
    // srcBufSize = <size of the input  buffer you created>;
    // dstBufSize = <size of the output buffer you created>;
#else
    // or malloc input & output bufs, zero-copy will be disabled in this case.
    srcBufSize = stSrcImgInfo.nFrameSize;
    dstBufSize = stDstImgInfo.nFrameSize;
    stSrcImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(srcBufSize);
    stDstImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(dstBufSize);
#endif
    stSrcImgInfo.nBufferSize = srcBufSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */
    stDstImgInfo.nBufferSize = dstBufSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */

    /* 3.2 set other proc parameters */
    procParams.bEnablePropControl   = 0;    // set to 1 if want to use `adb properties` to control the effects
    procParams.bEnableSliderControl = 0;    // set to 1 if want to use `PQTool` to control the effects (works only when bEnablePropControl=0)
    // set ROI to the area you want, value range: [0, dst_width] / [0, dst_height]
    // below ROI set the processing area to the right half side of the source image
    procParams.stImgRoi.bEnableRoi = 0;     // set to 1 if want to apply ROI feature
    procParams.stImgRoi.x = stDstImgInfo.nPixWid / 2;
    procParams.stImgRoi.y = 0;
    procParams.stImgRoi.w = stDstImgInfo.nPixWid / 2;
    procParams.stImgRoi.h = stDstImgInfo.nPixHgt;
    procParams.nExtenType = 0;      // reserved, set to 0
    procParams.pExtenConfig = NULL; // reserved, set to NULL

    /* step 4. call rkpq_proc() to run */
    int frameIdx = 0;
    int exitFlag = 0;
    while (exitFlag != 1)   /* break condition */
    {
        // input: read data to fill stSrcImgInfo.aPlaneAddrs
        /*
        fread(stSrcImgInfo.aPlaneAddrs[0], sizeof(unsigned char), stSrcImgInfo.nFrameSize, fpIn);
        */

        /* for multi-input-output modules,
         *  - use 'rkpq_set_inputs()' instead of 'rkpq_proc_params::stSrcImgInfo',
         *  - use 'rkpq_set_outputs()' instead of 'rkpq_proc_params::stDstImgInfo'
         **/
        // rkpq_set_inputs(context, &stSrcImgInfo, 1);
        // rkpq_set_outputs(context, &stDstImgInfo, 1);

        procParams.nFrameIdx = frameIdx;

        ret = rkpq_proc(context, &procParams);
        if (ret)
        {
            printf("Fail to rkpq_proc() %d\n", ret);
            break;
        }

        // output: stDstImgInfo.aPlaneAddrs
        /*
        fread(stDstImgInfo.aPlaneAddrs[0], sizeof(unsigned char), stDstImgInfo.nFrameSize, fpOut);
        */

        frameIdx++;
        exitFlag = frameIdx > 1;
    }

    /* step 5. call rkpq_deinit() to free the resource */
    ret = rkpq_deinit(context);
    if (ret)
    {
        printf("Failed to call rkpq_deinit() %d\n", ret);
    }

#if USE_HARDWARE_BUFFER
    /* free hardware buffer here */
    /* ... */
#else
    // free the buffers that you allocated
    if (stSrcImgInfo.aPlaneAddrs[0] != NULL)
    {
        free(stSrcImgInfo.aPlaneAddrs[0]);
        stSrcImgInfo.aPlaneAddrs[0] = NULL;
    }
    if (stDstImgInfo.aPlaneAddrs[0] != NULL)
    {
        free(stDstImgInfo.aPlaneAddrs[0]);
        stDstImgInfo.aPlaneAddrs[0] = NULL;
    }
#endif

    return ret;
}

//////////////////////////////////////////////////////////////////////////
////---- pipeline demos

/* Below code shows a pipeline example for the only DM module */
/* The data flow is: [1920x1080 yuv420sp] =DM=> [1920x1080 yuv420sp] */
int setPipelineDemoDM(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam)
{
    int ret = 0;

    /* set init parameters */
    if (pInitParam)
    {
        /* set the custom pipeline order */
        pInitParam->aModPipeOrder[0] = RKPQ_MODULE_AI_DM;
        pInitParam->nModNumInPipe = 1;  // only 1 module in the pipeline
    }

    /* set proc parameters */
    if (pProcParam)
    {
        rkpq_imgbuf_info &stSrcImgInfo = pProcParam->stSrcImgInfo;
        rkpq_imgbuf_info &stDstImgInfo = pProcParam->stDstImgInfo;

        /* set the image info */
        stSrcImgInfo.nColorSpace = RKPQ_CLR_SPC_YUV_601_FULL;   // see rkpq_clr_spc, full-range here
        stSrcImgInfo.nPixFmt = RKPQ_IMG_FMT_NV12;               // see rkpq_img_fmt, YUV420SP_NV12 here
        stSrcImgInfo.nPixWid = 1920;                            // set the input image width
        stSrcImgInfo.nPixHgt = 1080;                            // set the input image height
        stSrcImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stSrcImgInfo.nAlignment = 64;                           // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
        stSrcImgInfo.aWidStrides[0] = 1920;                     // in this case, no padding in the width dimension
        stSrcImgInfo.aHgtStrides[0] = 1088;                     // in this case, there are 8 lines padding after the first (luma) plane
        stSrcImgInfo.aHgtStrides[1] = 540;                      // in this case, no padding after the second (chroma) semi-plane

        memcpy(&stDstImgInfo, &stSrcImgInfo, sizeof(rkpq_imgbuf_info)); // the output format is the same to the input format
        stDstImgInfo.nAlignment = 0;                            // no alignment, aWidStrides[0] will be set to nPixWid (1920) automatically
        stDstImgInfo.aHgtStrides[0] = 0;                        // no padding after the first (luma) plane, unlike the input data

        /** (optional)
         * call rkpq_query() to get the remain image buffer infos
         * no need to call rkpq_query() ONLY IF you know and filled all the rkpq_imgbuf_info of the src & dst images
         */
    #if 1 /* not necessary */
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stSrcImgInfo);
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stDstImgInfo);
        if (ret)
        {
            printf("Failed to call rkpq_query() %d, Please check the API parameters!\n", ret);
            return ret;
        }

        /* !!! change the width / height stride IF the query result of rkpq_imgbuf_info is different with the real buffer !!! */
        // stDstImgInfo.aWidStrides[1] = 1920; // 1856 -> 1920, padding aligns to 64 bytes on the chroma plane
        // stDstImgInfo.aPlaneSizes[1] += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the chroma plane size
        // stDstImgInfo.nFrameSize += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the total frame size
    #endif
    }

    /* (optional) get the module tuning configs and change the parameter manually if necessary */
    if (ctx)
    {
        rkpq_dm_cfg *pDmConfig = (rkpq_dm_cfg *)rkpq_get_default_cfg(ctx, 0, RKPQ_MODULE_AI_DM);   // order 0 is the DM module in the pipeline
        pDmConfig->bEnableDM = pDmConfig->bEnableAIDM = 1;  // enable AIDM feature, the DM module will run with NPU (need RKNN SDK v1.4)
        /* (optional) adjust the specified configuration manually */
        // pDmConfig->nProcessRatio = 156;
        // pDmConfig->nDenoiseThresholdLow = 2;
        // pDmConfig->nDenoiseThresholdHigh = 60;
    }

    return ret;
}

/* Below code shows a pipeline example for SR modules: CSC => SR */
/* The data flow is: [1920x1080 rgb888 (limited-range)] =CSC=> [1920x1080 yuv444sp (full-range)] =SR=> [3840x2160 yuv422sp] */
int setPipelineDemoSR(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam)
{
    int ret = 0;

    /* set init parameters */
    if (pInitParam)
    {
        /* set the custom pipeline order */
        int order = 0;
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_CSC;   /* order 0 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_AI_SR; /* order 1 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_CSC;   /* order 2 */
        pInitParam->nModNumInPipe = order;                      /* module num in the pipeline */
    }

    /* set proc parameters */
    if (pProcParam && ctx)
    {
        rkpq_imgbuf_info &stSrcImgInfo = pProcParam->stSrcImgInfo;
        rkpq_imgbuf_info &stDstImgInfo = pProcParam->stDstImgInfo;

        /* set the image info */
        stSrcImgInfo.nColorSpace = RKPQ_CLR_SPC_RGB_LIMITED;    // see rkpq_clr_spc, limited-range here
        stSrcImgInfo.nPixFmt = RKPQ_IMG_FMT_BGR;                // see rkpq_img_fmt, BGR888 here
        stSrcImgInfo.nPixWid = 1920;                            // set the input image width
        stSrcImgInfo.nPixHgt = 1080;                            // set the input image height
        stSrcImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stSrcImgInfo.nAlignment = 64;                           // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
        stDstImgInfo.nColorSpace = RKPQ_CLR_SPC_YUV_709_FULL;   // see rkpq_clr_spc, full-range here
        stDstImgInfo.nPixFmt = RKPQ_IMG_FMT_NV16;               // see rkpq_img_fmt, YUV422SP_NV16 here
        stDstImgInfo.nPixWid = 3840;                            // set the output image width
        stDstImgInfo.nPixHgt = 2160;                            // set the output image height
        stDstImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stDstImgInfo.nAlignment = 0;                            // aWidStrides[0] will be set to 3840 when rkpq_query() called with 'RKPQ_QUERY_IMG_BUF_INFO'
        stDstImgInfo.aWidStrides[0] = 3840;                     // or you can set it manually according to the real case

        /** (optional)
         * call rkpq_query() to get the remain image buffer infos
         * no need to call rkpq_query() ONLY IF you know and filled all the rkpq_imgbuf_info of the src & dst images
         */
    #if 1 /* not necessary */
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stSrcImgInfo);
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stDstImgInfo);
        if (ret)
        {
            printf("Failed to call rkpq_query() %d, Please check the API parameters!\n", ret);
            return ret;
        }

        /* !!! change the width / height stride IF the query result of rkpq_imgbuf_info is different with the real buffer !!! */
        // stDstImgInfo.aWidStrides[1] = 1920; // 1856 -> 1920, padding aligns to 64 bytes on the chroma plane
        // stDstImgInfo.aPlaneSizes[1] += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the chroma plane size
        // stDstImgInfo.nFrameSize += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the total frame size
    #endif

        /* set the buffer info */
    #if 1
        // set buffer info with imported hardware buffer of input & output. Recommend
        int fdSrc = 0, fdDst = 0;           // create hardware buffer then import through fd value
        int fdSrcIdx = 0, fdDstIdx = 0;     // create hardware buffer then import through fd value
        /* create hardware buffers to make 'fdSrc' & 'fdDst' valid... */
        /* ... */
        // set the fd values
        stSrcImgInfo.nFdValue = fdSrc;
        stDstImgInfo.nFdValue = fdDst;
        stSrcImgInfo.nFdIndex = fdSrcIdx;   // unique identifier of the device buffer
        stDstImgInfo.nFdIndex = fdDstIdx;
    #else
        // or malloc input & output bufs
        stSrcImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(stSrcImgInfo.nFrameSize);  // please make sure the src buffer is valid and >= stSrcImgInfo.nFrameSize
        stDstImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(stDstImgInfo.nFrameSize);  // please make sure the dst buffer is valid and >= stDstImgInfo.nFrameSize
    #endif
        stSrcImgInfo.nBufferSize = stSrcImgInfo.nFrameSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */
        stDstImgInfo.nBufferSize = stDstImgInfo.nFrameSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */

        /* set other proc parameters */
        pProcParam->bEnablePropControl   = 0;    // set to 1 if want to use `adb properties` to control the effects
        pProcParam->bEnableSliderControl = 0;    // set to 1 if want to use `PQTool` to control the effects (works only when bEnablePropControl=0)
        // set ROI to the area you want, value range: [0, src_width] / [0, src_height]
        // below ROI set the processing area to the right half side of the source image
        pProcParam->stImgRoi.bEnableRoi = 0;    // set to 1 if want to apply ROI feature
        pProcParam->stImgRoi.x = stSrcImgInfo.nPixWid / 2;
        pProcParam->stImgRoi.y = 0;
        pProcParam->stImgRoi.w = stSrcImgInfo.nPixWid / 2;
        pProcParam->stImgRoi.h = stSrcImgInfo.nPixHgt;
        pProcParam->nExtenType = 0;      // reserved, set to 0
        pProcParam->pExtenConfig = NULL; // reserved, set to NULL

        /* set the 'rkpq_pipe_fmt_info' & 'rkpq_pipe_res_info' since the image format or resolution will change in the pipeline! */
        rkpq_csc_cfg *pCscConfig0 = (rkpq_csc_cfg *)rkpq_get_default_cfg(ctx, 0, RKPQ_MODULE_CSC);  // order 0 is the first CSC module in the pipeline
        rkpq_sr_cfg  *pSrConfig   = (rkpq_sr_cfg  *)rkpq_get_default_cfg(ctx, 1, RKPQ_MODULE_AI_SR);// order 1 is the        SR module in the pipeline
        // rkpq_csc_cfg *pCscConfig1 = (rkpq_csc_cfg *)rkpq_get_default_cfg(ctx, 2, RKPQ_MODULE_CSC);  // order 2 is the last  CSC module in the pipeline
        // set the first CSC pipeline format change info: from the src pixel format to YUV444SP_NV24
        pCscConfig0->stPipeFmtInfo.nSrcClrSpc = stSrcImgInfo.nColorSpace;
        pCscConfig0->stPipeFmtInfo.nSrcPixFmt = stSrcImgInfo.nPixFmt;
        pCscConfig0->stPipeFmtInfo.nDstClrSpc = RKPQ_CLR_SPC_YUV_709_FULL;
        pCscConfig0->stPipeFmtInfo.nDstPixFmt = RKPQ_IMG_FMT_NV24;
        // set the last CSC pipeline format change info: from YUV444SP_NV24 to the dst pixel format
        // pCscConfig1->stPipeFmtInfo.nSrcClrSpc = RKPQ_CLR_SPC_YUV_709_FULL;
        // pCscConfig1->stPipeFmtInfo.nSrcPixFmt = RKPQ_IMG_FMT_NV24;
        // pCscConfig1->stPipeFmtInfo.nDstClrSpc = stDstImgInfo.nColorSpace;
        // pCscConfig1->stPipeFmtInfo.nDstPixFmt = stDstImgInfo.nPixFmt;
        // set the SR pipeline format change info: remain YUV444SP_NV24 unchanged
        pSrConfig->stPipeFmtInfo.nSrcClrSpc = RKPQ_CLR_SPC_YUV_709_FULL;
        pSrConfig->stPipeFmtInfo.nSrcPixFmt = RKPQ_IMG_FMT_NV24;
        pSrConfig->stPipeFmtInfo.nDstClrSpc = RKPQ_CLR_SPC_YUV_709_FULL;
        pSrConfig->stPipeFmtInfo.nDstPixFmt = RKPQ_IMG_FMT_NV16;
        // set the SR pipeline resolution change info: form src size (1920x1080) to dst size (3840x2160)
        pSrConfig->stPipeResInfo.nSrcImgWid = stSrcImgInfo.nPixWid;
        pSrConfig->stPipeResInfo.nSrcImgHgt = stSrcImgInfo.nPixHgt;
        pSrConfig->stPipeResInfo.nDstImgWid = stDstImgInfo.nPixWid;
        pSrConfig->stPipeResInfo.nDstImgHgt = stDstImgInfo.nPixHgt;

        /* (optional) adjust the specified configuration manually */
        pSrConfig->bEnableAISR = 1;       // enbale AISR feature, the SR module will run with NPU (need RKNN SDK v1.4)
        // pSrConfig->nColorStrength = 160;  // set AISR color intensity to 1.25(=160/128)
        // pCscConfig0->nSaturation = 384;   // set CSC saturation gain to 1.5(=384/256)
    }

    return ret;
}

/* Below code shows a pipeline example for ZME modules: CSC => DCI => ACM => ZME => SHP */
/* The data flow is: [1920x1080 rgb888 (limited-range)] =CSC=> [1920x1080 yuv444sp (full-range)]
  =DCI,ACM=> [1920x1080 yuv444sp (full-range)] =ZME=> [3840x2160 yuv422sp] =SHP=> [3840x2160 yuv422sp] */
int setPipelineDemoZME(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam)
{
    int ret = 0;

    /* set init parameters */
    if (pInitParam)
    {
        /* set the custom pipeline order */
        int order = 0;
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_CSC;   /* order 0 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_DCI;   /* order 1 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_ACM;   /* order 2 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_ZME;   /* order 3 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_SHP;   /* order 4 */
        pInitParam->aModPipeOrder[order++] = RKPQ_MODULE_CSC;   /* order 5 */
        pInitParam->nModNumInPipe = order;                      /* module num in the pipeline */
    }

    /* set proc parameters */
    if (pProcParam && ctx)
    {
        rkpq_imgbuf_info &stSrcImgInfo = pProcParam->stSrcImgInfo;
        rkpq_imgbuf_info &stDstImgInfo = pProcParam->stDstImgInfo;

        /* set the image info */
        stSrcImgInfo.nColorSpace = RKPQ_CLR_SPC_RGB_LIMITED;    // see rkpq_clr_spc, limited-range here
        stSrcImgInfo.nPixFmt = RKPQ_IMG_FMT_BGR;                // see rkpq_img_fmt, BGR888 here
        stSrcImgInfo.nPixWid = 1920;                            // set the input image width
        stSrcImgInfo.nPixHgt = 1080;                            // set the input image height
        stSrcImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stSrcImgInfo.nAlignment = 64;                           // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
        stDstImgInfo.nColorSpace = RKPQ_CLR_SPC_YUV_709_FULL;   // see rkpq_clr_spc, full-range here
        stDstImgInfo.nPixFmt = RKPQ_IMG_FMT_NV16;               // see rkpq_img_fmt, YUV422SP_NV16 here
        stDstImgInfo.nPixWid = 3840;                            // set the output image width
        stDstImgInfo.nPixHgt = 2160;                            // set the output image height
        stDstImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stDstImgInfo.nAlignment = 0;                            // aWidStrides[0] will be set to 3840 when rkpq_query() called with 'RKPQ_QUERY_IMG_BUF_INFO'
        stDstImgInfo.aWidStrides[0] = 3840;                     // or you can set it manually according to the real case

        /** (optional)
         * call rkpq_query() to get the remain image buffer infos
         * no need to call rkpq_query() ONLY IF you know and filled all the rkpq_imgbuf_info of the src & dst images
         */
    #if 1 /* not necessary */
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stSrcImgInfo);
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stDstImgInfo);
        if (ret)
        {
            printf("Failed to call rkpq_query() %d, Please check the API parameters!\n", ret);
            return ret;
        }

        /* !!! change the width / height stride IF the query result of rkpq_imgbuf_info is different with the real buffer !!! */
        // stDstImgInfo.aWidStrides[1] = 1920; // 1856 -> 1920, padding aligns to 64 bytes on the chroma plane
        // stDstImgInfo.aPlaneSizes[1] += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the chroma plane size
        // stDstImgInfo.nFrameSize += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the total frame size
    #endif

        /* set the buffer info */
    #if 1
        // set buffer info with imported hardware buffer of input & output. Recommend
        int fdSrc = 0, fdDst = 0;           // create hardware buffer then import through fd value
        int fdSrcIdx = 0, fdDstIdx = 0;     // create hardware buffer then import through fd value
        /* create hardware buffers to make 'fdSrc' & 'fdDst' valid... */
        /* ... */
        // set the fd values
        stSrcImgInfo.nFdValue = fdSrc;
        stDstImgInfo.nFdValue = fdDst;
        stSrcImgInfo.nFdIndex = fdSrcIdx;   // unique identifier of the device buffer
        stDstImgInfo.nFdIndex = fdDstIdx;
    #else
        // or malloc input & output bufs
        stSrcImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(stSrcImgInfo.nFrameSize);  // please make sure the src buffer is valid and >= stSrcImgInfo.nFrameSize
        stDstImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(stDstImgInfo.nFrameSize);  // please make sure the dst buffer is valid and >= stDstImgInfo.nFrameSize
    #endif
        stSrcImgInfo.nBufferSize = stSrcImgInfo.nFrameSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */
        stDstImgInfo.nBufferSize = stDstImgInfo.nFrameSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */

        /* set other proc parameters */
        pProcParam->bEnablePropControl   = 0;    // set to 1 if want to use `adb properties` to control the effects
        pProcParam->bEnableSliderControl = 0;    // set to 1 if want to use `PQTool` to control the effects (works only when bEnablePropControl=0)
        // set ROI to the area you want, value range: [0, src_width] / [0, src_height]
        // below ROI set the processing area to the right half side of the source image
        pProcParam->stImgRoi.bEnableRoi = 0;    // set to 1 if want to apply ROI feature
        pProcParam->stImgRoi.x = stSrcImgInfo.nPixWid / 2;
        pProcParam->stImgRoi.y = 0;
        pProcParam->stImgRoi.w = stSrcImgInfo.nPixWid / 2;
        pProcParam->stImgRoi.h = stSrcImgInfo.nPixHgt;
        pProcParam->nExtenType = 0;      // reserved, set to 0
        pProcParam->pExtenConfig = NULL; // reserved, set to NULL

        /* set the 'rkpq_pipe_fmt_info' & 'rkpq_pipe_res_info' since the image format or resolution will change in the pipeline! */
        rkpq_csc_cfg *pCscConfig0 = (rkpq_csc_cfg *)rkpq_get_default_cfg(ctx, 0, RKPQ_MODULE_CSC); // order 0 is the 1st module (CSC) in the pipeline
        rkpq_dci_cfg *pDciConfig = (rkpq_dci_cfg *)rkpq_get_default_cfg(ctx, 1, RKPQ_MODULE_DCI);  // order 1 is the 2nd module (DCI) in the pipeline
        rkpq_acm_cfg *pAcmConfig = (rkpq_acm_cfg *)rkpq_get_default_cfg(ctx, 2, RKPQ_MODULE_ACM);  // order 2 is the 3rd module (ACM) in the pipeline
        rkpq_zme_cfg *pZmeConfig = (rkpq_zme_cfg *)rkpq_get_default_cfg(ctx, 3, RKPQ_MODULE_ZME);  // order 3 is the 4th module (ZME) in the pipeline
        rkpq_shp_cfg *pShpConfig = (rkpq_shp_cfg *)rkpq_get_default_cfg(ctx, 4, RKPQ_MODULE_SHP);  // order 4 is the 5th module (SHP) in the pipeline
        rkpq_csc_cfg *pCscConfig1 = (rkpq_csc_cfg *)rkpq_get_default_cfg(ctx, 5, RKPQ_MODULE_CSC); // order 4 is the last module(CSC) in the pipeline

        // set the CSC pipeline format change info: from the src pixel format to YUV444SP_NV24
        //!NOTE: CSC模块输入格式和色彩空间一定和src一致
        //!NOTE: 由于后续的DCI/ACM模块只支持YUV格式, 故这里CSC的输出格式必须是YUV，且色彩空间必须是YUV full-range
        if (pCscConfig0)
        {
            pCscConfig0->stPipeFmtInfo.nSrcClrSpc = stSrcImgInfo.nColorSpace;
            pCscConfig0->stPipeFmtInfo.nSrcPixFmt = stSrcImgInfo.nPixFmt;
            if (stDstImgInfo.nPixFmt < RKPQ_IMG_FMT_YUV_MAX) {
                //!NOTE: 如果目标dst格式是YUV(如NV12), 这里可以直接输出成目标格式(不必非得转成NV24,可以减少计算量), 但色彩空间必须是full-range
                pCscConfig0->stPipeFmtInfo.nDstClrSpc = stDstImgInfo.nColorSpace | 1; // to full-range
                pCscConfig0->stPipeFmtInfo.nDstPixFmt = stDstImgInfo.nPixFmt;  // use as the dst format
            } else {
                //!NOTE: 如果目标dst格式不是YUV(如RGBA), 这里建议转成YUV444(NV24), 色彩空间必须是full-range
                pCscConfig0->stPipeFmtInfo.nDstClrSpc = RKPQ_CLR_SPC_YUV_709_FULL; // default 709F
                pCscConfig0->stPipeFmtInfo.nDstPixFmt = RKPQ_IMG_FMT_NV24; // default RGB->NV24
            }
        } else {
            printf("Warning: failed to get the CSC_0 config!\n");
            return -1;
        }
        if (pZmeConfig)
        {
            // set the ZME pipeline format change info: from YUV444SP_NV24 to the dst pixel format
            //!NOTE: 由于前面两个模块(DCI/ACM)都不改变输入输出图像格式，故这里ZME的输入格式必定是CSC的输出格式，输出一般不改变格式
            pZmeConfig->stPipeFmtInfo.nSrcClrSpc = pCscConfig0->stPipeFmtInfo.nDstClrSpc;
            pZmeConfig->stPipeFmtInfo.nSrcPixFmt = pCscConfig0->stPipeFmtInfo.nDstPixFmt;
            pZmeConfig->stPipeFmtInfo.nDstClrSpc = pCscConfig0->stPipeFmtInfo.nDstClrSpc;
            pZmeConfig->stPipeFmtInfo.nDstPixFmt = pCscConfig0->stPipeFmtInfo.nDstPixFmt;
            //!NOTE: ZME的输出在YUV格式的情况下可以直接在YUV4xxsp(NV24/NV16/NV12)之间转换，这里也可以不要此步骤，直接交给最后一级CSC完成转换
            // if (stDstImgInfo.nPixFmt <= RKPQ_IMG_FMT_NV12) {
            //     pZmeConfig->stPipeFmtInfo.nDstPixFmt = stDstImgInfo.nPixFmt;
            // }
            // set the ZME pipeline resolution change info: form src size (1920x1080 YUV444SP_NV24) to dst size (3840x2160 YUV422SP_NV16)
            //!NOTE: ZME模块前/后面均没有其他模块会改变图像的分辨率，故这里的输入/输出分辨率一定是src和dst的分辨率
            pZmeConfig->stPipeResInfo.nSrcImgWid = stSrcImgInfo.nPixWid;
            pZmeConfig->stPipeResInfo.nSrcImgHgt = stSrcImgInfo.nPixHgt;
            pZmeConfig->stPipeResInfo.nDstImgWid = stDstImgInfo.nPixWid;
            pZmeConfig->stPipeResInfo.nDstImgHgt = stDstImgInfo.nPixHgt;
        }

        /* (optional) adjust the specified configuration manually */
        // pCscConfig0->nColorTemperature = 7000;  // set CSC color temperature to 7000K
        // pDciConfig->bEnableDCI = 0;             // disable DCI module in the pipeline
        // pAcmConfig->nLumGain = 256;             // set ACM luminance gain to 256(1.0f)
        // pShpConfig->bEnableShootCtrl = 1;       // enable SHP shooting control
    }
    return ret;
}

/* Below code shows a pipeline example for the only DFC module */
/* The data flow is: [1920x1080 yuv420sp] =DFC=> [1920x1080 yuv444sp] or [1920x1080 RGBA8888] */
int setPipelineDemoDFC(rkpq_context ctx, rkpq_init_params *pInitParam, rkpq_proc_params *pProcParam)
{
    int ret = 0;

    /* set init parameters */
    if (pInitParam)
    {
        /* set the custom pipeline order */
        pInitParam->aModPipeOrder[0] = RKPQ_MODULE_AI_DFC;
        pInitParam->nModNumInPipe = 1;  // only 1 module in the pipeline
    }

    /* set proc parameters */
    if (pProcParam)
    {
        rkpq_imgbuf_info &stSrcImgInfo = pProcParam->stSrcImgInfo;
        rkpq_imgbuf_info &stDstImgInfo = pProcParam->stDstImgInfo;

        /* set the src image info */
        stSrcImgInfo.nColorSpace = RKPQ_CLR_SPC_YUV_601_LIMITED;// see rkpq_clr_spc, full-range here
        stSrcImgInfo.nPixFmt = RKPQ_IMG_FMT_NV12;               // see rkpq_img_fmt, YUV420SP_NV12 here
        stSrcImgInfo.nPixWid = 1920;                            // set the input image width
        stSrcImgInfo.nPixHgt = 1080;                            // set the input image height
        stSrcImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stSrcImgInfo.nAlignment = 128;                          // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
        stSrcImgInfo.aWidStrides[0] = 1920;                     // in this case, no padding in the width dimension
        stSrcImgInfo.aHgtStrides[0] = 1080;                     // in this case, there are 8 lines padding after the first (luma) plane
        stSrcImgInfo.aHgtStrides[1] = 540;                      // in this case, no padding after the second (chroma) semi-plane

        /* set the dst image info */
    #if 1 // NV24
        stDstImgInfo.nColorSpace = RKPQ_CLR_SPC_YUV_601_LIMITED;// see rkpq_clr_spc, full-range here
        stDstImgInfo.nPixFmt = RKPQ_IMG_FMT_NV24;               // see rkpq_img_fmt, YUV420SP_NV12 here
        stDstImgInfo.nPixWid = 1920;                            // set the input image width
        stDstImgInfo.nPixHgt = 1080;                            // set the input image height
        stDstImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stDstImgInfo.nAlignment = 128;                          // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
        stDstImgInfo.aWidStrides[0] = 1920;                     // in this case, no padding in the width dimension
        stDstImgInfo.aHgtStrides[0] = 1080;                     // in this case, there are 8 lines padding after the first (luma) plane
        stDstImgInfo.aHgtStrides[1] = 1080;                     // in this case, no padding after the second (chroma) semi-plane
    #else // RGBA
        stDstImgInfo.nColorSpace = RKPQ_CLR_SPC_RGB_FULL;// see rkpq_clr_spc, full-range here
        stDstImgInfo.nPixFmt = RKPQ_IMG_FMT_RGBA;               // see rkpq_img_fmt, YUV420SP_NV12 here
        stDstImgInfo.nPixWid = 1920;                            // set the input image width
        stDstImgInfo.nPixHgt = 1080;                            // set the input image height
        stDstImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
        stDstImgInfo.nAlignment = 128;                          // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
        stDstImgInfo.aWidStrides[0] = 7680;                     // in this case, no padding in the width dimension
        stDstImgInfo.aHgtStrides[0] = 1080;                     // in this case, there are 8 lines padding after the first (luma) plane
    #endif

        /** (optional)
         * call rkpq_query() to get the remain image buffer infos
         * no need to call rkpq_query() ONLY IF you know and filled all the rkpq_imgbuf_info of the src & dst images
         */
    #if 1 /* not necessary */
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stSrcImgInfo);
        ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stDstImgInfo);
        if (ret)
        {
            printf("Failed to call rkpq_query() %d, Please check the API parameters!\n", ret);
            return ret;
        }

        /* !!! change the width / height stride IF the query result of rkpq_imgbuf_info is different with the real buffer !!! */
        // stDstImgInfo.aWidStrides[1] = 1920; // 1856 -> 1920, padding aligns to 64 bytes on the chroma plane
        // stDstImgInfo.aPlaneSizes[1] += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the chroma plane size
        // stDstImgInfo.nFrameSize += (1920 - 1856) * stDstImgInfo.aHgtStrides[1]; // update the total frame size
    #endif

        /* (optional) get the module tuning configs and change the parameter manually if necessary */
        if (ctx)
        {
            rkpq_dfc_cfg *pDmConfig = (rkpq_dfc_cfg *)rkpq_get_default_cfg(ctx, 0, RKPQ_MODULE_AI_DFC);   // order 0 is the DFC module in the pipeline
            pDmConfig->stPipeFmtInfo.nSrcClrSpc = stSrcImgInfo.nColorSpace;
            pDmConfig->stPipeFmtInfo.nSrcPixFmt = stSrcImgInfo.nPixFmt;
            pDmConfig->stPipeFmtInfo.nDstClrSpc = stDstImgInfo.nColorSpace;
            pDmConfig->stPipeFmtInfo.nDstPixFmt = stDstImgInfo.nPixFmt;
            pDmConfig->bEnableDFC = 1;  // enable AIDM feature, the DM module will run with NPU (need RKNN SDK v1.4)
        }
    }

    return ret;
}
