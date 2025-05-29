#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

#include "RkHwcProxyAidl.h"

using ::aidl::rockchip::hwc::proxy::aidl::impl::RkHwcProxyAidl;

int main() {
//    ABinderProcess_setThreadPoolMaxThreadCount(2);
    ABinderProcess_startThreadPool();
    std::shared_ptr<RkHwcProxyAidl> rkHwcProxyAidl = ndk::SharedRefBase::make<RkHwcProxyAidl>();

    // 这个instance就是service name，SELinux配置时需要用
    const std::string instance = std::string() + RkHwcProxyAidl::descriptor + "/default";
    // 通过NDK的方法注册
    // 这样client在system和vendor都能够调用的到此service
    binder_status_t status = AServiceManager_addService(rkHwcProxyAidl->asBinder().get(), instance.c_str());
    CHECK_EQ(status, STATUS_OK);

    rkHwcProxyAidl->internalLoop();
    //ABinderProcess_joinThreadPool();
    return -1; // Should never be reached
}
