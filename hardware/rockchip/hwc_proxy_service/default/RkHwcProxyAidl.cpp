#include "RkHwcProxyAidl.h"

#include <log/log.h>
#include <DrmApi.h>

namespace aidl {
namespace rockchip {
namespace hwc {
namespace proxy {
namespace aidl {
namespace impl {

RkHwcProxyAidl::RkHwcProxyAidl() {
    ALOGI("Initial RkHwcProxyAidl class.");
}

void RkHwcProxyAidl::internalLoop() {
    while (true) {
        usleep(1000000);
        if (callbacks_.size() == 0) continue;

        std::unique_lock<decltype(callbacks_lock_)> lock(callbacks_lock_);
        for (auto cb : callbacks_) {
            std::lock_guard<decltype(results_lock_)> lock(results_lock_);
            for (auto ret : results_) {
                cb->asyncDone(ret);
            }
        }
        lock.unlock();
    }
}

::ndk::ScopedAStatus RkHwcProxyAidl::run(const RkHwcProxyAidlRequest& request, RkHwcProxyAidlResponse* response) {
	std::unique_lock<decltype(callbacks_lock_)> lock(callbacks_lock_);
	for (auto cb : callbacks_) {
		cb->onCallback(request, response);
	}
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus RkHwcProxyAidl::registerCallback(const std::shared_ptr<IRkHwcProxyAidlCallback>& callback) {
    ALOGI("registerCallback...");
    if (callback == nullptr) {
        ALOGI("registerCallback failed, nullptr!");
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }

    {
        std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);
        callbacks_.emplace_back(callback);
        // unlock
    }
    ALOGI("registerCallback OK!");
    return ndk::ScopedAStatus::ok();
}
::ndk::ScopedAStatus RkHwcProxyAidl::unregisterCallback(const std::shared_ptr<IRkHwcProxyAidlCallback>& callback) {
    if (callback == nullptr) {
        // For now, this shouldn't happen because argument is not nullable.
        return ndk::ScopedAStatus::fromExceptionCode(EX_NULL_POINTER);
    }

    std::lock_guard<decltype(callbacks_lock_)> lock(callbacks_lock_);

    auto matches = [callback](const auto& list) {
        return list->asBinder() == callback->asBinder();  // compares binder object
    };
    auto it = std::remove_if(callbacks_.begin(), callbacks_.end(), matches);
    bool removed = (it != callbacks_.end());
    callbacks_.erase(it, callbacks_.end());  // calls unlinkToDeath on deleted callbacks.
    return removed ? ndk::ScopedAStatus::ok()
                   : ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
}
} // namespace impl
} // namespace aidl
} // namespace proxy
} // namespace hwc
} // namespace rockchip
} // namespace aidl
