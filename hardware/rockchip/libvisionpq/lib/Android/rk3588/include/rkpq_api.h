//
/////////////////////////////////////////////////////////////////////////
// Copyright(c) 2022 by Rockchip Corp. All right reserved.
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
// Last update on 2024-04-09

#pragma once
#ifndef __RKPQ_API_H_
#define __RKPQ_API_H_

#include <stdint.h>
#ifdef __linux__
    #include <cstddef>
#endif

#if defined(_MSC_VER)
#define RKPQ_ATTR_ALIGN(n)  __declspec(align(n))
#elif defined(__GNUC__) || defined(__CLANG__)
#define RKPQ_ATTR_ALIGN(n)  __attribute__((aligned(n)))
#else
#error Please set the equivalent of __attribute__((aligned(n))) for your compiler
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* flags definition for rkpq_init() */
#define RKPQ_FLAG_DEFAULT               (0)
#define RKPQ_FLAG_PERF_DETAIL           (1 << 0)    /* reserved */ /* enable logging out performance info */
#define RKPQ_FLAG_HIGH_PERFORM          (1 << 1)    /* reserved */ /* fuse some PQ modules to achive high performence */
#define RKPQ_FLAG_HIGH_PRECISION        (1 << 2)    /* reserved */ /* enlarge bit width to achive high precision */
#define RKPQ_FLAG_ASYNC_MODE            (1 << 3)    /* flag for async processing */
#define RKPQ_FLAG_FIX_RES_IMG_IO        (1 << 4)    /* reserved */ /* fixed resolution image input/output, better performance */
#define RKPQ_FLAG_CIRCULAR_BUF_IN       (1 << 5)    /* reserved */ /* circular input buffer with same size & format, better performance */
#define RKPQ_FLAG_CIRCULAR_BUF_OUT      (1 << 6)    /* reserved */ /* circular output buffer with same size & format, better performance */
#define RKPQ_FLAG_CALC_MEAN_LUMA        (1 << 3)    /* deprecated */
#define RKPQ_FLAG_CVT_RANGE_ONLY        (1 << 4)    /* deprecated */

/* const numbers definition */
#define RKPQ_MIN_IMAGE_WIDTH            128
#define RKPQ_MIN_IMAGE_HEIGHT           128
#define RKPQ_MAX_IMAGE_WIDTH            16384
#define RKPQ_MAX_IMAGE_HEIGHT           16384

#define RKPQ_MAX_PLANE_NUM              3   // max number of planes supported.
#define RKPQ_MAX_PERFORM_NUM            32  // max number of proc performence count
#define RKPQ_MAX_IMG_FMT_NUM            32  // max number of supported image formats
#define RKPQ_MAX_CLR_SPC_NUM            32  // max number of supported color spaces
#define RKPQ_MAX_PQ_MODULE_NUM          64  // max number of supported PQ module types
#define RKPQ_MAX_PIPE_MODULE_NUM        64  // max number of supported PQ modules in the pipeline
#define RKPQ_MAX_MODULE_REPEAT_TIME     4   // max number of repeat time of PQ modules in the pipeline

#define RKPQ_DCI_LUT_SIZE               33  // valid DCI_Y LUT range [0:32]
#define RKPQ_ACM_LUT_LENGTH_Y           9   // valid ACM_Y LUT range [0:8]
#define RKPQ_ACM_LUT_LENGTH_H           65  // valid ACM_H LUT range [0:64]
#define RKPQ_ACM_LUT_LENGTH_S           13  // valid ACM_S LUT range [0:12]
#define RKPQ_SHP_PEAKING_BAND_NUM       4   // number of valid bands for Sharp module
#define RKPQ_ZME_COEF_LENGTH            8   // valid ZME coefs range [0:7]


/* forward declaration */
typedef void * rkpq_context;
struct _rkpq_cvt_cfg;
struct _rkpq_csc_cfg;
struct _rkpq_dci_cfg;
struct _rkpq_acm_cfg;
struct _rkpq_sr_cfg;
struct _rkpq_zme_cfg;
struct _rkpq_shp_cfg;
struct _rkpq_sd_cfg;
struct _rkpq_dbmsr_cfg;
struct _rkpq_dm_cfg;
struct _rkpq_fe_cfg;
struct _rkpq_shp_acm_cfg;

