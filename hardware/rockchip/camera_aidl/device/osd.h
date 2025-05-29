#pragma once
namespace android {
namespace hardware {
namespace camera {
namespace device {
namespace V3_4 {
namespace implementation {

#define PNG_LOGO "/vendor/etc/camera/osd_logo.png"
#define FONT_CN_FILE "/system/fonts/NotoSansCJK-Regular.ttc"
#define FONT_EN_FILE "/system/fonts/DroidSans.ttf"
#define OSD_TEXT L"%04d年%02d月%02d日 %02d:%02d:%02d"

extern void processOSD(int width,int height,unsigned long dst_fd,int index);


}  // namespace implementation
}  // namespace V3_4
}  // namespace device
}  // namespace camera
}  // namespace hardware
}  // namespace android
