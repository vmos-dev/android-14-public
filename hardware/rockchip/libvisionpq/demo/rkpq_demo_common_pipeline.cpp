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


/**
 * 以下代码展示了一个通用pipeline的集成示例, pipeline组成为: CSC => DCI => ACM => ZME => SHP => CSC
 * 该pipeline的适用场景：
 *  1. 简单的图像格式/色彩空间转换，保留第一个CSC模块即可实现，其他模块可在pipeline中删去或设置关闭;
 *  2. 图像增强，通过DCI模块调整对比度，通过ACM模块调整色彩，通过SHP模块对图像进行锐化处理;
 *  3. 图像缩放，通过ZME模块实现任意倍率缩放;
 *  4. 以上功能全部开启，支持大部分图像转为YUV，再进行图像增强和缩放，最后再转至目标格式和色彩空间。
 * 本例的数据流: 1080P RGB => 4K NV16, 内部各模块的输入输出数据如下：
 *  input  : [1920x1080 rgb888 (limited-range)] =CSC=> (需要设置格式变换信息`rkpq_pipe_fmt_info`)
 *  CSC out: [1920x1080 yuv444sp (full-range)]  =DCI=>
 *  DCI out: [1920x1080 yuv444sp (full-range)]  =ACM=>
 *  ACM out: [1920x1080 yuv444sp (full-range)]  =ZME=> (需要设置格式变换信息`rkpq_pipe_fmt_info`和分辨率变换信息`rkpq_pipe_res_info`)
 *  ZME out: [3840x2160 yuv422sp (full-range)]  =SHP=>
 *  SHP out: [3840x2160 yuv422sp (full-range)]  =CSC=> (需要设置格式变换信息`rkpq_pipe_fmt_info`)
 *  output : [3840x2160 yuv422sp (limited-range)]
 */
int main(int argc, const char *argv[])
{
    int ret = 0;
    int logLevel = 4;
    printf("logLevel: %d\n", logLevel);

    /* 在初始化`rkpq_context`之前可以设置日志等级、缓存路径等非必须操作 */
    ret |= rkpq_set_loglevel(nullptr, logLevel);             /* 日志等级说明请看api文件 */
    ret |= rkpq_set_cache_path(nullptr, "/data/rkalgo/", 1); /* 参数说明请看api文件 */
    ret |= rkpq_set_target_platform("rk3588");               /* reserved */
    if (ret)
    {
        printf("Failed to set loglevel or cache_path or target_platform! %d\n", ret);
        // return ret;
    }

    /* 创建相关结构体对象，有条件可以做一层封装 */
    rkpq_context ctx = NULL;
    rkpq_init_params initParams = {0};
    rkpq_proc_params procParams = {0};

    /* 步骤1: 填写初始化参数'rkpq_init_params'然后通过接口`rkpq_init()`初始化`rkpq_context`上下文实例 */
    initParams.nInitFlag = RKPQ_FLAG_DEFAULT;
    int order = 0;
    initParams.aModPipeOrder[order++] = RKPQ_MODULE_CSC;    /* order 0 */
    initParams.aModPipeOrder[order++] = RKPQ_MODULE_DCI;    /* order 1 */
    initParams.aModPipeOrder[order++] = RKPQ_MODULE_ACM;    /* order 2 */
    initParams.aModPipeOrder[order++] = RKPQ_MODULE_ZME;    /* order 3 */
    initParams.aModPipeOrder[order++] = RKPQ_MODULE_SHP;    /* order 4 */
    initParams.aModPipeOrder[order++] = RKPQ_MODULE_CSC;    /* order 5 */
    initParams.nModNumInPipe = order;                       /* module num in the pipeline */
    ret = rkpq_init(&ctx, &initParams);                     // 注意这里参数是两个指针!
    if (ret)
    {
        printf("Failed to call rkpq_init() %d, Please check the API parameters!\n", ret);
        return ret;
    }

    /**
     * 步骤2: 准备输入输出的图像和缓冲区信息，填写`rkpq_imgbuf_info`结构体，
     * 一般模块仅支持"单输入-单输出"模式，图像和缓存信息位于`rkpq_proc_params::stSrcImgInfo`和`rkpq_proc_params::stDstImgInfo`结构体中，
     */
    rkpq_imgbuf_info &stSrcImgInfo = procParams.stSrcImgInfo; // 注意这里是引用
    rkpq_imgbuf_info &stDstImgInfo = procParams.stDstImgInfo; // 请注意这里变量的生命周期

    /* 步骤2.1: 设置输入输出图像信息 */
    stSrcImgInfo.nColorSpace = RKPQ_CLR_SPC_RGB_LIMITED;    // see rkpq_clr_spc, limited-range here
    stSrcImgInfo.nPixFmt = RKPQ_IMG_FMT_RGB;                // see rkpq_img_fmt, RGB888 here
    stSrcImgInfo.nPixWid = 1920;                            // set the input image width
    stSrcImgInfo.nPixHgt = 1080;                            // set the input image height
    stSrcImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
    stSrcImgInfo.nAlignment = 64;                           // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
    stSrcImgInfo.aWidStrides[0] = 5760;                     // in this case, no padding in the width dimension
    stSrcImgInfo.aHgtStrides[0] = 1088;                     // in this case, there are 8 lines padding after the first plane
    stDstImgInfo.nColorSpace = RKPQ_CLR_SPC_YUV_709_FULL;   // see rkpq_clr_spc, full-range here
    stDstImgInfo.nPixFmt = RKPQ_IMG_FMT_NV16;               // see rkpq_img_fmt, YUV420SP_NV16 here
    stDstImgInfo.nPixWid = 3840;                            // set the input image width
    stDstImgInfo.nPixHgt = 2160;                            // set the input image height
    stDstImgInfo.nEleDepth = 8;                             // set the first component(Y or R) depth [bit]
    stDstImgInfo.nAlignment = 0;                            // set the pitch alignment [byte], better equal to 'imgAlignment' queried before
    stDstImgInfo.aWidStrides[0] = 0;                        // in this case, no padding in the width dimension

    /* 步骤2.2: 调用`rkpq_query()`接口填充剩下的图像信息，如果`rkpq_imgbuf_info`内所有属性你都填好了，这一步可以省略 */
#if 1
    ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stSrcImgInfo);
    ret |= rkpq_query(NULL, RKPQ_QUERY_IMG_BUF_INFO, sizeof(rkpq_imgbuf_info), &stDstImgInfo);
    if (ret)
    {
        printf("Failed to call rkpq_query() %d, Please check the API parameters!\n", ret);
        return ret;
    }