/* the RKPQ modules, use rkpq_query() to check if supported */
typedef enum _rkpq_module
{
    RKPQ_MODULE_NOTUSE  = 0,        /* please DO NOT use this item! */

    /* Common Modules */
    RKPQ_MODULE_CVT     = 1,        /* reserved */ /* format Convert, Y2Y or R2R */
    RKPQ_MODULE_CSC     = 2,        /* Color Space Convert, supported case: Y2Y, Y2R, R2Y, R2R */
    RKPQ_MODULE_DCI     = 3,        /* Dynamic Contrast Improvement */
    RKPQ_MODULE_ACM     = 4,        /* Auto Color Management */
    RKPQ_MODULE_ZME     = 5,        /* Zoom Manage Engine */
    RKPQ_MODULE_SHP     = 6,        /* Sharpen */
    RKPQ_MODULE_MLC     = 7,        /* Mean Luminance Calculation */
    RKPQ_MODULE_MAX,                /* please DO NOT use this item! */

    /* AI Modules, need RKNN support */
    RKPQ_MODULE_AI_MIN  = 100,      /* please DO NOT use this item! */
    RKPQ_MODULE_AI_SR   = 100,      /* AI Super Resolution */
    RKPQ_MODULE_AI_SD   = 101,      /* AI Scene Detection */
    RKPQ_MODULE_AI_DM   = 102,      /* AI DeMosquito */
    RKPQ_MODULE_AI_DFC  = 103,      /* AI DeFalseColor */
    RKPQ_MODULE_AI_DE   = 104,      /* AI Doc Enhance */
    RKPQ_MODULE_AI_DD   = 105,      /* AI Doc Detection */
    RKPQ_MODULE_AI_MAX,             /* please DO NOT use this item! */

    /* Mixture Modules, better performance for a specific scene */
    RKPQ_MIXTURE_MIN     = 1000,    /* please DO NOT use this item! */
    RKPQ_MIXTURE_SHP_ACM = 1000,    /* Mixture modules: SHP + ACM */
    RKPQ_MIXTURE_FE      = 1001,    /* Mixture modules: AI Face Enhance */
    RKPQ_MIXTURE_FE2     = 1002,    /* Mixture modules: AI Face Enhance v2*/
    RKPQ_MIXTURE_FE3     = 1003,    /* Mixture modules: AI Face Enhance v3*/
    RKPQ_MIXTURE_FE4     = 1004,    /* Mixture modules: AI Face Enhance v4*/
    RKPQ_MIXTURE_MSSR    = 1005,    /* Mixture modules: MEMC + SD + SR + RD */
    RKPQ_MIXTURE_MAX                /* please DO NOT use this item! */
} rkpq_module;

/* the query commands, see rkpq_query() for more detail infos */
typedef enum _rkpq_query_cmd
{
    RKPQ_QUERY_SDK_VERSION = 0,         /* get the SDK version info */
    RKPQ_QUERY_PERF_INFO,               /* get the performence info after rkpq_proc() */ /* reserved */
    RKPQ_QUERY_MODULES_SUPPORT,         /* get the supported PQ modules */
    RKPQ_QUERY_MODULES_ROI_SUPPORT,     /* get the supported PQ modules with ROI processing */
    RKPQ_QUERY_IMG_RES_CHANGE_SUPPORT,  /* get the flag if allow to change image resolution when running */
    RKPQ_QUERY_IMG_FMT_CHANGE_SUPPORT,  /* get the flag if allow to change image format when running */
    RKPQ_QUERY_IMG_FMT_INPUT_SUPPORT,   /* get the supported image formats for input */
    RKPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT,  /* get the supported image formats for output */
    RKPQ_QUERY_IMG_COLOR_SPACE_SUPPORT, /* get the supported image color space */
    RKPQ_QUERY_IMG_BUF_INFO,            /* get the image buffer infos with known image format & size */
    RKPQ_QUERY_IMG_ALIGNMENT_OCL,       /* get the OpenCL image alignment size in width, unit: pixel */
    RKPQ_QUERY_BUF_ALIGNMENT_OCL,       /* get the OpenCL sub-buffer alignment size, unit: byte */
    RKPQ_QUERY_RKNN_SUPPORT,            /* get the RKNN supported flag for AIPQ */
    // RKPQ_QUERY_LAST_ERRORS,             /* get the last error messages */
    RKPQ_QUERY_MAX,                     /* the max query command value, please DO NOT use this item! */
} rkpq_query_cmd;

/**
 * the image formats supported
 * @Detail:
 *  - bpp means 'bits per pixel',
 *  - bpc means 'bits per component'.
 */
