package rockchip.hwc.proxy.aidl;

import rockchip.hwc.proxy.aidl.RkHwcProxyAidlRequest;
import rockchip.hwc.proxy.aidl.RkHwcProxyAidlResponse;
import rockchip.hwc.proxy.aidl.IRkHwcProxyAidlCallback;

@VintfStability
interface IRkHwcProxyAidl {
    const int STATUS_FAILED = 1;
    const int STATUS_SUCCESSFULLY = 2;
    RkHwcProxyAidlResponse run(in RkHwcProxyAidlRequest request);
    void registerCallback(in IRkHwcProxyAidlCallback callback);
    void unregisterCallback(in IRkHwcProxyAidlCallback callback);
}
