#ifndef RGAUTILS_H
#define RGAUTILS_H
namespace hwc_rga_utils{

int HwcGetRgaFormat(int format);
int UnifyAndroidFormatForRK3588(int format);
bool isRK3588RGA3SupportFormat(int format);
int UnifyAndroidFormatForRK3576(int format);
bool isRK3576RGA2SupportFormat(int format);

#ifndef IM_SCHEDULER_RGA2_CORE0
#define IM_SCHEDULER_RGA2_CORE0 (1 << 2)
#endif

//RK3576新增RGA2_CORE1
#ifndef IM_SCHEDULER_RGA2_CORE1
#define IM_SCHEDULER_RGA2_CORE1 (1 << 3)
#endif

//RK3576新增RFBC支持
#ifndef IM_RKFBC64x4_MODE
#define IM_RKFBC64x4_MODE (1 << 4)
#endif

//RK3576新增AFBC32x8支持
#ifndef IM_AFBC32x8_MODE
#define IM_AFBC32x8_MODE (1 << 5)
#endif

};
#endif //RGAUTILS_H