typedef enum _rkpq_image_format
{
    // YUV
    RKPQ_IMG_FMT_YUV_MIN = 0,   /* the min YUV format value, please DO NOT use this item! */
    RKPQ_IMG_FMT_NV24    = 0,   /* DRM_FORMAT_NV24,   YUV444SP, 24bpp(8|8*2)   */
    RKPQ_IMG_FMT_NV16    = 1,   /* DRM_FORMAT_NV16,   YUV422SP, 16bpp(8|8*2/2) */
    RKPQ_IMG_FMT_NV12    = 2,   /* DRM_FORMAT_NV12,   YUV420SP, 12bpp(8|8*2/4) */
    RKPQ_IMG_FMT_NV30    = 3,   /* DRM_FORMAT_NV30,   YUV444SP, 30bpp(10|10*2),   10bit packed */ /* reserved */
    RKPQ_IMG_FMT_NV20    = 4,   /* DRM_FORMAT_NV20,   YUV422SP, 20bpp(10|10*2/2), 10bit packed */ /* reserved */
    RKPQ_IMG_FMT_NV15    = 5,   /* DRM_FORMAT_NV15,   YUV420SP, 15bpp(10|10*2/4), 10bit packed */ /* reserved */
    RKPQ_IMG_FMT_YU24    = 6,   /* DRM_FORMAT_YUV444, YUV444P,  24bpp(8|8|8)     */ /* reserved */
    RKPQ_IMG_FMT_YU16    = 7,   /* DRM_FORMAT_YUV422, YUV422P,  16bpp(8|8/2|8/2) */ /* reserved */
    RKPQ_IMG_FMT_YU12    = 8,   /* DRM_FORMAT_YUV420, YUV420P,  12bpp(8|8/4|8/4) */ /* reserved */
    RKPQ_IMG_FMT_YUYV    = 9,   /* DRM_FORMAT_YUYV,   YUV422I,  16bpp(8*4/2) */     /* reserved */
    RKPQ_IMG_FMT_VU24    = 10,  /* DRM_FORMAT_VUY888, YUV444I,  24bpp([23:0]=[V8:U8:Y8]) */
    RKPQ_IMG_FMT_VU30    = 11,  /* DRM_FORMAT_VUY101010,YUV444I,32bpp([31:0]=[X2:V10:U10:Y10]), MSB order */ /* reserved */
    RKPQ_IMG_FMT_Y8      = 12,  /* DRM_FORMAT_R8,   8bit YUV400, 8bpp */
    RKPQ_IMG_FMT_UV88    = 13,  /* DRM_FORMAT_GR88, 8bit UV, 16bpp([15:0]=[V8:U8]) */
    RKPQ_IMG_FMT_VU88    = 14,  /* DRM_FORMAT_RG88, 8bit VU, 16bpp([15:0]=[U8:V8]) */
    RKPQ_IMG_FMT_YUV_MAX,       /* the max YUV format value, please DO NOT use this item! */

    // RGB
    RKPQ_IMG_FMT_RGB_MIN = 1000,    /* the min RGB format value, please DO NOT use this item! */
    RKPQ_IMG_FMT_RGBA    = 1000,    /* DRM_FORMAT_ABGR8888, RGBA8888, 32bpp([31:0]=[A8:B8:G8:R8]) */
    RKPQ_IMG_FMT_BGRA    = 1001,    /* DRM_FORMAT_ARGB8888, BGRA8888, 32bpp([31:0]=[A8:R8:G8:B8]) */
    RKPQ_IMG_FMT_RGB     = 1002,    /* DRM_FORMAT_BGR888,   RGB888,   24bpp([23:0]=[B8:G8:R8]) */
    RKPQ_IMG_FMT_BGR     = 1003,    /* DRM_FORMAT_RGB888,   BGR888,   24bpp([23:0]=[R8:G8:B8]) */
    RKPQ_IMG_FMT_RGB565  = 1004,    /* DRM_FORMAT_RGB565,   RGB565,   16bpp([15:0]=[R5:G5:B5]), MSB order */
    RKPQ_IMG_FMT_BGR565  = 1005,    /* DRM_FORMAT_BGR565,   BGR565,   16bpp([15:0]=[B5:G5:R5]), MSB order */
    RKPQ_IMG_FMT_RGB_MAX,           /* the max RGB format value, please DO NOT use this item! */
    // RGB Aliases
    RKPQ_IMG_FMT_RG24    = RKPQ_IMG_FMT_RGB,
    RKPQ_IMG_FMT_BG24    = RKPQ_IMG_FMT_BGR,
} rkpq_img_fmt;

/* the color space supported */
typedef enum _rkpq_color_space
{
    RKPQ_CLR_SPC_YUV_601_LIMITED,       /* ITU-R BT.601 (Limited-range) for SDTV (720P) */
    RKPQ_CLR_SPC_YUV_601_FULL,          /* ITU-R BT.601 Full-range      for SDTV (720P) */
    RKPQ_CLR_SPC_YUV_709_LIMITED,       /* ITU-R BT.709 (Limited-range) for HDTV (1080P) */
    RKPQ_CLR_SPC_YUV_709_FULL,          /* ITU-R BT.709 Full-range      for HDTV (1080P) */
    RKPQ_CLR_SPC_YUV_2020_LIMITED,      /* reserved. ITU-R BT.2020 (Limited-range) for UHDTV (4K/8K) */
    RKPQ_CLR_SPC_YUV_2020_FULL,         /* reserved. ITU-R BT.2020 Full-range      for UHDTV (4K/8K) */
    RKPQ_CLR_SPC_RGB_LIMITED,           /* RGB Limited-range */
    RKPQ_CLR_SPC_RGB_FULL,              /* RGB Full-range */
    RKPQ_CLR_SPC_MAX,                   /* the max color space value, please DO NOT use this item! */
} rkpq_clr_spc;

/* the information for RKPQ_QUERY_SDK_VERSION */
typedef struct _rkpq_version_info
{
    uint32_t    nVerMajor;      /* the major number */
    uint32_t    nVerMinor;      /* the minor number */
    uint32_t    nVerRvson;      /* the revision number */
    char        sVerInfo[64];   /* the full version info string */
} rkpq_version_info;

/* the information for RKPQ_QUERY_MODULES_SUPPORT & RKPQ_QUERY_MODULES_ROI_SUPPORT */
typedef struct _rkpq_module_info
{
    rkpq_module     aValidMods[RKPQ_MAX_PQ_MODULE_NUM]; /* see rkpq_module */
    uint32_t        nValidModNum;                       /* number of valid PQ modules, <= RKPQ_MAX_PQ_MODULE_NUM */
} rkpq_module_info;

/* the information for RKPQ_QUERY_IMG_FMT_INPUT_SUPPORT & RKPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT */
typedef struct _rkpq_imgfmt_info
{
    rkpq_img_fmt    aValidFmts[RKPQ_MAX_IMG_FMT_NUM];   /* see rkpq_img_fmt */
    uint32_t        nValidFmtNum;                       /* number of valid formats, <= RKPQ_MAX_IMG_FMT_NUM */
} rkpq_imgfmt_info;

