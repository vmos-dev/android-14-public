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
// Last update on 2023-09-01

#pragma once
#ifndef __RKHWPQ_API_H_
#define __RKHWPQ_API_H_

#include <stdint.h>
#ifdef __linux__
    #include <cstddef>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/* flags definition for rk_hwpq_init() */
#define RKHWPQ_FLAG_DEFAULT               0x00000000
#define RKHWPQ_FLAG_PERF_DETAIL           0x00000001  /* enable logging out performance info. reserved */
#define RKHWPQ_FLAG_HIGH_PERFORM          0x00000002  /* fuse some PQ modules to achive high performence */
#define RKHWPQ_FLAG_HIGH_PRECISION        0x00000004  /* enlarge bit width to achive high precision. reserved */
#define RKHWPQ_FLAG_CALC_MEAN_LUMA        0x00000008  /* calculate mean luma value (full-range) when processing */
#define RKHWPQ_FLAG_CVT_RANGE_ONLY        0x00000010  /* convert between full and limited range only, no PQ modules to run */
#define RKHWPQ_FLAG_ASYNC_MODE            0x00000020  /* flag for async processing */ /* reserved */

/* const numbers definition */
#define RKHWPQ_MIN_IMAGE_WIDTH            128
#define RKHWPQ_MIN_IMAGE_HEIGHT           128
#define RKHWPQ_MAX_IMAGE_WIDTH            16384
#define RKHWPQ_MAX_IMAGE_HEIGHT           16384

#define RKHWPQ_MAX_PLANE_NUM              4   // max number of planes supported.
#define RKHWPQ_MAX_PERFORM_NUM            32  // max number of proc performence count
#define RKHWPQ_MAX_IMG_FMT_NUM            32  // max number of supported image formats
#define RKHWPQ_MAX_CLR_SPC_NUM            32  // max number of supported color spaces
#define RKHWPQ_MAX_PQ_MODULE_NUM          32  // max number of supported PQ modules


/* forward declaration */
typedef void * rk_hwpq_context;
struct _rk_hwpq_csc_cfg;
struct _rk_hwpq_dci_cfg;
struct _rk_hwpq_acm_cfg;
struct _rk_hwpq_shp_cfg;
struct _rk_hwpq_vdpp_info;
struct _rk_hwpq_cfg;
struct _rk_hwpq_reg;
struct _rk_vop_status;
struct _rk_hwpq_zme_cfg;

typedef struct _rk_hwpq_pipe_res_info
{
    uint32_t    nSrcImgWid;
    uint32_t    nSrcImgHgt;
    uint32_t    nDstImgWid;
    uint32_t    nDstImgHgt;
} rk_hwpq_pipe_res_info;

typedef struct _rk_hwpq_zme_cfg
{
    rk_hwpq_pipe_res_info  stPipeResInfo;  /* resolution change info, must set manually */

    uint32_t    bEnableZME;                         // [0, (1)]
    uint32_t    bEnableDeringing;                   // [0, (1)]
    uint32_t    bEnableLimitControl;                // [0, (1)]
    uint32_t    bEnableBilinearScaleForChroma;      // [(0), 1]
    int16_t     aVerCoefs[8];                       // [0, 512]
    int16_t     aHorCoefs[8];                       // [0, 512]

    /* for future use */
    uint32_t    aReservedData[8];                   // reserved for future use
} rk_hwpq_zme_cfg;

/* the RKHWPQ run modes */
typedef enum _rk_hwpq_run_mode
{
    RKHWPQ_RUN_MODE_TRAD_PQ,    /* trad pq mode */
    RKHWPQ_RUN_MODE_AISR,       /* aisr mode */
    RKHWPQ_RUN_MODE_MAX,        /* the max PQ-RUN-MODE, please DO NOT use this item! */
} rk_hwpq_run_mode;

/* RKHWPQ Support Platforms */
typedef enum _rk_hwpq_platform
{
    RK_HWPQ_PLAT_RK3576, 
    RK_HWPQ_PLAT_RK3588, 
    RK_HWPQ_PLAT_RK3568, 
    RK_HWPQ_PLAT_RK3528, 
    RK_HWPQ_PLAT_MAX, 
} rk_hwpq_platform;

/* the query commands, see rk_hwpq_query() for more detail infos */
typedef enum _rk_hwpq_query_cmd
{
    RKHWPQ_QUERY_SDK_VERSION = 0,         /* get the SDK version info */
    RKHWPQ_QUERY_PERF_INFO,               /* get the performence info after rk_hwpq_proc() */
    RKHWPQ_QUERY_IMG_FMT_INPUT_SUPPORT,   /* get the supported image formats for input */
    RKHWPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT,  /* get the supported image formats for output */
    RKHWPQ_QUERY_IMG_FMT_CHANGE_SUPPORT,  /* get the flag if enable change image format when running */
    // RKHWPQ_QUERY_IMG_RES_INPUT_SUPPORT,   /* get the supported image resolutions for input */
    // RKHWPQ_QUERY_IMG_RES_OUTPUT_SUPPORT,  /* get the supported image resolutions for output */
    RKHWPQ_QUERY_IMG_RES_CHANGE_SUPPORT,  /* get the flag if enable change image resolution when running */
    RKHWPQ_QUERY_IMG_COLOR_SPACE_SUPPORT, /* get the supported image color space */
    RKHWPQ_QUERY_IMG_BUF_INFO,            /* get the image buffer infos with known image format & size */
    RKHWPQ_QUERY_IMG_ALIGNMENT_OCL,       /* get the OpenCL image alignment size in width, unit: pixel */
    RKHWPQ_QUERY_RKNN_SUPPORT,            /* get the RKNN supported flag for SR */
    RKHWPQ_QUERY_MEAN_LUMA,               /* get the mean luma value (full-range) of the output image after rk_hwpq_proc() */
    RKHWPQ_QUERY_MODULES_SUPPORT,         /* get the supported PQ modules */
    // RKHWPQ_QUERY_3DLUT_AI_TABLE,          /* get the 3D-LUT table from RKNN result */
    RKHWPQ_QUERY_MAX,                     /* the max query command value, please DO NOT use this item! */
} rk_hwpq_query_cmd;

