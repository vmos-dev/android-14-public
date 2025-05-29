package rockchip.hwc.proxy.aidl;

@VintfStability
parcelable RkHwcProxyAidlRequest {
    int id;
    int req_len;
    int ret_len;
    byte[] data;
}