/* the information for RKPQ_QUERY_IMG_COLOR_SPACE_SUPPORT */
typedef struct _rkpq_clrspc_info
{
    rkpq_clr_spc    aValidSpcs[RKPQ_MAX_CLR_SPC_NUM];   /* see rkpq_clr_spc */
    uint32_t        nValidSpcNum;                       /* number of valid color spaces, <= RKPQ_MAX_CLR_SPCE_NUM */
} rkpq_clrspc_info;

/* the information for RKPQ_QUERY_IMG_BUF_INFO */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_imgbuf_info
{
    /* image info */
    uint32_t    nColorSpace;                        /* [i] see rkpq_clr_spc */
    uint32_t    nPixFmt;                            /* [i] see rkpq_img_fmt */
    uint32_t    nDrmFmt;                            /* [i/o] pixel formats described using the fourcc codes */ /* reserved */
    uint32_t    nPixWid;                            /* [i] pixel width, align with 4 */
    uint32_t    nPixHgt;                            /* [i] pixel height, align with 2 */
    uint32_t    nEleDepth;                          /* [i] element depth (bpc), unit: bit */
    uint32_t    nAlignment;                         /* [i] buffer alignment length / row pitch, unit: byte */
    uint32_t    aWidStrides[RKPQ_MAX_PLANE_NUM];    /* [i/o] image padding width of each plane, unit: byte (at least to be aligned to 4).
                                                       aWidStrides[0] always be >= nPixWid. Set aWidStrides[0] if padding exist,
                                                       and aWidStrides[1:2] will be auto updated according to `nPixFmt`.
                                                       If aWidStrides[0] = 0, all the values will be updated with `nAlignment`. */
    uint32_t    aHgtStrides[RKPQ_MAX_PLANE_NUM];    /* [i/o] image padding height of each plane, unit: line.
                                                       aHgtStrides[0] always be >= nPixHgt. Set aHgtStrides[0] if padding exist,
                                                       and aHgtStrides[1:2] will be auto updated according to `nPixFmt`.
                                                       If aHgtStrides[0] = 0, it will be updated equal to `nPixHgt`. */
    size_t      aPlaneSizes[RKPQ_MAX_PLANE_NUM];    /* [o] buffer size of each plane, unit: byte */
    uint32_t    aPlaneElems[RKPQ_MAX_PLANE_NUM];    /* [o] element number of each plane */
    uint32_t    nPlaneNum;                          /* [o] number of valid planes total */
    size_t      nFrameSize;                         /* [o] full frame size, unit: byte */

    /* buffer info */
    uint8_t    *aPlaneAddrs[RKPQ_MAX_PLANE_NUM];    /* [i] virtual address of each plane, could be NULL if nFdValue set */
    int32_t     nFdValue;                           /* [i] file descriptor, used to import device buffer */
    uint32_t    nFdIndex;                           /* [i] unique identifier of the device buffer, like frame buffer id */
    size_t      nBufferSize;                        /* [i] it should be >= nFrameSize! unit: byte */
} rkpq_imgbuf_info;

/**
 * ROI parameters. Modules supported ROI:
 *  DCI, ACM, SHP, DM
 * NOTE: the coordinate & size should base on the resolution of the output frame
 */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_roi
{
    uint32_t    bEnableRoi;     /* is enbale the ROI feature */
    uint32_t    bComplement;    /* is a complement to the ROI */

    uint32_t    x;     /* output ROI left-top coordinate of x, unit: pixel */
    uint32_t    y;     /* output ROI left-top coordinate of y, unit: pixel */
    uint32_t    w;     /* output ROI size of width , unit: pixel */
    uint32_t    h;     /* output ROI size of height, unit: pixel */
} rkpq_roi;


/* initialization parameters */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_init_params
{
    /* Initialization flag, control the initialization behavior of a rkpq_context */
    uint32_t    nInitFlag;          /* see RKPQ_FLAG_XXX */

    /* Extension flag, set to 0 if no extension loaded */
    uint32_t    nExtenFlag;         /* reserved */

    /**
     * set the rkpq_module pipeline, such as:
     * { CVT(or CSC) [-> ZME] -> CVT(or CSC) ... }
     * { CSC -> DCI -> ACM [-> ZME(or SR)] -> SHP ... }
     * { CSC -> DBMSR -> SHP ... }
    */
    uint32_t    nModNumInPipe;                           /* [i] number of modules in the pipeline */
    uint32_t    aModPipeOrder[RKPQ_MAX_PIPE_MODULE_NUM]; /* [i] order of the modules (see rkpq_module) in the pipeline */
    uint32_t    aReservedData[1];                        /* reserved, for future use */
} rkpq_init_params;