/**
 * the image formats supported
 * @Detail:
 *  - bpp means 'bits per pixel',
 *  - bpc means 'bits per component'.
 */
typedef enum _rk_hwpq_image_format
{
    // YUV
    RKHWPQ_IMG_FMT_YUV_MIN = 0,   /* the min YUV format value, please DO NOT use this item! */
    RKHWPQ_IMG_FMT_NV24 = 0,      /* YUV444SP, 2 plane YCbCr, 24bpp/8 bpc, non-subsampled Cr:Cb plane */
    RKHWPQ_IMG_FMT_NV16,          /* YUV422SP, 2 plane YCbCr, 16bpp/8 bpc, 2x1 subsampled Cr:Cb plane */
    RKHWPQ_IMG_FMT_NV12,          /* YUV420SP, 2 plane YCbCr, 12bpp/8 bpc, 2x2 subsampled Cr:Cb plane */
    RKHWPQ_IMG_FMT_NV15,          /* YUV420SP, 2 plane YCbCr, 15bpp/10bpc, 10bit packed data */
    RKHWPQ_IMG_FMT_NV20,          /* YUV422SP, 2 plane YCbCr, 20bpp/10bpc, 10bit packed data, output supported only */ /* reserved */
    RKHWPQ_IMG_FMT_NV30,          /* YUV444SP, 2 plane YCbCr, 30bpp/10bpc, 10bit packed data, output supported only */
    RKHWPQ_IMG_FMT_P010,          /* YUV420SP, 2 plane YCbCr, 24bpp/16bpc, 10bit unpacked data with MSB aligned, output supported only */
    RKHWPQ_IMG_FMT_P210,          /* YUV422SP, 2 plane YCbCr, 32bpp/16bpc, 10bit unpacked data with MSB aligned, output supported only */ /* reserved */
    RKHWPQ_IMG_FMT_Q410,          /* YUV444P , 3 plane YCbCr, 48bpp/16bpc, 10bit unpacked data with MSB aligned, output supported only */
    RKHWPQ_IMG_FMT_Y_ONLY_8BIT,   /* Only 8bit-Y Plane, For VDPP y-uv diff mode */
    RKHWPQ_IMG_FMT_UV_ONLY_8BIT,  /* Only 8bit-UV Plane, For VDPP y-uv diff mode */
    RKHWPQ_IMG_FMT_YUV_MAX,       /* the max YUV format value, please DO NOT use this item! */

    // RGB
    RKHWPQ_IMG_FMT_RGB_MIN = 1000,/* the min RGB format value, please DO NOT use this item! */
    RKHWPQ_IMG_FMT_RGBA = 1000,   /* RGBA8888, 32bpp */
    RKHWPQ_IMG_FMT_RG24,          /* RGB888, 24bpp */
    RKHWPQ_IMG_FMT_BG24,          /* BGR888, 24bpp */
    RKHWPQ_IMG_FMT_AB30,          /* ABGR2101010, reserved */
    RKHWPQ_IMG_FMT_RGB_MAX,       /* the max RGB format value, please DO NOT use this item! */
} rk_hwpq_img_fmt;

/* the color space supported */
typedef enum _rk_hwpq_color_space
{
    RKHWPQ_CLR_SPC_YUV_601_LIMITED,       /* ITU-R BT.601 (Limited-range) for SDTV (720P) */
    RKHWPQ_CLR_SPC_YUV_601_FULL,          /* ITU-R BT.601 Full-range      for SDTV (720P) */
    RKHWPQ_CLR_SPC_YUV_709_LIMITED,       /* ITU-R BT.709 (Limited-range) for HDTV (1080P) */
    RKHWPQ_CLR_SPC_YUV_709_FULL,          /* ITU-R BT.709 Full-range      for HDTV (1080P) */
    RKHWPQ_CLR_SPC_YUV_2020_LIMITED,      /* reserved. ITU-R BT.2020 (Limited-range) for UHDTV (4K/8K) */
    RKHWPQ_CLR_SPC_YUV_2020_FULL,         /* reserved. ITU-R BT.2020 Full-range      for UHDTV (4K/8K) */
    RKHWPQ_CLR_SPC_RGB_LIMITED,           /* RGB Limited-range */
    RKHWPQ_CLR_SPC_RGB_FULL,              /* RGB Full-range */
    RKHWPQ_CLR_SPC_MAX,                   /* the max color space value, please DO NOT use this item! */
} rk_hwpq_clr_spc;

