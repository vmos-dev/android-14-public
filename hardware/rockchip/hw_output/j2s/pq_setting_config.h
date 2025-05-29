#define HIGH_QUALITY "HIGH_QUALITY_MODE"
#define LOW_LATENCY "LOW_LATENCY_MODE"

typedef struct {
    char* name;
    bool cscEnable;
    char* cscMode;
    unsigned int cscBrightness;
    unsigned int cscHue;
    unsigned int cscContrast;
    unsigned int cscSaturation;
    unsigned int cscRGain;
    unsigned int cscGGain;
    unsigned int cscBGain;
} csc;

typedef struct {
    char* name;
    bool dciEnable;
    unsigned short dciWgtCoef_low[33];
    unsigned short dciWgtCoef_mid[33];
    unsigned short dciWgtCoef_high[33];
    unsigned short dciWeight_low[32];
    unsigned short dciWeight_mid[32];
    unsigned short dciWeight_high[32];
} dci;

typedef struct {
    char* name;
    bool acmEnable;
    short acmTableDeltaYbyH[65];
    short acmTableDeltaHbyH[65];
    short acmTableDeltaSbyH[65];
    short acmTableGainYbyY[585];
    short acmTableGainHbyY[585];
    short acmTableGainSbyY[585];
    short acmTableGainYbyS[845];
    short acmTableGainHbyS[845];
    short acmTableGainSbyS[845];
    unsigned int lumGain;
    unsigned int hueGain;
    unsigned int satGain;
} acm;

typedef struct {
    char* name;
    bool gammaEnable;
    unsigned short gammaTab_R[1024];
    unsigned short gammaTab_G[1024];
    unsigned short gammaTab_B[1024];
} gamma;

typedef struct {
    char* name;
    bool sharpEnable;
    unsigned int sharpPeakingGain;
    bool sharpEnableShootCtrl;
    unsigned int sharpShootCtrlOver;
    unsigned int sharpShootCtrlUnder;
    bool sharpEnableCoringCtrl;
    unsigned short sharpCoringCtrlRatio0;
    unsigned short sharpCoringCtrlRatio1;
    unsigned short sharpCoringCtrlRatio2;
    unsigned short sharpCoringCtrlRatio3;
    unsigned short sharpCoringCtrlZero0;
    unsigned short sharpCoringCtrlZero1;
    unsigned short sharpCoringCtrlZero2;
    unsigned short sharpCoringCtrlZero3;
    unsigned short sharpCoringCtrlThrd0;
    unsigned short sharpCoringCtrlThrd1;
    unsigned short sharpCoringCtrlThrd2;
    unsigned short sharpCoringCtrlThrd3;
    bool sharpEnableGainCtrl;
    unsigned short sharpGainCtrlPos0;
    unsigned short sharpGainCtrlPos1;
    unsigned short sharpGainCtrlPos2;
    unsigned short sharpGainCtrlPos3;
    bool sharpEnableLimitCtrl;
    unsigned short sharpLimitCtrlPos00;
    unsigned short sharpLimitCtrlPos01;
    unsigned short sharpLimitCtrlPos02;
    unsigned short sharpLimitCtrlPos03;
    unsigned short sharpLimitCtrlPos10;
    unsigned short sharpLimitCtrlPos11;
    unsigned short sharpLimitCtrlPos12;
    unsigned short sharpLimitCtrlPos13;
    unsigned short sharpLimitCtrlBndPos0;
    unsigned short sharpLimitCtrlBndPos1;
    unsigned short sharpLimitCtrlBndPos2;
    unsigned short sharpLimitCtrlBndPos3;
    unsigned short sharpLimitCtrlRatio0;
    unsigned short sharpLimitCtrlRatio1;
    unsigned short sharpLimitCtrlRatio2;
    unsigned short sharpLimitCtrlRatio3;
} sharp;

typedef struct {
    csc csc[1];
    dci dci[6];
    acm acm[6];
    gamma gamma[10];
    sharp sharp[16];
} pq_tuning_param;

typedef struct {
    pq_tuning_param pq_tuning_param;
} pq_setting_config;