/* execution parameters */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_proc_params
{
    // Image & buffer info
    rkpq_imgbuf_info    stSrcImgInfo;                   /* [i] src image buffer info */
    rkpq_imgbuf_info    stDstImgInfo;                   /* [i] dst image buffer info */
    rkpq_roi            stImgRoi;                       /* [i] src image ROI, use 'RKPQ_QUERY_MODULES_ROI_SUPPORT' to query the ROI-supported modules */

    // Extension config
    uint32_t            nExtenType;                     /* [i] set equal to rkpq_init_params::nExtenFlag for now */
    void               *pExtenConfig;                   /* [i] set to NULL if no extension configuration specified */

    // Parameter info
    uint32_t            nFrameIdx;                      /* [i] frame index in the video sequence */
    uint32_t            bEnablePropControl;             /* [i] enable real-time control with env(like adb properties). PQTool will not work if set to 1 */
    uint32_t            bEnableSliderControl;           /* [i] enable real-time control with PQTool */

    uint32_t            aReservedData[1];               /* reserved, for future use */
} rkpq_proc_params;

/**
 * the info structure for repeatable modules in the pipeline
 */
/* format change info for repeatable modules in the pipeline */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_pipe_fmt_info
{
    uint32_t    nSrcClrSpc;     /* [i] see rkpq_clr_spc */
    uint32_t    nSrcPixFmt;     /* [i] see rkpq_img_fmt */
    uint32_t    nSrcDrmFmt;     /* reserved */

    /* NOTE: `nDstClrSpc` CANNOT be a limited-range colorspace except the LAST module in the pipeline */
    uint32_t    nDstClrSpc;     /* [i] see rkpq_clr_spc */
    uint32_t    nDstPixFmt;     /* [i] see rkpq_img_fmt */
    uint32_t    nDstDrmFmt;     /* reserved */
} rkpq_pipe_fmt_info;

/* resolution change info for repeatable modules in the pipeline */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_pipe_res_info
{
    uint32_t    nSrcImgWid;     /* src image width,  unit: pixel */
    uint32_t    nSrcImgHgt;     /* src image height, unit: pixel */
    uint32_t    nDstImgWid;     /* src image width,  unit: pixel */
    uint32_t    nDstImgHgt;     /* src image height, unit: pixel */
} rkpq_pipe_res_info;

/**
 * the configurations for PQ modules below
 */
/* CVT configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_cvt_cfg
{
    rkpq_pipe_fmt_info  stPipeFmtInfo;  /* format change info, must set manually */

    uint32_t    bEnableCVT;             // [0, (1)]
    uint32_t    nUVSampleMethod;        // YUV chroma data sampling method: {0-bilinear, 1-nearest}

    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_cvt_cfg;

/* CSC configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_csc_cfg
{
    rkpq_pipe_fmt_info  stPipeFmtInfo;  /* format change info, must set manually */

    uint32_t    bEnableCSC;         // [0, (1)]
    uint32_t    nBrightness;        // [0, (256), 511]
    uint32_t    nHue;               // [0, (256), 511]
    uint32_t    nContrast;          // [0, (256), 511]
    uint32_t    nSaturation;        // [0, (256), 511]
    uint32_t    nRGain;             // [0, (256), 511]
    uint32_t    nGGain;             // [0, (256), 511]
    uint32_t    nBGain;             // [0, (256), 511]
    uint32_t    nROffset;           // [0, (256), 511]
    uint32_t    nGOffset;           // [0, (256), 511]
    uint32_t    nBOffset;           // [0, (256), 511]

    uint32_t    nColorTemperature;  // [4000, (6500), 12000], unit: K
    uint32_t    aReservedData[1];   // reserved for future use
} rkpq_csc_cfg;

/* DCI configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_dci_cfg
{
    uint32_t    bEnableDCI;                         // [0, (1)]
    uint16_t    aWgtCoefLow[RKPQ_DCI_LUT_SIZE];     // [0, 1024]
    uint16_t    aWgtCoefMid[RKPQ_DCI_LUT_SIZE];     // [0, 1024]
    uint16_t    aWgtCoefHigh[RKPQ_DCI_LUT_SIZE];    // [0, 1024]
    uint16_t    aWeightLow[RKPQ_DCI_LUT_SIZE - 1];  // [0, 32]
    uint16_t    aWeightMid[RKPQ_DCI_LUT_SIZE - 1];  // [0, 32]
    uint16_t    aWeightHigh[RKPQ_DCI_LUT_SIZE - 1]; // [0, 32]

    /* only for env vars below: */
    uint32_t    nContrastGlobal;    // [0, (256), 512], Deprecated
    uint32_t    nContrastDark;      // [0, (256), 512], Deprecated
    uint32_t    nContrastLight;     // [0, (256), 512], Deprecated

    uint32_t    aReservedData[1];   // reserved for future use
} rkpq_dci_cfg;

