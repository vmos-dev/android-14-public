#pragma once

// Bn*.h是通过aidl的编译，自动生成的，可以在out下面找到
#include <aidl/rockchip/hwc/proxy/aidl/BnRkHwcProxyAidl.h>

namespace aidl {
namespace rockchip {
namespace hwc {
namespace proxy {
namespace aidl {
namespace impl {

using ::aidl::rockchip::hwc::proxy::aidl::BnRkHwcProxyAidl;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlRequest;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlResponse;
using ::aidl::rockchip::hwc::proxy::aidl::IRkHwcProxyAidlCallback;

// 实现接口，Bn*是通过aidl的编译，自动生成的class
// 可以在out下面找到
class RkHwcProxyAidl : public BnRkHwcProxyAidl {
public:
    // 如果有需要修改初始化一些参数，自己修改构造函数
    RkHwcProxyAidl();

    ::ndk::ScopedAStatus run(const RkHwcProxyAidlRequest& request, RkHwcProxyAidlResponse* response) override;
    ::ndk::ScopedAStatus registerCallback(const std::shared_ptr<IRkHwcProxyAidlCallback>& callback) override;
    ::ndk::ScopedAStatus unregisterCallback(const std::shared_ptr<IRkHwcProxyAidlCallback>& callback) override;

    void internalLoop();
private:
    std::mutex callbacks_lock_;
    std::vector<std::shared_ptr<IRkHwcProxyAidlCallback>> callbacks_;
    std::mutex results_lock_;
    std::vector<RkHwcProxyAidlResponse> results_;
};

} // namespace impl
} // namespace aidl
} // namespace proxy
} // namespace hwc
} // namespace rockchip
} // namespace aidl
