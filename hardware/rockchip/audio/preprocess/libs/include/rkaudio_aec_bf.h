#ifndef _RKAUDIO_AEC_BF_H_
#define _RKAUDIO_AEC_BF_H_

#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NUM_CHANNEL					 4
#define NUM_REF_CHANNEL              2
#define NUM_DROP_CHANNEL             0
#define REF_POSITION				 1
	//static short Array[NUM_SRC_CHANNEL] = { 9,7,5,3,2,4,6,8 }
	//static short Array[NUM_CHANNEL] = {2, 3, 0, 1}; //src first, ref second
	//static short Array[NUM_CHANNEL] = { 0, 1, 3, 2 };// , 4, 5 };
	//static short Array[NUM_CHANNEL] = { 10, 3, 11, 2, 4, 13, 5, 12, 0, 1, 6, 7, 8, 9, 14, 15};
	// static short Array[NUM_CHANNEL] = { 2, 3, 0, 1, 4, 5};
	//static short Array[NUM_CHANNEL] = { 4, 13, 5, 12, 10, 3, 11, 2, 0, 1, 6, 7, 8, 9 ,14, 15};//8mic + 8ref,drop last 6ref
	static short Array[NUM_CHANNEL] = {0, 1, 2, 3}; //src first, ref second

	//Array[NUM_SRC_CHANNEL] = { 2,3,4,5,6,7};
	/**********************EQ Parameter**********************/
	static short EqPara_16k[5][13] =
	{
		//filter_bank 1
		{-1 ,-1 ,-1 ,-1 ,-2 ,-1 ,-1 ,-1 ,-1 ,-1 ,-1 ,-2 ,-3 },
		//filter_bank 2
		{-1 ,-1 ,-1 ,-1 ,-2 ,-2 ,-3 ,-5 ,-3 ,-2 ,-1 ,-1 ,-2 },
		//filter_bank 3
		{-2 ,-5 ,-9 ,-4 ,-2 ,-2 ,-1 ,-5 ,-5 ,-11 ,-20 ,-11 ,-5 },
		//filter_bank 4
		{-5 ,-1 ,-7 ,-7 ,-19 ,-40 ,-20 ,-9 ,-10 ,-1 ,-20 ,-24 ,-60 },
		//filter_bank 5
		{-128 ,-76 ,-40 ,-44 ,-1 ,-82 ,-111 ,-383 ,-1161 ,-1040 ,-989 ,-3811 ,32764 },
	};

	/**********************AES Parameter**********************/
	static float LimitRatio[2][3] = {
		/* low freq   median freq    high freq*/
		{   1.5f,        1.2f,          1.2f  },  //limit
		{   1.5f,        1.2f,          1.2f  },  //ratio
	};

	/**********************THD Parameter**********************/
	static short ThdSplitFreq[4][2] = {
		{ 500,1000},
		{ 1000,2400},
		{ 2400,4000},
		{ 0,0},
	};

	static float ThdSupDegree[4][10] =
	{
		/* 2th		3th		4th		5th		6th		7th		8th		9th		10th	11th order  */
		{ 0.005f, 0.005f,	0,		0,		0,		0,		0,		0,		0,		0},
		{ 0.005f, 0.005f,   0.005f,      0,		0,		0,		0,		0,		0,		0},
		{ 0.005f, 0.005f, 0.005f,   0.005f, 0,	0,	0,0,		0,		0,},
		{ 0.003f, 0.003f, 0.004f,	0.005f, 0.003f, 0.003f, 0.003f,	0,		0,		0},
	};
	static short HardSplitFreq[5][2] = {
		{ 3800,5500},	//1 to 4 is select hard suppress freq bin
		{ 5500,6200},
		{ 6200,8000},
		{ 0,0},
		{ 300,1800},//freq use to calculate mean_G
	};
	static float HardThreshold[4] = { 0.15,0.15, 0.25, 0.15 };

	/*************************************************/
	/*The Main Enable which used to control the AEC,BF and RX*/
	typedef enum RKAUDIOEnable_
	{
		RKAUDIO_EN_AEC = 1 << 0,
		RKAUDIO_EN_BF = 1 << 1,
		RKAUDIO_EN_RX = 1 << 2,
		RKAUDIO_EN_CMD = 1 << 3,
	} RKAUDIOEnable;

	/* The Sub-Enable which used to control the AEC,BF and RX*/
	typedef enum RKAecEnable_
	{
		EN_DELAY = 1 << 0,
		EN_ARRAY_RESET = 1 << 1,
	} RKAecEnable;
	typedef enum RKPreprocessEnable_
	{
		EN_Fastaec = 1 << 0,
		EN_Wakeup = 1 << 1,
		EN_Dereverberation = 1 << 2,
		EN_Nlp = 1 << 3,
		EN_AES = 1 << 4,
		EN_Agc = 1 << 5,
		EN_Anr = 1 << 6,
		EN_GSC = 1 << 7,
		GSC_Method = 1 << 8,
		EN_Fix = 1 << 9,
		EN_STDT = 1 << 10,
		EN_CNG = 1 << 11,
		EN_EQ = 1 << 12,
		EN_CHN_SELECT = 1 << 13,
		EN_HOWLING = 1 << 14,
		EN_DOA = 1 << 15,
		EN_WIND = 1 << 16,
		EN_AINR = 1 << 17,
	} RKPreprocessEnable;
	typedef enum RkaudioRxEnable_
	{
		EN_RX_Anr = 1 << 0,
		EN_RX_HOWLING = 1 << 1,
	} RkaudioRxEnable;
	/*****************************************/

	/* Set the three Main Para which used to initialize the AEC,BF and RX*/
	typedef struct SKVAECParameter_ {
		int pos;
		int drop_ref_channel;
		int model_aec_en;
		int delay_len;
		int look_ahead;
		short * Array_list;
		//mdf
		short filter_len;
		//delay
		void* delay_para;
	} SKVAECParameter;

	typedef struct SKVPreprocessParam_
	{
		/* Parameters of agc */
		int model_bf_en;
		int ref_pos;
		int Targ;
		int num_ref_channel;
		int drop_ref_channel;
		void* dereverb_para;
		void* aes_para;
		void* nlp_para;
		void* anr_para;
		void* agc_para;
		void* cng_para;
		void* dtd_para;
		void* eq_para;
		void* howl_para;
		void* doa_para;
	}SKVPreprocessParam;

	typedef struct RkaudioRxParam_
	{
		/* Parameters of agc */
		int model_rx_en;
		void* anr_para;
		void* howl_para;
	}RkaudioRxParam;
	/****************************************/
	/*The param struct of sub-mudule of AEC,BF and RX*/
	typedef struct RKAudioDelayParam_ {
		short	MaxFrame;
		short	LeastDelay;
		short	JumpFrame;
		short	DelayOffset;
		short	MicAmpThr;
		short	RefAmpThr;
		short	StartFreq;
		short	EndFreq;
		float	SmoothFactor;
	}RKAudioDelayParam;

	typedef struct SKVANRParam_ {
		float noiseFactor;
		int   swU;
		float PsiMin;
		float PsiMax;
		float fGmin;

		short Sup_Freq1;
		short Sup_Freq2;
		float Sup_Energy1;
		float Sup_Energy2;

		short InterV;
		float BiasMin;
		short UpdateFrm;
		float NPreGammaThr;
		float NPreZetaThr;
		float SabsGammaThr0;
		float SabsGammaThr1;
		float InfSmooth;
		float ProbSmooth;
		float CompCoeff;
		float PrioriMin;
		float PostMax;
		float PrioriRatio;
		float PrioriRatioLow;
		int   SplitBand;
		float PrioriSmooth;
		//transient
		short TranMode;
	} SKVANRParam;

	typedef struct RKAudioDereverbParam_
	{
		int		rlsLg;
		int     curveLg;
		int		delay;
		float   forgetting;
		float   T60;
		float	coCoeff;
	} RKAudioDereverbParam;

	typedef struct RKAudioAESParameter_ {
		float	Beta_Up;
		float	Beta_Down;
		float	Beta_Up_Low;
		float	Beta_Down_Low;
		short	low_freq;
		short	high_freq;
		short	THD_Flag;
		short	HARD_Flag;
		float	LimitRatio[2][3];
		short	ThdSplitFreq[4][2];
		float	ThdSupDegree[4][10];
		short	HardSplitFreq[5][2];
		float	HardThreshold[4];
	} RKAudioAESParameter;

	typedef struct RKDTDParam_
	{
		float ksiThd_high;			 /* 单双讲判决阈值 */
		float ksiThd_low;			 /* 单双讲判决阈值 */

	}RKDTDParam;

	typedef struct SKVNLPParameter_ {
		short int g_ashwAecBandNlpPara_16k[8][2];
	} SKVNLPParameter;

	typedef struct RKAGCParam_ {
		/* 新版AGC参数 */
		float              attack_time;  /* 触发时间，即AGC增益下降所需要的时间 */
		float			   release_time; /* 施放时间，即AGC增益上升所需要的时间 */
		float              max_gain; /* 最大增益，同时也是线性段增益，单位：dB */
		float 			   max_peak; /* 经AGC处理后，输出语音的最大能量，范围：单位：dB */
		float              fRth0;    /* 扩张段结束能量dB阈值，同时也是线性段开始阈值 */
		float              fRk0;     /* 扩张段斜率 */
		float              fRth1;    /* 压缩段起始能量dB阈值，同时也是线性段结束阈值 */

		/* 无效参数 */
		int            fs;                       /* 数据采样率 */
		int            frmlen;                   /* 处理帧长 */
		float          attenuate_time; /* 噪声衰减时间，即噪声段增益衰减到1所需的时间 */
		float          fRth2;                     /* 压缩段起始能量dB阈值 */
		float          fRk1;                      /* 扩张段斜率 */
		float          fRk2;                      /* 扩张段斜率 */
		float          fLineGainDb;               /* 线性段提升dB数 */
		int            swSmL0;                    /* 扩张段时域平滑点数 */
		int            swSmL1;                    /* 线性段时域平滑点数 */
		int            swSmL2;                    /* 压缩段时域平滑点数 */

	} RKAGCParam;

	typedef struct RKCNGParam_
	{
		/*CNG Parameter*/
		float              fGain;                     /* INT16 Q0 施加舒适噪声幅度比例 */
		float              fMpy;						/* INT16 Q0 白噪随机数生成幅度 */
		float              fSmoothAlpha;              /* 舒适噪声平滑系数 */
		float              fSpeechGain;               /* 根据语音能量额外施加舒适噪声比例增益 */
	} RKCNGParam;

	typedef struct RKaudioEqParam_ {
		int shwParaLen;           // 滤波器系数个数
		short pfCoeff[5][13];          // 滤波器系数
	} RKaudioEqParam;

	typedef struct RKHOWLParam_
	{
		short howlMode;
	}RKHOWLParam;
	typedef struct RKDOAParam_
	{
		float rad;//线阵2mic间距，圆阵则为半径；阵列不支持指定，必须根据库而定（比如出的圆阵库则只支持圆阵定位。）
		short start_freq;
		short end_freq;
		short lg_num;			//该数值应该为偶数
		short lg_pitch_num;		//only used for circle array, linear array must be 1, 俯仰角扫描。
	}RKDOAParam;
	/*************** TX ***************/

	/* Set the Sub-Para which used to initialize the DELAY*/
	inline static void* rkaudio_delay_param_init() {
		RKAudioDelayParam* param = (RKAudioDelayParam*)malloc(sizeof(RKAudioDelayParam));
		param->MaxFrame = 32;		/* delay最长估计帧数 */
		param->LeastDelay = 0;		/* delay最短估计帧数 */
		param->JumpFrame = 12;		/* 跳过帧数 */
		param->DelayOffset = 1;		/* delay offset帧数 */
		param->MicAmpThr = 50;		/* mic端最小能量阈值 */
		param->RefAmpThr = 50;		/* ref端最小能量阈值 */
		param->StartFreq = 500;		/* 延时估计起始频段的频率 */
		param->EndFreq = 4000;		/* 延时估计终止频段的频率 */
		param->SmoothFactor = 0.97f;
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the ANR*/
	inline static void* rkaudio_anr_param_init_tx() {
		SKVANRParam* param = (SKVANRParam*)malloc(sizeof(SKVANRParam));
		/* anr parameters */
		param->noiseFactor = 0.88f;//-3588.0f to compatible old json
		//param->noiseFactor = -3588.0f;
		param->swU = 20;
		param->PsiMin = 0.02;
		param->PsiMax = 0.516;
		param->fGmin = 0.05;
		param->Sup_Freq1 = -3588;
		param->Sup_Freq2 = -3588;
		param->Sup_Energy1 = 10000;
		param->Sup_Energy2 = 10000;

		param->InterV = 8;				//ANR_NOISE_EST_V
		param->BiasMin = 1.67f;			//ANR_NOISE_EST_BMIN
		param->UpdateFrm = 15;			//UPDATE_FRAME
		param->NPreGammaThr = 4.6f;		//ANR_NOISE_EST_GAMMA0
		param->NPreZetaThr = 1.67f;		//ANR_NOISE_EST_PSI0
		param->SabsGammaThr0 = 1.0f;	//ANR_NOISE_EST_GAMMA2
		param->SabsGammaThr1 = 3.0f;	//ANR_NOISE_EST_GAMMA1
		param->InfSmooth = 0.8f;		//ANR_NOISE_EST_ALPHA_S
		param->ProbSmooth = 0.7f;		//ANR_NOISE_EST_ALPHA_D
		param->CompCoeff = 1.4f;		//ANR_NOISE_EST_BETA
		param->PrioriMin = 0.0316f;		//ANR_NOISE_EST_ESP_MIN
		param->PostMax = 40.0f;			//ANR_NOISE_EST_GAMMA_MAX
		param->PrioriRatio = 0.95f;		//ANR_NOISE_EST_ALPHA
		param->PrioriRatioLow = 0.95f;	//ANR_NOISE_EST_ALPHA
		param->SplitBand = 20;
		param->PrioriSmooth = 0.7f;		//ANR_ENHANCE_BETA

		//transient
		param->TranMode = 0;
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the Dereverb*/
	inline static void* rkaudio_dereverb_param_init() {
		RKAudioDereverbParam* param = (RKAudioDereverbParam*)malloc(sizeof(RKAudioDereverbParam));
		param->rlsLg = 4;			/* RLS滤波器阶数 */
		param->curveLg = 30;		/* 分布曲线阶数 */
		param->delay = 2;			/* RLS滤波器延时 */
		param->forgetting = 0.98;	/* RLS滤波器遗忘因子 */
		param->T60 = 0.3;//1.5;		/* 混响时间估计值（单位：s），越大，去混响能力越强，但是越容易过消除 */
		param->coCoeff = 1;			/* 互相干性调整系数，防止过消除，越大能力越强，建议取值：0.5到2之间 */
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the AES*/
	inline static void* rkaudio_aes_param_init() {
		RKAudioAESParameter* param = (RKAudioAESParameter*)malloc(sizeof(RKAudioAESParameter));
		param->Beta_Up = 0.002f; /* 上升速度 -3588.0f to compatible old json*/
		//param->Beta_Up = -3588.0f;
		param->Beta_Down = 0.001f; /* 下降速度 */
		param->Beta_Up_Low = 0.005f; /* 低频上升速度 */
		param->Beta_Down_Low = 0.001f; /* 低频下降速度 */
		param->low_freq = 1000;
		param->high_freq = 3750;
		param->THD_Flag = 0;	/* 1 open THD, 0 close THD */
		param->HARD_Flag = 0;	/* 1 open Hard Suppress, 0 close Hard Suppress */
		int i, j;
		for (i = 0; i < 2; i++)
			for (j = 0; j < 3; j++)
				param->LimitRatio[i][j] = LimitRatio[i][j];
		for (i = 0; i < 4; i++)
			for (j = 0; j < 2; j++)
				param->ThdSplitFreq[i][j] = ThdSplitFreq[i][j];
		for (i = 0; i < 4; i++)
			for (j = 0; j < 10; j++)
				param->ThdSupDegree[i][j] = ThdSupDegree[i][j];

		for (i = 0; i < 5; i++)
			for (j = 0; j < 2; j++)
				param->HardSplitFreq[i][j] = HardSplitFreq[i][j];
		for (i = 0; i < 4; i++)
			param->HardThreshold[i] = HardThreshold[i];
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the DTD*/
	inline static void* rkaudio_dtd_param_init()
	{
		RKDTDParam* param = (RKDTDParam*)malloc(sizeof(RKDTDParam));
		/* dtd paremeters*/
		param->ksiThd_high = 0.60f;										            /* 单双讲判决阈值 */
		param->ksiThd_low = 0.50f;
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the AGC*/
	inline static void* rkaudio_agc_param_init()
	{
		RKAGCParam* param = (RKAGCParam*)malloc(sizeof(RKAGCParam));

		/* 新版AGC参数 */
		param->attack_time = 200.0;		/* 触发时间，即AGC增益上升所需要的时间 */
		param->release_time = 400.0;	/* 施放时间，即AGC增益下降所需要的时间 */
		//param->max_gain = 35.0;		/* 最大增益，同时也是线性段增益，单位：dB */
		param->max_gain = 40;			/* 最大增益，同时也是线性段增益，单位：dB */
		param->max_peak = -1.0;			/* 经AGC处理后，输出语音的最大能量，范围：单位：dB */
		param->fRk0 = 2;				/* 扩张段斜率 */
		param->fRth2 = -47;				/* 压缩段起始能量dB阈值，同时也是线性段结束阈值，增益逐渐降低，注意 fRth2 + max_gain < max_peak */
		param->fRth1 = -75;				/* 扩张段结束能量dB阈值，同时也是线性段开始阈值，能量高于改区域以max_gain增益 */
		param->fRth0 = -80;				/* 噪声门阈值 */

		/* 无效参数 */
		param->fs = 16000;                       /* 数据采样率 */
		param->frmlen = 256;                   /* 处理帧长 */
		param->attenuate_time = 1000; /* 噪声衰减时间，即噪声段增益衰减到1所需的时间 */
		param->fRk1 = 0.8;                      /* 扩张段斜率 */
		param->fRk2 = 0.4;                      /* 扩张段斜率 */
		param->fLineGainDb = -25.0f;               /* 低于该值，起始的attenuate_time(ms)内不做增益 */
		param->swSmL0 = 40;                    /* 扩张段时域平滑点数 */
		param->swSmL1 = 80;                    /* 线性段时域平滑点数 */
		param->swSmL2 = 80;                    /* 压缩段时域平滑点数 */

		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the CNG*/
	inline static void* rkaudio_cng_param_init()
	{
		RKCNGParam* param = (RKCNGParam*)malloc(sizeof(RKCNGParam));
		/* cng paremeters */
		param->fSmoothAlpha = 0.99f;										            /* INT16 Q15 施加舒适噪声平滑度 */
		param->fSpeechGain = 0;										                /* INT16 Q15 施加舒适噪声语音纹理模拟程度 */
		param->fGain = 10.0;                                           /* INT16 Q0 施加舒适噪声幅度比例 */
		param->fMpy = 10;                                            /* INT16 Q0 白噪随机数生成幅度 */
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the EQ*/
	inline static void* rkaudio_eq_param_init() {
		RKaudioEqParam* param = (RKaudioEqParam*)malloc(sizeof(RKaudioEqParam));
		param->shwParaLen = 65;
		int i, j;
		for (i = 0; i < 5; i++) {
			for (j = 0; j < 13; j++) {
				param->pfCoeff[i][j] = EqPara_16k[i][j];
			}
		}
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the HOWL*/
	inline static void* rkaudio_howl_param_init_tx() {
		RKHOWLParam* param = (RKHOWLParam*)malloc(sizeof(RKHOWLParam));
		param->howlMode = 1;
		return (void*)param;
	}
	inline static void* rkaudio_doa_param_init() {
		RKDOAParam* param = (RKDOAParam*)malloc(sizeof(RKDOAParam));
		param->rad = 0.04f;
		param->start_freq = 1000;
		param->end_freq = 4000;
		param->lg_num = 40;
		param->lg_pitch_num = 1;
		return (void*)param;
	}
	/************* RX *************/
	inline static void* rkaudio_anr_param_init_rx() {
		SKVANRParam* param = (SKVANRParam*)malloc(sizeof(SKVANRParam));
		/* anr parameters */
		param->noiseFactor = 0.88f;
		param->swU = 10;
		param->PsiMin = 0.02;
		param->PsiMax = 0.516;
		param->fGmin = 0.05;

		param->Sup_Freq1 = -3588;
		param->Sup_Freq2 = -3588;
		param->Sup_Energy1 = 100000;
		param->Sup_Energy2 = 100000;

		param->InterV = 8;				//ANR_NOISE_EST_V
		param->BiasMin = 1.67f;			//ANR_NOISE_EST_BMIN
		param->UpdateFrm = 15;			//UPDATE_FRAME
		param->NPreGammaThr = 4.6f;		//ANR_NOISE_EST_GAMMA0
		param->NPreZetaThr = 1.67f;		//ANR_NOISE_EST_PSI0
		param->SabsGammaThr0 = 1.0f;	//ANR_NOISE_EST_GAMMA2
		param->SabsGammaThr1 = 3.0f;	//ANR_NOISE_EST_GAMMA1
		param->InfSmooth = 0.8f;		//ANR_NOISE_EST_ALPHA_S
		param->ProbSmooth = 0.7f;		//ANR_NOISE_EST_ALPHA_D
		param->CompCoeff = 1.4f;		//ANR_NOISE_EST_BETA
		param->PrioriMin = 0.0316f;		//ANR_NOISE_EST_ESP_MIN
		param->PostMax = 40.0f;			//ANR_NOISE_EST_GAMMA_MAX
		param->PrioriRatio = 0.95f;		//ANR_NOISE_EST_ALPHA
		param->PrioriRatioLow = 0.95f;	//ANR_NOISE_EST_ALPHA
		param->SplitBand = 20;
		param->PrioriSmooth = 0.7f;		//ANR_ENHANCE_BETA

		//transient
		param->TranMode = 0;

		return (void*)param;
	}
	inline static void* rkaudio_howl_param_init_rx() {
		RKHOWLParam* param = (RKHOWLParam*)malloc(sizeof(RKHOWLParam));
		param->howlMode = 2;
		return (void*)param;
	}

	/* Set the Sub-Para which used to initialize the AEC*/
	inline static void* rkaudio_aec_param_init()
	{
		SKVAECParameter* param = (SKVAECParameter*)malloc(sizeof(SKVAECParameter));
		param->pos = REF_POSITION;
		param->drop_ref_channel = NUM_DROP_CHANNEL;
		// param->model_aec_en = EN_DELAY;//param->model_aec_en = EN_DELAY;
		param->delay_len = 0;//-3588 to compatible old json
		param->look_ahead = 0;
		param->Array_list = Array;
		//mdf
		param->filter_len = 2;
		//delay
		param->delay_para = rkaudio_delay_param_init();
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the BF*/
	inline static void* rkaudio_preprocess_param_init()
	{
		SKVPreprocessParam* param = (SKVPreprocessParam*)malloc(sizeof(SKVPreprocessParam));
		//param->model_bf_en = EN_Fastaec | EN_AES | EN_Agc | EN_Anr | EN_Dereverberation;
		//param->model_bf_en = EN_Fastaec | EN_AES ;
		param->model_bf_en = EN_Fastaec | EN_AES | EN_Anr | EN_Dereverberation;
		//param->model_bf_en = EN_Anr | EN_Dereverberation | EN_Agc;
		//param->model_bf_en = EN_Fastaec | EN_Anr;
		//param->model_bf_en = EN_Agc | EN_Anr | EN_Dereverberation;
		//param->model_bf_en = EN_Fastaec | EN_AES | EN_Anr | EN_Dereverberation;
		//param->model_bf_en = EN_Wakeup;
		//param->model_bf_en = EN_Agc;
		//param->model_bf_en = EN_Fastaec | EN_AES | EN_Anr | EN_Agc;
		param->Targ = 0;
		param->ref_pos = REF_POSITION;
		param->num_ref_channel = NUM_REF_CHANNEL;
		param->drop_ref_channel = NUM_DROP_CHANNEL;
		param->anr_para = rkaudio_anr_param_init_tx();
		param->dereverb_para = rkaudio_dereverb_param_init();
		param->aes_para = rkaudio_aes_param_init();
		param->dtd_para = rkaudio_dtd_param_init();
		param->agc_para = rkaudio_agc_param_init();
		param->cng_para = rkaudio_cng_param_init();
		param->eq_para = rkaudio_eq_param_init();
		param->howl_para = rkaudio_howl_param_init_tx();
		param->doa_para = rkaudio_doa_param_init();
		return (void*)param;
	}
	/* Set the Sub-Para which used to initialize the RX*/
	inline static void* rkaudio_rx_param_init()
	{
		RkaudioRxParam* param = (RkaudioRxParam*)malloc(sizeof(RkaudioRxParam));
		param->model_rx_en = EN_RX_Anr;
		param->anr_para = rkaudio_anr_param_init_rx();
		param->howl_para = rkaudio_howl_param_init_rx();
		return (void*)param;
	}
	typedef struct RKAUDIOParam_
	{
		int model_en;
		void* aec_param;
		void* bf_param;
		void* rx_param;
		int read_size;
	} RKAUDIOParam;

	inline static void rkaudio_aec_param_destory(void* param_)
	{
		SKVAECParameter* param = (SKVAECParameter*)param_;
		free(param->delay_para); param->delay_para = NULL;
		free(param); param = NULL;
	}

	inline static void rkaudio_preprocess_param_destory(void* param_)
	{
		SKVPreprocessParam* param = (SKVPreprocessParam*)param_;
		free(param->dereverb_para); param->dereverb_para = NULL;
		free(param->aes_para); param->aes_para = NULL;
		free(param->anr_para); param->anr_para = NULL;
		free(param->agc_para); param->agc_para = NULL;
		param->nlp_para = NULL;
		free(param->cng_para); param->cng_para = NULL;
		free(param->dtd_para); param->dtd_para = NULL;
		free(param->eq_para); param->eq_para = NULL;
		free(param->howl_para); param->howl_para = NULL;
		free(param->doa_para); param->doa_para = NULL;
		free(param); param = NULL;
	}

	inline static void rkaudio_rx_param_destory(void* param_)
	{
		RkaudioRxParam* param = (RkaudioRxParam*)param_;
		free(param->anr_para); param->anr_para = NULL;
		free(param->howl_para); param->howl_para = NULL;
		free(param); param = NULL;
	}

	inline static void rkaudio_param_deinit(void* param_)
	{
		RKAUDIOParam* param = (RKAUDIOParam*)param_;
		if (param->aec_param != NULL)
			rkaudio_aec_param_destory(param->aec_param);
		if (param->bf_param != NULL)
			rkaudio_preprocess_param_destory(param->bf_param);
		if (param->rx_param != NULL)
			rkaudio_rx_param_destory(param->rx_param);
	}

	void* rkaudio_preprocess_init(int rate, int bits, int src_chan, int ref_chan, RKAUDIOParam* param);
	void rkaudio_param_printf(int src_chan, int ref_chan, RKAUDIOParam* param);
	int rkaudio_Doa_invoke(void* st_ptr);
	int rkaudio_Cir_Doa_invoke(void* st_ptr, int* ang_doa, int* pth_doa);

	int rkaudio_preprocess_get_cmd_id(void* st_ptr, float* cmd_score, int* cmd_id);

	int rkaudio_preprocess_get_asr_id(void* st_ptr, float* asr_score, int* asr_id);

	int rkaudio_param_set(void* st_ptr, int rkaudio_enable, int rkaec_enable, int rkbf_enable);

	void rkaudio_preprocess_destory(void* st_ptr);

	int rkaudio_preprocess_short(void* st_ptr, short* in, short* out, int in_size, int* wakeup_status);
	int rkaudio_rx_short(void* st_ptr, short* in, short* out);
	void rkaudio_asr_set_param(float min, float max, float keep);
	int rkaudio_rknn_path_set(char* asr_rknn_path_, char* kws_rknn_path_);
	void rkaudio_param_deinit(void* param_);
#ifdef __cplusplus
}
#endif

#endif    // _RKAUDIO_AEC_BF_H_