/* ACM configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_acm_cfg
{
    uint32_t    bEnableACM;         // [0, (1)]

    int16_t     aTableDeltaYbyH[RKPQ_ACM_LUT_LENGTH_H];                         // [-255, 255]
    int16_t     aTableDeltaSbyH[RKPQ_ACM_LUT_LENGTH_H];                         // [-255, 255]
    int16_t     aTableDeltaHbyH[RKPQ_ACM_LUT_LENGTH_H];                         // [-64, 64]
    int8_t      aTableGainYbyY[RKPQ_ACM_LUT_LENGTH_H][RKPQ_ACM_LUT_LENGTH_Y];   // [-127, 127]
    int8_t      aTableGainSbyY[RKPQ_ACM_LUT_LENGTH_H][RKPQ_ACM_LUT_LENGTH_Y];   // [-127, 127]
    int8_t      aTableGainHbyY[RKPQ_ACM_LUT_LENGTH_H][RKPQ_ACM_LUT_LENGTH_Y];   // [-127, 127]
    int8_t      aTableGainYbyS[RKPQ_ACM_LUT_LENGTH_H][RKPQ_ACM_LUT_LENGTH_S];   // [-127, 127]
    int8_t      aTableGainSbyS[RKPQ_ACM_LUT_LENGTH_H][RKPQ_ACM_LUT_LENGTH_S];   // [-127, 127]
    int8_t      aTableGainHbyS[RKPQ_ACM_LUT_LENGTH_H][RKPQ_ACM_LUT_LENGTH_S];   // [-127, 127]
    uint32_t    nLumGain;           // [0, (256), 1023]
    uint32_t    nHueGain;           // [0, (256), 1023]
    uint32_t    nSatGain;           // [0, (256), 1023]

    /* only for env vars below: */
    uint32_t    nHueRed;            // [0, (256), 512], Deprecated
    uint32_t    nHueGreen;          // [0, (256), 512], Deprecated
    uint32_t    nHueBlue;           // [0, (256), 512], Deprecated
    uint32_t    nHueSkin;           // [0, (256), 512], Deprecated
    uint32_t    nSaturation;        // [0, (256), 512], Deprecated

    uint32_t    aReservedData[1];   // reserved for future use
} rkpq_acm_cfg;

/* ZME configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_zme_cfg
{
    rkpq_pipe_fmt_info  stPipeFmtInfo;  /* format change info, must set manually */
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableZME;                         // [0, (1)]
    uint32_t    bEnableDeringing;                   // [0, (1)]
    uint32_t    bEnableLimitControl;                // [0, (1)]
    uint32_t    bEnableBilinearScaleForChroma;      // [(0), 1]
    int16_t     aVerCoefs[RKPQ_ZME_COEF_LENGTH];    // [0, 512]
    int16_t     aHorCoefs[RKPQ_ZME_COEF_LENGTH];    // [0, 512]

    /* for future use */
    uint32_t    aReservedData[1];                   // reserved for future use
} rkpq_zme_cfg;

/* Sharpen configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_shp_cfg
{
    uint32_t    bEnableSHP;                                 // [0, (1)]
    uint32_t    nPeakingGain;                               // [0, (64), 1024]
    uint32_t    bEnableShootCtrl;                           // [0, (1)]
    uint32_t    nShootCtrlOver;                             // [0, (32), 128]
    uint32_t    nShootCtrlUnder;                            // [0, (32), 128]

    uint32_t    bEnableCoringCtrl;                          // [0, (1)]
    uint16_t    aCoringCtrlRatio[RKPQ_SHP_PEAKING_BAND_NUM];// [512, (2048), 2048]
    uint16_t    aCoringCtrlZero[RKPQ_SHP_PEAKING_BAND_NUM]; // [0, (4), 32]
    uint16_t    aCoringCtrlThrd[RKPQ_SHP_PEAKING_BAND_NUM]; // [0, (40), 64]

    uint32_t    bEnableGainCtrl;                            // [0, (1)]
    uint16_t    aGainCtrlPos[RKPQ_SHP_PEAKING_BAND_NUM];    // [0, (1024), 2048]

    uint32_t    bEnableLimitCtrl;                           // [(0), 1]
    uint16_t    aLimitCtrlPos0[RKPQ_SHP_PEAKING_BAND_NUM];  // [0, (64), 128]
    uint16_t    aLimitCtrlPos1[RKPQ_SHP_PEAKING_BAND_NUM];  // [0, (120), 128]
    uint16_t    aLimitCtrlBndPos[RKPQ_SHP_PEAKING_BAND_NUM];// [0, (65), 128]
    uint16_t    aLimitCtrlRatio[RKPQ_SHP_PEAKING_BAND_NUM]; // [0, (128), 512]

    uint32_t    aReservedData[1];                           // reserved for future use
} rkpq_shp_cfg;

/* MLC configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_mlc_cfg
{
    uint32_t    bEnableMLC;         /* [i] {0, (1)} */
    uint32_t    nBlockNumX;         /* [i] [(1), 16]*/
    uint32_t    nBlockNumY;         /* [i] [(1), 16]*/

    uint32_t    aReservedData[1];   /* reserved for future use */
} rkpq_mlc_cfg;

/* AI SR configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_sr_cfg
{
    rkpq_pipe_fmt_info  stPipeFmtInfo;  /* format change info, must set manually */
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableSR;              // [0, (1)]

    /* dir filter & interp */
    uint32_t    bEnableDirFilter;       // [0, (1)]
    uint32_t    nEdgeThreshold;         // [0, (30), 256]
    uint32_t    nSinglePixelRetain;     // [5, (10), 20]
    uint32_t    nSinglePixelAband;      // [180, (200), 220] /* not work, Reserved */
    uint32_t    nMinNeighborCandNum;    // [1, (3), 5]
    uint32_t    nMinMainDirPercent;     // [0, (128), 256]

    /* RKNN-based AISR */
    uint32_t    bEnableAISR;            // [0, (1)]
    uint32_t    bEnableUsm;             // [0, (1)]
    uint32_t    nUsmGain;               // [0, (64), 256]
    uint32_t    nUsmCtrlOver;           // [0, (32), 256]
    uint32_t    nUsmCtrlUnder;          // [0, (32), 256]
    uint32_t    nColorStrength;         // [0, (128), 256], reserved
    uint32_t    nEdgeStrength;          // [0, (128), 256], reserved

    uint32_t    aReservedData[20];      // reserved for future use
} rkpq_sr_cfg;