/* the information for RKHWPQ_QUERY_SDK_VERSION */
typedef struct _rk_hwpq_version_info
{
    uint32_t    nVerMajor;      /* the major number */
    uint32_t    nVerMinor;      /* the minor number */
    uint32_t    nVerRvson;      /* the revision number */
    char        sVerInfo[64];   /* the full version info string */
} rk_hwpq_version_info;


/* the information for RKHWPQ_QUERY_PERF_INFO */
typedef struct _rk_hwpq_perf_info
{
    float   fTimeCostInit;      /* cost time of rk_hwpq_init() interface */
    float   fTimeCostDeinit;    /* invalid */
    float   fTimeCostProcs[RKHWPQ_MAX_PERFORM_NUM];
} rk_hwpq_perf_info;

/* the information for RKHWPQ_QUERY_IMG_FMT_INPUT_SUPPORT & RKHWPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT */
typedef struct _rk_hwpq_imgfmt_info
{
    int32_t     aValidFmts[RKHWPQ_MAX_IMG_FMT_NUM];   /* see rk_hwpq_img_fmt */
    uint32_t    nValidFmtNum;                       /* number of valid formats, <= RKHWPQ_MAX_IMG_FMT_NUM */
} rk_hwpq_imgfmt_info;

/* the information for RKHWPQ_QUERY_IMG_COLOR_SPACE_SUPPORT */
typedef struct _rk_hwpq_clrspc_info
{
    int32_t     aValidSpcs[RKHWPQ_MAX_CLR_SPC_NUM];   /* see rk_hwpq_clr_spc */
    uint32_t    nValidSpcNum;                       /* number of valid color spaces, <= RKHWPQ_MAX_CLR_SPCE_NUM */
} rk_hwpq_clrspc_info;

/**
 * the information for RKHWPQ_QUERY_IMG_BUF_INFO
 * @Detail: bpc means 'bits per component'
 */
typedef struct _rk_hwpq_imgbuf_info
{
    uint32_t    nColorSpace;                        /* [i] see rk_hwpq_clr_spc */
    uint32_t    nPixFmt;                            /* [i] see rk_hwpq_img_fmt */
    uint32_t    nPixWid;                            /* [i] pixel width */
    uint32_t    nPixHgt;                            /* [i] pixel height */
    uint32_t    nEleDepth;                          /* [i] element depth (bpc), unit: bit */
    uint32_t    nAlignment;                         /* [i] buffer alignment length / row pitch, unit: byte */
    uint32_t    aWidStrides[RKHWPQ_MAX_PLANE_NUM];    /* [i/o] image padding width of each plane, unit: byte. aWidStrides[0] always >= nPixWid.
                                                        Set aWidStrides[0] if padding exist, and aWidStrides[1:2] will be auto updated according to `nPixFmt`.
                                                        If aWidStrides[0] = 0, all the values will be updated with `nAlignment`. */
    uint32_t    aHgtStrides[RKHWPQ_MAX_PLANE_NUM];    /* [i/o] image padding height of each plane, unit: line.
                                                        Set aHgtStrides[0] if padding exist, and aHgtStrides[1:2] will be auto updated according to `nPixFmt`.
                                                        If aHgtStrides[0] = 0, it will be updated equal to `nPixHgt`. */
    uint32_t    nPixWidStrd;                        /* [o] pixel width stride with padding, unit: pixel */
    uint32_t    nPlaneNum;                          /* [o] number of valid planes */
    size_t      nFrameSize;                         /* [o] full frame size, unit: byte */
    size_t      aPlaneSizes[RKHWPQ_MAX_PLANE_NUM];    /* [o] size of each plane, unit: byte */
    uint32_t    aPlaneElems[RKHWPQ_MAX_PLANE_NUM];    /* [o] element number of each plane */
} rk_hwpq_imgbuf_info;

typedef struct {
    uint8_t*    p_addr;
    int32_t     fd;
    uint32_t    offset;
    uint32_t    length;
} rk_hwpq_img_addr_info;

/* initialization parameters */
typedef struct _rk_hwpq_init_params
{
    rk_hwpq_imgbuf_info    stSrcFromRgaImgInfo;        /* [i] src image buffer from rga info */
    rk_hwpq_imgbuf_info    stSrcFromVdppImgInfo_y;     /* [i] src image buffer from vdpp info */
    rk_hwpq_imgbuf_info    stSrcFromVdppImgInfo_uv;    /* [i] src image buffer from vdpp info */
    rk_hwpq_imgbuf_info    stDstImgInfo;               /* [i] dst image buffer for vop info */
    uint32_t            aReserved0[16];             /* reserved array, for new flags added in the future */

    // init flag
    uint32_t            nInitFlag;                  /* [i] see RKHWPQ_FLAG_XXXX */

    // hwpq_run_Mode
    rk_hwpq_run_mode       nHwpqRunMode;               /* [i] hwpq run mode, 0-trad_pq, 1-aisr */    

    // plat form info
    uint32_t            nPaltformInfo;
    uint32_t            aReserved1[16];             /* reserved array, for new flags added in the future */        
} rk_hwpq_init_params;
typedef rk_hwpq_init_params RKHWPQ_Init_Params;