#endif

    /* 步骤2.3. 申请缓冲区并将信息填入`rkpq_imgbuf_info`内 */
#if !USE_HARDWARE_BUFFER
    // 直接使用`malloc`申请内存，这种情况下不具有零拷贝功能，算法会显示地对输入和输出做一次拷贝操作，不推荐
    stSrcImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(stSrcImgInfo.nFrameSize);
    stDstImgInfo.aPlaneAddrs[0] = (unsigned char*)malloc(stDstImgInfo.nFrameSize);
    stSrcImgInfo.nBufferSize = stSrcImgInfo.nFrameSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */
    stDstImgInfo.nBufferSize = stDstImgInfo.nFrameSize; /* real buffer sieze, should be >= nFrameSize! unit: byte */
    // 这里以读写文件的形式模拟输入输出数据的变化
    FILE *fpSrc = fopen("input.yuv", "rb");
    FILE *fpDst = fopen("output.yuv", "wb");
    if (!fpSrc || !fpDst)
    {
        // 有条件可以对context做一层封装，程序退出时自动调用`rkpq_deinit()`接口释放资源
        printf("Failed to open input/output file!\n");
        // rkpq_deinit(ctx);
        // return -1;
    }
#endif

    /* 步骤2.4. 设置其他处理参数，此部分可根据帧级需求进行更新 */
    procParams.bEnablePropControl   = 0;    // 允许通过`adb properties`控制模块效果
    procParams.bEnableSliderControl = 0;    // 允许`PQTool`上位机软件控制模块效果(仅在`bEnablePropControl=0`时生效)
    procParams.nExtenType = 0;              // reserved, set to 0
    procParams.pExtenConfig = NULL;         // reserved, set to NULL
    // 设置ROI范围（坐标信息基于输出图像的分辨率），以下案例设置了只处理图像右半部分的例子（但未开启）
    procParams.stImgRoi.bEnableRoi = 0;     // 如果需要开启ROI功能，则设置该项为1，并设置ROI坐标信息
    procParams.stImgRoi.x = stDstImgInfo.nPixWid / 2;
    procParams.stImgRoi.y = 0;
    procParams.stImgRoi.w = stDstImgInfo.nPixWid / 2;
    procParams.stImgRoi.h = stDstImgInfo.nPixHgt;

    /* 步骤3: 设置模块的调校参数；通过`rkpq_get_default_cfg`接口获取到的配置参数值均是默认值，可以根据具体需求进行修改。*/
    rkpq_csc_cfg *pCscConfig0 = (rkpq_csc_cfg *)rkpq_get_default_cfg(ctx, 0, RKPQ_MODULE_CSC); // order 0 is the 1st module (CSC) in the pipeline
    rkpq_dci_cfg *pDciConfig = (rkpq_dci_cfg *)rkpq_get_default_cfg(ctx, 1, RKPQ_MODULE_DCI);  // order 1 is the 2nd module (DCI) in the pipeline
    rkpq_acm_cfg *pAcmConfig = (rkpq_acm_cfg *)rkpq_get_default_cfg(ctx, 2, RKPQ_MODULE_ACM);  // order 2 is the 3rd module (ACM) in the pipeline
    rkpq_zme_cfg *pZmeConfig = (rkpq_zme_cfg *)rkpq_get_default_cfg(ctx, 3, RKPQ_MODULE_ZME);  // order 3 is the 4th module (ZME) in the pipeline
    rkpq_shp_cfg *pShpConfig = (rkpq_shp_cfg *)rkpq_get_default_cfg(ctx, 4, RKPQ_MODULE_SHP);  // order 4 is the 5th module (SHP) in the pipeline
    rkpq_csc_cfg *pCscConfig1 = (rkpq_csc_cfg *)rkpq_get_default_cfg(ctx, 5, RKPQ_MODULE_CSC); // order 5 is the last module(CSC) in the pipeline
    if (!pCscConfig0 || !pDciConfig || !pAcmConfig || !pZmeConfig || !pShpConfig || !pCscConfig1)
    {
        printf("Failed to get the default config!\n");
        rkpq_deinit(ctx);
        return -1;
    }

    // 设置CSC模块的格式变换信息，
    //!NOTE: CSC0模块输入格式和色彩空间一定和src一致，CSC1模块输出格式和色彩空间一定和dst一致
    //!NOTE: 由于后续的DCI/ACM/SHP模块只支持YUV格式, 故这里CSC0的输出格式必须是YUV，且色彩空间必须是full-range
    pCscConfig0->stPipeFmtInfo.nSrcClrSpc = stSrcImgInfo.nColorSpace;
    pCscConfig0->stPipeFmtInfo.nSrcPixFmt = stSrcImgInfo.nPixFmt;
    pCscConfig0->stPipeFmtInfo.nDstClrSpc = RKPQ_CLR_SPC_YUV_709_FULL; // default 709F
    pCscConfig0->stPipeFmtInfo.nDstPixFmt = RKPQ_IMG_FMT_NV24; // default RGB->NV24
    //!NOTE: 如果目标dst格式是YUV(如NV12), 这里可以直接输出成目标格式(不必非得转成NV24,可以减少计算量), 但色彩空间必须是full-range
    // if (stDstImgInfo.nPixFmt < RKPQ_IMG_FMT_YUV_MAX) {
    //     pCscConfig0->stPipeFmtInfo.nDstClrSpc = stDstImgInfo.nColorSpace | 1; // to full-range
    //     pCscConfig0->stPipeFmtInfo.nDstPixFmt = stDstImgInfo.nPixFmt;  // use as the dst format
    // }
    // pCscConfig0->nColorTemperature = 7000;  // (需要设置该项就取消注释) 设置色温到7000K
    // pDciConfig->bEnableDCI = 0;             // (需要设置该项就取消注释) 关闭DCI模块
    // pAcmConfig->nLumGain = 256;             // (需要设置该项就取消注释) 设置ACM的亮度增益系数为256
    // pShpConfig->bEnableShootCtrl = 1;       // (需要设置该项就取消注释) 开启SHP模块的'ShootControl'功能

    // 设置ZME模块的格式变换信息和分辨率变换信息
    //!NOTE: 由于前面两个模块(DCI/ACM)都不改变输入输出图像格式，故这里ZME的输入格式必定是CSC0的输出格式，输出格式一般情况下不改变
    pZmeConfig->stPipeFmtInfo.nSrcClrSpc = pCscConfig0->stPipeFmtInfo.nDstClrSpc;
    pZmeConfig->stPipeFmtInfo.nSrcPixFmt = pCscConfig0->stPipeFmtInfo.nDstPixFmt;
    pZmeConfig->stPipeFmtInfo.nDstClrSpc = pCscConfig0->stPipeFmtInfo.nDstClrSpc;
    pZmeConfig->stPipeFmtInfo.nDstPixFmt = pCscConfig0->stPipeFmtInfo.nDstPixFmt;
    //!NOTE: ZME的输出在YUV格式的情况下可以直接在YUV4xxsp(NV24/NV16/NV12)之间转换，这里也可以不要此步骤，直接交给最后一级CSC完成转换
    // if (stDstImgInfo.nPixFmt <= RKPQ_IMG_FMT_NV12) {
    //     pZmeConfig->stPipeFmtInfo.nDstPixFmt = stDstImgInfo.nPixFmt;
    // }
    //!NOTE: ZME模块前/后面均没有其他模块会改变图像的分辨率，故这里的输入/输出分辨率一定是src和dst的分辨率
    pZmeConfig->stPipeResInfo.nSrcImgWid = stSrcImgInfo.nPixWid;
    pZmeConfig->stPipeResInfo.nSrcImgHgt = stSrcImgInfo.nPixHgt;
    pZmeConfig->stPipeResInfo.nDstImgWid = stDstImgInfo.nPixWid;
    pZmeConfig->stPipeResInfo.nDstImgHgt = stDstImgInfo.nPixHgt;


    /* 步骤4: 调用`rkpq_proc()`执行算法 */
    int frameIdx = 0;
    int exitFlag = 0;
    while (exitFlag != 1)   /* break condition */
    {
        procParams.nFrameIdx = frameIdx; // 设置帧号，每帧更新，建议填写

        /* 更新输入帧数据 */
    #if USE_HARDWARE_BUFFER
        // 使用带fd的硬件缓冲区，算法内部会对其进行映射使用，该方案可以实现零拷贝，推荐使用（ARM下一般需要至少64byte对齐）
        int fdSrc = 0, fdDst = 0;           // create hardware buffer then import through fd value
        int fdSrcIdx = 0, fdDstIdx = 0;     // create hardware buffer then import through fd value
        /**
         * 填入提前申请的fd-buffer属性，有fd的话，虚地址可以不用填
         * 注意: 一个buffer被释放后再次申请，fd值可能保持不变，但实际buffer对象已经是另一个新的了，这种现象经常出现在如播放器片源切换、HDMI信号切换等场景。
         *      由于算法内部会对buffer创建映射缓存列表，防止重复映射，提高性能，在这种情况下，缓存对应的buffer实际已经失效，这会导致使用了非法的脏数据。
         *      使用`nFdIndex`属性可以对缓存的有效性做一次双重检验，避免出现上诉问题。 如果你的fd-buffer有对应的唯一标识符，可以使用此唯一标识符作为`nFdIndex`；
         *      如果没有唯一标识符，可以使用一个初始数值作为（当前这一组所有）buffer的`nFdIndex`，在（当前这一组所有）buffer被释放后将此数值递增。
         */
        stSrcImgInfo.nFdValue = fdSrc;
        stDstImgInfo.nFdValue = fdDst;
        stSrcImgInfo.nFdIndex = fdSrcIdx;   // unique identifier of the device buffer
        stDstImgInfo.nFdIndex = fdDstIdx;
    #else
        // 从文件读取输入数据
        if (fpSrc)
        {
            size_t nReadSize = fread(stSrcImgInfo.aPlaneAddrs[0], sizeof(unsigned char), stSrcImgInfo.nFrameSize, fpSrc);
            if (nReadSize != stSrcImgInfo.nFrameSize)
            {
                printf("Failed to read the input buffer on frame #%d!\n", frameIdx);
                break;
            }
        }
    #endif

        ret = rkpq_proc(ctx, &procParams); // 算法阻塞执行
        if (ret)
        {
            printf("Fail to rkpq_proc() %d\n", ret);
            break;
        }

        /* 输出帧数据使用 */
    #if !USE_HARDWARE_BUFFER
        if (fpDst) {
            fwrite(stDstImgInfo.aPlaneAddrs[0], sizeof(unsigned char), stDstImgInfo.nFrameSize, fpDst);
        }
    #endif

        frameIdx++;              // 更新帧号，切换信号和片源后，建议重置
        exitFlag = frameIdx > 1; // 这里跑2帧就退出，实际业务中可以根据实际情况设置退出条件
    }

    /* 步骤5: 回收资源 */
    ret = rkpq_deinit(ctx);
    if (ret) {
        printf("Failed to call rkpq_deinit() %d\n", ret);
    }

#if USE_HARDWARE_BUFFER
    /* free hardware buffer created before */
    /* ... */
#else
    // free the buffers that you allocated
    if (stSrcImgInfo.aPlaneAddrs[0] != NULL) {
        free(stSrcImgInfo.aPlaneAddrs[0]);
    }
    if (stDstImgInfo.aPlaneAddrs[0] != NULL) {
        free(stDstImgInfo.aPlaneAddrs[0]);
    }
    if (fpSrc) {
        fclose(fpSrc);
    }
    if (fpDst) {
        fclose(fpDst);
    }
#endif
    return ret;
}