/* AI SD configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_sd_cfg
{
    uint32_t    bEnableSD;          // [0, (1)]
    uint32_t    bEnableAISD;        // [0, (1)]

    uint32_t    aReservedData[1];   // reserved for future use
} rkpq_sd_cfg;

#if 0
/* AI DBMSR configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_dbmsr_cfg
{
    uint32_t    bEnableDbmsr;       // [0, (1)]
    uint32_t    bEnableAIDbmsr;     // [0, (1)]
    uint32_t    nFilterStrength;    // [0, (256), 512], useless for now
    uint32_t    nDampingLevel;      // [0, (256), 512], useless for now
    uint32_t    nDenoiseStrength;   // [0, (256), 512], useless for now

    uint32_t    aReservedData[1];   // reserved for future use
} rkpq_dbmsr_cfg;
#endif

/* AI DeMosquito configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_dm_cfg
{
    uint32_t    bEnableDM;              // [0, (1)]
    uint32_t    bEnableAIDM;            // [0, (1)]
    uint32_t    nProcessRatio;          // [0, (128), 255]
    uint32_t    nDenoiseThresholdLow;   // [0, (0), 255]
    uint32_t    nDenoiseThresholdHigh;  // [0, (30), 255]

    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_dm_cfg;

/* AI DeFalseColor configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_dfc_cfg
{
    rkpq_pipe_fmt_info  stPipeFmtInfo;  /* format change info, must set manually */

    uint32_t    bEnableDFC;             // [0, (1)]
    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_dfc_cfg;

/* AI DocEnhance configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_de_cfg
{
    uint32_t    bEnableDE;              // [0, (1)]
    int32_t     aRectPointsX[4];        // LeftTop, RightTop, LeftBottom, RightBottom
    int32_t     aRectPointsY[4];        // LeftTop, RightTop, LeftBottom, RightBottom

    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_de_cfg;

/* AI DocEnhance configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_dd_cfg
{
    uint32_t    bEnableDD;              // [i] range: [0, (1)]
    uint32_t    bDrawLines;             // [i] draw boundary lines
    int32_t     aRectPointsX[4];        // [o] LeftTop, RightTop, LeftBottom, RightBottom
    int32_t     aRectPointsY[4];        // [o] LeftTop, RightTop, LeftBottom, RightBottom

    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_dd_cfg;

/* Mixture SHP + ACM configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_shp_acm_cfg
{
    // for ACM
    uint32_t    bEnableACM;             // [0, (1)]
    uint32_t    nLumGain;               // [0, (256), 1023]
    uint32_t    nHueGain;               // [0, (256), 1023]
    uint32_t    nSatGain;               // [0, (256), 1023]
    // for SHP
    uint32_t    bEnableSHP;             // [0, (1)]
    uint32_t    nPeakingGain;           // [0, (64), 1024]

    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_shp_acm_cfg;

/* Mixture FaceEnhance configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_fe_cfg
{
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableFE;              // [0, (1)]
    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_fe_cfg;

/* Mixture FaceEnhanceV2 configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_fe2_cfg
{
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableFE2;             // [0, (1)]
    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_fe2_cfg;

/* Mixture FaceEnhanceV3 configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_fe3_cfg
{
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableFE3;             // [0, (1)]
    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_fe3_cfg;

/* Mixture FaceEnhanceV4 configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_fe4_cfg
{
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableFE4;             // [0, (1)]
    uint32_t    aReservedData[1];       // reserved for future use
} rkpq_fe4_cfg;

/* Mixture MSSR configuration */
typedef struct RKPQ_ATTR_ALIGN(16) _rkpq_mssr_cfg
{
    rkpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t        bEnableMEMC;        /* [i] {0, (1)} */
    uint32_t        nModelIdx;          /* [i] {0, (1)} */
    uint32_t        nMemcScale;         /* [i] {(4), 5} */
    uint32_t        aDstIdx[16];        /* [i] next output buffer indexs */
    uint32_t        nDstNum;            /* [i] next output number */
    uint32_t        nDstNumPrev;        /* [o] current output number */
    uint8_t        *pHist;              /* [i] pointer of histogram */

    rkpq_sr_cfg    *pSRConfig;
    rkpq_sd_cfg    *pSDConfig;

    uint32_t        aReservedData[1];   /* reserved for future use */
} rkpq_mssr_cfg;