/* execution parameters */
typedef struct _rk_hwpq_proc_params
{
    // Data information
    uint32_t            nFrameIdx;                          /* [i] image index in the video sequence */                            /* [i] dst buffer size, it should be >= stDstImgInfo::nFrameSize! unit: byte */
    
    rk_hwpq_img_addr_info  stSrcFromRgaYrgbAddrInfo;
    rk_hwpq_img_addr_info  stSrcFromRgaUvAddrInfo;
    rk_hwpq_img_addr_info  stSrcFromVdppYrgbAddrInfo;
    rk_hwpq_img_addr_info  stSrcFromVdppUvAddrInfo;
    rk_hwpq_img_addr_info  stDstBufYrgbAddrInfo;
    rk_hwpq_img_addr_info  stDstBufUvAddrInfo;

    uint32_t            aReserved0[24];                     /* reserved array, for new flags added in the future */
    
    // Vdpp information
    _rk_hwpq_vdpp_info     *pVdppInfo;                         /* [i] information from vdpp */

    // Vop Display status
    _rk_vop_status      *pVopStatus;                        /* [i] vop display status */

    // Module configurations
    _rk_hwpq_cfg        *pConfigHwpq;                       /* [i] hwpq user config */

    // Output Vop Register configurations
    _rk_hwpq_reg        *pRegResult;                        /* [o] hwpq output reg */

    uint32_t            aReserved1[24];                     /* reserved array, for new flags added in the future */
    // TODO: add new module here
} rk_hwpq_proc_params;
typedef rk_hwpq_proc_params RKHWPQ_Proc_Params;

/**
 * the configurations for PQ modules below
 */
/* CSC configuration */
typedef struct _rk_hwpq_csc_cfg
{
    bool        bEnableCSC;

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

    uint32_t    aReservedData[6];   // reserved for future use
} rk_hwpq_csc_cfg;
typedef rk_hwpq_csc_cfg RKHWPQ_CSC_CFG;

/* DCI configuration */
#define RKVOP_PQ_DCI_CF_CFG_HIST_SIZE           (32)
#define RKVOP_PQ_DCI_CF_CFG_LUT_SIZE	        (RKVOP_PQ_DCI_CF_CFG_HIST_SIZE + 1)
typedef struct cf_params{
	uint16_t	wgtCoef_low[RKVOP_PQ_DCI_CF_CFG_LUT_SIZE];
	uint16_t	wgtCoef_mid[RKVOP_PQ_DCI_CF_CFG_LUT_SIZE];
	uint16_t	wgtCoef_high[RKVOP_PQ_DCI_CF_CFG_LUT_SIZE];
	uint16_t	weight_low[RKVOP_PQ_DCI_CF_CFG_HIST_SIZE];	//5bitFix
	uint16_t	weight_mid[RKVOP_PQ_DCI_CF_CFG_HIST_SIZE];	//5bitFix
	uint16_t	weight_high[RKVOP_PQ_DCI_CF_CFG_HIST_SIZE];	//5bitFix

	uint8_t	gain_low; // 5bit Fix
	uint8_t	gain_mid; // 5bit Fix
	uint8_t	gain_high; // 5bit Fix

	uint8_t	hist_cor_thr0;
	uint8_t	hist_cor_thr1;
	uint8_t	hist_cor_thr2;
}rk_pq_dci_cf_params;

typedef struct he_params{
	uint16_t	splitPoint;
	float	    leftClip;
	float	    rightClip;
	uint8_t	    overLap;
}rk_pq_dci_he_params;

typedef struct bs_params{
	uint16_t    bs_enable;
	uint16_t    bs_set_point;
	uint16_t    bs_ratio; //6bit Fix
	uint16_t    bs_overlap; //6bit Fix
}rk_pq_dci_bs_params;

typedef struct ws_params {
    uint16_t    ws_enable;
    uint16_t    ws_set_point;
    uint16_t    ws_ratio; //6bit Fix
    uint16_t    ws_overlap; //6bit Fix
}rk_pq_dci_ws_params;

typedef struct ca_params{
	uint16_t    ca_enable;
	uint16_t    saturation_w; //6bit Fix
	uint16_t    ca_adj_luma_coring_zero; //6bit Fix
	uint16_t    ca_adj_luma_coring_thrd; //6bit Fix
}rk_pq_dci_ca_params;

typedef struct clahe_params {
	uint16_t	clahe_enable;
	float		clip_value;	//default value: 1/256
    uint16_t    local_ratio; // 5bit Fix
    float       left_alpha; 
    float       left_ThrLmin; 
    float       left_ThrLmax; 
    float       left_lumRatio; 
    float       right_alpha;
    float       right_ThrRmin;
    float       right_ThrRmax;
}rk_pq_dci_clahe_params;

