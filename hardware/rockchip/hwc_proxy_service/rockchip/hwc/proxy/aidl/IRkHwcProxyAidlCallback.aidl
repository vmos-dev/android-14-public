package rockchip.hwc.proxy.aidl;

import rockchip.hwc.proxy.aidl.RkHwcProxyAidlResponse;
import rockchip.hwc.proxy.aidl.RkHwcProxyAidlRequest;

@VintfStability
interface IRkHwcProxyAidlCallback {
    oneway void asyncDone(in RkHwcProxyAidlResponse response);
    void onCallback(in RkHwcProxyAidlRequest request, out RkHwcProxyAidlResponse response);
}