/**
 * @Function: rkpq_init()
 * @Descrptn: rkpq_context initialization, create a rkpq_context instance.
 * @Params:
 *      rkpq_context *pCtxPtr - a pointer of the pq handle of context
 *      rkpq_init_params *pInitParam - a pointer of the API initialization parameters
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_init(rkpq_context *pCtxPtr, rkpq_init_params *pInitParam);

/**
 * @Function: rkpq_proc()
 * @Descrptn: call a rkpq_context instance to execute RKPQ
 * @Params:
 *      rkpq_context ctx - the pq handle of context
 *      rkpq_proc_params *pProcParam - a pointer of the API execution parameters
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_proc(rkpq_context ctx, rkpq_proc_params *pProcParam);

/**
 * @Function: rkpq_deinit()
 * @Descrptn: release rkpq_context resource created by rkpq_init().
 * @Params:
 *      rkpq_context ctx - the pq handle of context
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_deinit(rkpq_context ctx);

/**
 * @Function: rkpq_query()
 * @Descrptn: query the information about image, buffer or others.
 * @Params:
 *      rkpq_context ctx - the pq handle of context
 *      rkpq_query_cmd cmd - the query command, see rkpq_query_cmd
 *      size_t size - the buffer size of retuned information
 *      void *info - the buffer pointer of retuned information value
 * @Return: error code, 0 indicates everything is ok.
 * @Detail: the detail explanation of the arguments lists below:
 *  |       Query Command               |   Need A Context  |   Return Type     |
 *  | --------------------------------- | ----------------- | ----------------- |
 *  | RKPQ_QUERY_SDK_VERSION            |       no          | rkpq_version_info |
 *  | RKPQ_QUERY_PERF_INFO              |       YES         | rkpq_perf_info    |
 *  | RKPQ_QUERY_MODULES_SUPPORT        |       YES         | rkpq_module_info  |
 *  | RKPQ_QUERY_MODULES_ROI_SUPPORT    |       YES         | rkpq_module_info  |
 *  | RKPQ_QUERY_IMG_RES_CHANGE_SUPPORT |       no          | uint32_t          |
 *  | RKPQ_QUERY_IMG_FMT_CHANGE_SUPPORT |       no          | uint32_t          |
 *  | RKPQ_QUERY_IMG_FMT_INPUT_SUPPORT  |       no          | rkpq_imgfmt_info  |
 *  | RKPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT |       no          | rkpq_imgfmt_info  |
 *  | RKPQ_QUERY_IMG_COLOR_SPACE_SUPPORT|       no          | rkpq_clrspc_info  |
 *  | RKPQ_QUERY_IMG_BUF_INFO           |       no          | rkpq_imgbuf_info  |
 *  | RKPQ_QUERY_IMG_ALIGNMENT_OCL      |       YES         | uint32_t          |
 *  | RKPQ_QUERY_BUF_ALIGNMENT_OCL      |       YES         | uint32_t          |
 *  | RKPQ_QUERY_RKNN_SUPPORT           |       YES         | uint32_t          |
 *  | RKPQ_QUERY_LAST_ERRORS            |       YES         | reserved          |
 */
int rkpq_query(rkpq_context ctx, rkpq_query_cmd cmd, size_t size, void *info);

/**
 * @Function: rkpq_set_loglevel()
 * @Descrptn: set the log level for a pq context
 * @Params:
 *      rkpq_context ctx - the pq handle of context, could be 0 for all rkpq_context
 *      int logLevel - the log level, valid range: {0-OFF, 1-FATAL, 2-ERROR, 3-WARNING, 4-INFO}
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_set_loglevel(rkpq_context ctx, int logLevel);

/**
 * @Function: rkpq_set_cache_path()
 * @Descrptn: set the cache data path, for fast initilization, RKNN model etc.
 * @Params:
 *      rkpq_context ctx - set to NULL before calling rkpq_init()
 *      const char *pPath - the folder path, ends with '/'
 *      int target - bitfield, 1 for Ocl caches, 2 for RKNN models, 4 for license path, 0 for all
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_set_cache_path(rkpq_context ctx, const char *pPath, int target);

/**
 * @Function: rkpq_set_default_cfg()
 * @Descrptn: set a specified module configuration to default values.
 * @Params:
 *      void *pModuleConfig - the target configuration of the specified module
 *      rkpq_module module - the specified module
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_set_default_cfg(void *pModuleConfig, rkpq_module module);

/**
 * @Function: rkpq_get_default_cfg()
 * @Descrptn: get the specified module configuration pointer.
 * @Params:
 *      rkpq_context ctx - the pq handle of context
 *      int pipeOrder - the module order in the pipeline
 *      int module - see rkpq_module
 * @Return: void * - the specified module configuration pointer, NULL indicates error happened.
 */
void *rkpq_get_default_cfg(rkpq_context ctx, int pipeOrder, int module);

/**
 * @Function: rkpq_set_inputs() / rkpq_set_outputs()
 * @Descrptn: for multi-input/output modules like AI_MEMC
 * @Params:
 *      rkpq_context ctx - the pq handle of context
 *      rkpq_imgbuf_info *pInputBufs - the input/output buffers' info
 *      int num - number of input/output buffers
 * @Return: error code, 0 indicates everything is ok.
 */
int rkpq_set_inputs(rkpq_context ctx, rkpq_imgbuf_info *pInputBufs, int num);
int rkpq_set_outputs(rkpq_context ctx, rkpq_imgbuf_info *pOutputBufs, int num);

/**
 * @Function: rkpq_set_target_platform()
 * @Descrptn: set the target platform, such as "rk3568", "RK3576", "rk3588[s]", "RK3399[pro]" etc.
 *      Call this function before rkpq_init() to set the target platform.
 * @Params:
 *      const char *pPlatformName - the target platform name, case insensitive
 * @Return: error code, 0 indicates everything is ok.
*/
int rkpq_set_target_platform(const char *pPlatformName);


#ifdef __cplusplus
}
#endif


#endif // __RKPQ_API_H_