typedef struct dci_abld_cfg_t {
	uint16_t	dci_metricAbldCoef0; //5bit Fix
	uint16_t	dci_metricAbldCoef1; //5bit Fix
	uint16_t	dci_metricAbldCoef2; //5bit Fix
	uint32_t    dci_clahe_scd_thr_max;
    uint32_t    dci_clahe_scd_thr_min;
	float	    dci_clahe_abld_ratio; 
	uint16_t	dci_minLuma_abld_ratio; //5bit Fix
	uint16_t	dci_maxLuma_abld_ratio; //5bit Fix
	uint16_t    dci_scd_thr;
}rk_pq_dci_abld_cfg_t;

typedef struct dci_vdpp_info_t {
	uint8_t*    p_hist_addr;
    uint32_t    hist_length;
	uint16_t    vdpp_img_w_in;
	uint16_t    vdpp_img_h_in;
	uint16_t    vdpp_img_w_out;
	uint16_t    vdpp_img_h_out;
	uint16_t    vdpp_blk_size_h;
	uint16_t    vdpp_blk_size_v;
}dci_vdpp_info_t;

typedef struct _dci_user_cfg_t
{
    uint8_t                 dci_enable;
	uint16_t			    dci_cf_he_ratio;
	rk_pq_dci_cf_params 	dci_cf_cfg;
	rk_pq_dci_he_params		dci_he_cfg;
	rk_pq_dci_bs_params		dci_bs_cfg;
	rk_pq_dci_ws_params		dci_ws_cfg;
	rk_pq_dci_ca_params		dci_ca_cfg;
	rk_pq_dci_clahe_params	dci_clahe_cfg;
	rk_pq_dci_abld_cfg_t	dci_abld_user_cfg;
} dci_user_cfg_t;

typedef struct _rk_hwpq_dci_cfg {
	dci_user_cfg_t 	dci_user_cfg;
	dci_vdpp_info_t dci_vdpp_info;
}rk_hwpq_dci_cfg;
typedef rk_hwpq_dci_cfg RKHWPQ_DCI_CFG;

/* ACM configuration */
#define RKVOP_PQ_ACM_YLUT_LENGTH		9  //Y sampling numbers, 0 : 32 : 255
#define RKVOP_PQ_ACM_SLUT_LENGTH		13 //S sampling numbers, 0 : 15 : 180
#define RKVOP_PQ_ACM_HLUT_LENGTH		65 //H sampling numbers, -180 : 360/64 : 180
#define RKVOP_PQ_ACM_HLUT_DOWN_LENGTH	17 
#define ACM_GAIN_LUT_HY_LENGTH		(RKVOP_PQ_ACM_YLUT_LENGTH*RKVOP_PQ_ACM_HLUT_DOWN_LENGTH)
#define ACM_GAIN_LUT_HY_TOTAL_LENGTH	(ACM_GAIN_LUT_HY_LENGTH * 3)
#define ACM_GAIN_LUT_HS_LENGTH		(RKVOP_PQ_ACM_SLUT_LENGTH*RKVOP_PQ_ACM_HLUT_DOWN_LENGTH)
#define ACM_GAIN_LUT_HS_TOTAL_LENGTH (ACM_GAIN_LUT_HS_LENGTH * 3)
#define ACM_DELTA_LUT_H_LENGTH		RKVOP_PQ_ACM_HLUT_LENGTH
#define ACM_DELTA_LUT_H_TOTAL_LENGTH	(ACM_DELTA_LUT_H_LENGTH * 3)
typedef struct _rk_hwpq_acm_cfg
{
	uint8_t		acmEnable;
	int16_t		acmTableDeltaYbyH[RKVOP_PQ_ACM_HLUT_LENGTH]; //RK_INT_S9
	int16_t		acmTableDeltaHbyH[RKVOP_PQ_ACM_HLUT_LENGTH]; //RK_INT_S7
	int16_t		acmTableDeltaSbyH[RKVOP_PQ_ACM_HLUT_LENGTH]; //RK_INT_S9
	int16_t		acmTableGainYbyY[RKVOP_PQ_ACM_YLUT_LENGTH * RKVOP_PQ_ACM_HLUT_DOWN_LENGTH]; //RK_INT_S9
	int16_t		acmTableGainHbyY[RKVOP_PQ_ACM_YLUT_LENGTH * RKVOP_PQ_ACM_HLUT_DOWN_LENGTH]; //RK_INT_S9
	int16_t		acmTableGainSbyY[RKVOP_PQ_ACM_YLUT_LENGTH * RKVOP_PQ_ACM_HLUT_DOWN_LENGTH]; //RK_INT_S9
	int16_t		acmTableGainYbyS[RKVOP_PQ_ACM_SLUT_LENGTH * RKVOP_PQ_ACM_HLUT_DOWN_LENGTH]; //RK_INT_S9
	int16_t		acmTableGainHbyS[RKVOP_PQ_ACM_SLUT_LENGTH * RKVOP_PQ_ACM_HLUT_DOWN_LENGTH]; //RK_INT_S9
	int16_t		acmTableGainSbyS[RKVOP_PQ_ACM_SLUT_LENGTH * RKVOP_PQ_ACM_HLUT_DOWN_LENGTH]; //RK_INT_S9
	uint16_t	lumGain;
	uint16_t	hueGain;
	uint16_t	satGain;
}rk_hwpq_acm_cfg;
typedef rk_hwpq_acm_cfg RKHWPQ_ACM_CFG;

/* Sharp configuration */
struct rkvop_cfg_lti_struct
{
	uint8_t 	lti_enable;
    uint16_t    lti_radius;
	uint16_t 	lti_slope;
	uint16_t 	lti_thresold;
	uint16_t 	lti_gain;
	uint16_t 	lti_noise_thr_pos;
	uint16_t 	lti_noise_thr_neg;
};

struct rkvop_cfg_peaking_struct
{
	uint8_t 	peaking_enable;
	uint16_t 	peaking_gain;

	uint8_t 	coring_enable;
	uint16_t 	coring_zero[8];
	uint16_t 	coring_thr[8];
	uint16_t 	coring_ratio[8];

	uint8_t 	gain_enable;
	uint16_t 	gain_pos[8];
	uint16_t 	gain_neg[8];

	uint8_t 	limit_ctrl_enable;
	uint16_t 	limit_ctrl_pos0[8];
	uint16_t 	limit_ctrl_pos1[8];
	uint16_t 	limit_ctrl_neg0[8];
	uint16_t 	limit_ctrl_neg1[8];
	uint16_t 	limit_ctrl_ratio[8];
	uint16_t 	limit_ctrl_bnd_pos[8];
	uint16_t 	limit_ctrl_bnd_neg[8];

    uint8_t   shoot_adj_enable;
    uint8_t   shoot_adj_delta_offset[8];
    uint8_t   shoot_adj_alpha_over[8];
    uint8_t   shoot_adj_alpha_under[8];
    uint8_t   shoot_adj_alpha_over_unlimit[8];
    uint8_t   shoot_adj_alpha_under_unlimit[8];

    uint8_t   edge_ctrl_enable;
    uint8_t   edge_ctrl_non_dir_thr;
    uint8_t   edge_ctrl_dir_cmp_ratio;
    uint8_t   edge_ctrl_non_dir_wgt_offset;
    uint8_t   edge_ctrl_non_dir_wgt_ratio;
    uint8_t   edge_ctrl_dir_cnt_thr;
    uint8_t   edge_ctrl_dir_cnt_avg;
    uint8_t   edge_ctrl_dir_cnt_offset;
    uint8_t   edge_ctrl_diag_dir_thr;
    uint8_t   edge_ctrl_diag_adj_gain_tab[8];

    uint8_t   edge_shoot_ctrl_enable;
    uint8_t   edge_shoot_ctrl_delta_offset_h;
    uint8_t   edge_shoot_ctrl_alpha_over_h;
    uint8_t   edge_shoot_ctrl_alpha_under_h;
    uint8_t   edge_shoot_ctrl_alpha_over_unlimit_h;
    uint8_t   edge_shoot_ctrl_alpha_under_unlimit_h;
    uint8_t   edge_shoot_ctrl_delta_offset_v;
    uint8_t   edge_shoot_ctrl_alpha_over_v;
    uint8_t   edge_shoot_ctrl_alpha_under_v;
    uint8_t   edge_shoot_ctrl_alpha_over_unlimit_v;
    uint8_t   edge_shoot_ctrl_alpha_under_unlimit_v;
    uint8_t   edge_shoot_ctrl_delta_offset_d0;
    uint8_t   edge_shoot_ctrl_alpha_over_d0;
    uint8_t   edge_shoot_ctrl_alpha_under_d0;
    uint8_t   edge_shoot_ctrl_alpha_over_unlimit_d0;
    uint8_t   edge_shoot_ctrl_alpha_under_unlimit_d0;
    uint8_t   edge_shoot_ctrl_delta_offset_d1;
    uint8_t   edge_shoot_ctrl_alpha_over_d1;
    uint8_t   edge_shoot_ctrl_alpha_under_d1;
    uint8_t   edge_shoot_ctrl_alpha_over_unlimit_d1;
    uint8_t   edge_shoot_ctrl_alpha_under_unlimit_d1;
    uint8_t   edge_shoot_ctrl_delta_offset_non;
    uint8_t   edge_shoot_ctrl_alpha_over_non;
    uint8_t   edge_shoot_ctrl_alpha_under_non;
    uint8_t   edge_shoot_ctrl_alpha_over_unlimit_non;
    uint8_t   edge_shoot_ctrl_alpha_under_unlimit_non;
    uint8_t   filter_cfg_diag_enh_coef;
    int16_t  filter_cfg_filt_core_H0[6];
    int16_t  filter_cfg_filt_core_H1[6];
    int16_t  filter_cfg_filt_core_H2[6];
    int16_t  filter_cfg_filt_core_H3[6];
    int16_t  filter_cfg_filt_core_V0[3];
    int16_t  filter_cfg_filt_core_V1[3];
    int16_t  filter_cfg_filt_core_V2[3];
    int16_t  filter_cfg_filt_core_USM[3];
};

struct rkvop_cfg_shootctrl_struct
{
	uint8_t shootctrl_enable;
	uint8_t filter_radius;
    uint8_t delta_offset;
    uint16_t alpha_over;
    uint16_t alpha_under;
    uint16_t alpha_over_unlimit;
    uint16_t alpha_under_unlimit;
};

struct rkvop_cfg_global_gain_struct
{
	uint8_t   global_gain_enable;
    uint8_t   lum_mode; 
    uint16_t  lum_grd[6];
    uint8_t   lum_val[6];
    uint16_t  adp_grd[6];
    uint8_t   adp_val[6];
    uint16_t  var_grd[6];
    uint8_t   var_val[6];
};

struct rkvop_cfg_color_ctrl_struct
{
    uint8_t   color_ctrl_enable;
    
    uint8_t   p0_scaling_coef;
    uint16_t   p0_ctrl_point_u;
    uint16_t   p0_ctrl_point_v;
    uint8_t   p0_ctrl_roll_tab[16];

    uint8_t   p1_scaling_coef;
    uint16_t   p1_ctrl_point_u;
    uint16_t   p1_ctrl_point_v;
    uint8_t   p1_ctrl_roll_tab[16];

    uint8_t   p2_scaling_coef;
    uint16_t   p2_ctrl_point_u;
    uint16_t   p2_ctrl_point_v;
    uint8_t   p2_ctrl_roll_tab[16];

    uint8_t   p3_scaling_coef;
    uint16_t   p3_ctrl_point_u;
    uint16_t   p3_ctrl_point_v;
    uint8_t   p3_ctrl_roll_tab[16];
};

struct rkvop_cfg_texture_adj_struct
{
	uint8_t   textureAdj_enable;
    uint8_t   y_mode_select;
    uint8_t   idx_mode_select;
    uint16_t  texture_grd[6];
    uint8_t   texture_val[6];
};

typedef struct _rk_hwpq_shp_cfg {
	uint8_t 						sharp_enable;
	// params config
	rkvop_cfg_lti_struct			sharp_lti_h_cfg;
	rkvop_cfg_lti_struct			sharp_cti_h_cfg;
	rkvop_cfg_lti_struct			sharp_lti_v_cfg;
	rkvop_cfg_lti_struct			sharp_cti_v_cfg;
	rkvop_cfg_peaking_struct 		sharp_peaking_cfg;
	rkvop_cfg_shootctrl_struct 		sharp_shootctrl_cfg;
	rkvop_cfg_global_gain_struct	sharp_global_gain_cfg;
	rkvop_cfg_color_ctrl_struct		sharp_color_adj_cfg;
	rkvop_cfg_texture_adj_struct	sharp_texture_adj_cfg;
}rk_hwpq_shp_cfg;
typedef rk_hwpq_shp_cfg RKHWPQ_SHP_CFG;

/* HW-PQ configuration*/
typedef struct _rk_hwpq_cfg
{
    rk_hwpq_shp_cfg rk_hwpq_shp_cfg;
    rk_hwpq_csc_cfg rk_hwpq_csc_cfg;
    rk_hwpq_acm_cfg rk_hwpq_acm_cfg;
    rk_hwpq_dci_cfg rk_hwpq_dci_cfg;
}rk_hwpq_cfg;

typedef struct _rk_vop_status
{
    uint16_t img_w_layer;
    uint16_t img_h_layer;
    uint16_t img_w_vp;
    uint16_t img_h_vp;
    uint16_t overlay_info;
    uint16_t reserved[8];
}rk_vop_status_t;
typedef struct _rk_hwpq_vdpp_info
{
    uint8_t* pHistAddr;
    uint32_t nHistLength;
    uint16_t nVdppHistImgW;
    uint16_t nVdppHistImgH;
    uint16_t nBlkSizeW;
    uint16_t nBlkSizeH;
}rk_hwpq_vdpp_info;

// Kernel ACM Struct Definition
#define     RKVOP_ACM_PROP_LENGTH   (2700)
typedef struct kernel_acm_prop {
    // Struct Header
    char        module_name[32];
    uint32_t    reserved0[8];
    // Property Info
    uint32_t    prop_length;
    uint8_t     prop_data[RKVOP_ACM_PROP_LENGTH];
    uint32_t    reserved1[8];
}kernel_acm_prop_t;

// Kernel CSC Struct Definition
#define     RKVOP_CSC_PROP_LENGTH   (80)
typedef struct kernel_csc_prop {
    // Struct Header
    char        module_name[32];
    uint32_t    reserved0[8];
    // Property Info
    uint32_t    prop_length;
    uint8_t     prop_data[RKVOP_CSC_PROP_LENGTH];
    uint32_t    reserved1[8];
}kernel_csc_prop_t;

// Kernel DCI Struct Definition
#define     RKVOP_DCI_PROP_LENGTH   (5700)
typedef struct kernel_dci_prop {
    // Struct Header
    char        module_name[32];
    uint32_t    reserved0[8];
    // Property Info
    uint32_t    prop_length;
    uint8_t     prop_data[RKVOP_DCI_PROP_LENGTH];
    uint32_t    reserved1[8];
}kernel_dci_prop_t;

// Kernel SHP Struct Definition
#define     RKVOP_SHP_PROP_LENGTH   (800)
typedef struct kernel_shp_prop {
    // Struct Header
    char        module_name[32];
    uint32_t    reserved0[8];
    // Property Info
    uint32_t    prop_length;
    uint8_t     prop_data[RKVOP_SHP_PROP_LENGTH];
    uint32_t    reserved1[8];
}kernel_shp_prop_t;

typedef struct _rk_hwpq_reg
{
    uint32_t          rk_hwpq_reg_version;
    uint32_t          reserved_0[4];
    kernel_shp_prop_t rk_hwpq_shp_reg;
    kernel_csc_prop_t rk_hwpq_csc_reg;
    kernel_acm_prop_t rk_hwpq_acm_reg;
    kernel_dci_prop_t rk_hwpq_dci_reg;
}rk_hwpq_reg;

/**
 * @Function: rk_hwpq_init()
 * @Descrptn: rk_hwpq_context initialization, create a rk_hwpq_context instance.
 * @Params:
 *      rk_hwpq_context *pCtxPtr - a pointer of the pq handle of context
 *      rk_hwpq_init_params *pInitParam - a pointer of the API initialization parameters
 * @Return: error code, 0 indicates everything is ok.
 */
int rk_hwpq_init(rk_hwpq_context *pCtxPtr, rk_hwpq_init_params *pInitParam);

/**
 * @Function: rk_hwpq_proc()
 * @Descrptn: call a rk_hwpq_context instance to execute RKPQ
 * @Params:
 *      rk_hwpq_context ctx - the pq handle of context
 *      rk_hwpq_proc_params *pProcParam - a pointer of the API execution parameters
 * @Return: error code, 0 indicates everything is ok.
 */
int rk_hwpq_proc(rk_hwpq_context ctx, rk_hwpq_proc_params *pProcParam);

/**
 * @Function: rk_hwpq_deinit()
 * @Descrptn: release rk_hwpq_context resource created by rk_hwpq_init().
 * @Params:
 *      rk_hwpq_context ctx - the pq handle of context
 * @Return: error code, 0 indicates everything is ok.
 */
int rk_hwpq_deinit(rk_hwpq_context ctx);

/**
 * @Function: rk_hwpq_query()
 * @Descrptn: query the information about image, buffer or others.
 * @Params:
 *      rk_hwpq_context ctx - the pq handle of context
 *      rk_hwpq_query_cmd cmd - the query command, see rk_hwpq_query_cmd
 *      size_t size - the buffer size of retuned information
 *      void *info - the buffer pointer of retuned information value
 * @Return: error code, 0 indicates everything is ok.
 * @Detail: the detail explanation of the arguments lists below:
 *  |       Query Command               |   Need A Context  |   Return Type     |
 *  | --------------------------------- | ----------------- | ----------------- |
 *  | RKHWPQ_QUERY_SDK_VERSION            |       no          | rk_hwpq_version_info |
 *  | RKHWPQ_QUERY_PERF_INFO              |       YES         | rk_hwpq_perf_info    |
 *  | RKHWPQ_QUERY_IMG_FMT_INPUT_SUPPORT  |       no          | rk_hwpq_imgfmt_info  |
 *  | RKHWPQ_QUERY_IMG_FMT_OUTPUT_SUPPORT |       no          | rk_hwpq_imgfmt_info  |
 *  | RKHWPQ_QUERY_IMG_FMT_CHANGE_SUPPORT |       no          | uint32_t          |
 *  | RKHWPQ_QUERY_IMG_RES_CHANGE_SUPPORT |       no          | uint32_t          |
 *  | RKHWPQ_QUERY_IMG_COLOR_SPACE_SUPPORT|       no          | rk_hwpq_clrspc_info  |
 *  | RKHWPQ_QUERY_IMG_BUF_INFO           |       no          | rk_hwpq_imgbuf_info  |
 *  | RKHWPQ_QUERY_IMG_ALIGNMENT_OCL      |       YES         | uint32_t          |
 *  | RKHWPQ_QUERY_RKNN_SUPPORT           |       YES         | uint32_t          |
 *  | RKHWPQ_QUERY_MEAN_LUMA              |       YES         | uint32_t          |
 *  | RKHWPQ_QUERY_MODULES_SUPPORT        |       YES         | rk_hwpq_module_info  |
 *  | RKHWPQ_QUERY_3DLUT_AI_TABLE         |       YES         | uint16_t[14739]   |
 */
int rk_hwpq_query(rk_hwpq_context ctx, rk_hwpq_query_cmd cmd, size_t size, void *info);

/**
 * @Function: rk_hwpq_set_loglevel()
 * @Descrptn: set the log level for a pq context
 * @Params:
 *      rk_hwpq_context ctx - the pq handle of context
 *      int logLevel - the log level, valid range: {0-OFF, 1-FATAL, 2-ERROR, 3-WARNING, 4-INFO}
 * @Return: error code, 0 indicates everything is ok.
 */
int rk_hwpq_set_loglevel(rk_hwpq_context ctx, int logLevel);


void rk_hwpq_set_shp_default_config(rk_hwpq_shp_cfg* p_shp_cfg);
void rk_hwpq_set_dci_default_config(rk_hwpq_dci_cfg* p_dci_cfg);
void rk_hwpq_set_acm_default_config(rk_hwpq_acm_cfg* p_acm_cfg);
void rk_hwpq_set_csc_default_config(rk_hwpq_csc_cfg* p_csc_cfg);

#ifdef __cplusplus
}
#endif


#endif // __RKHWPQ_API_H_
