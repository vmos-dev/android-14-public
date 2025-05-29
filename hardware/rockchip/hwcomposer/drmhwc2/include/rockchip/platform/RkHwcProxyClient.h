/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <aidl/rockchip/hwc/proxy/aidl/IRkHwcProxyAidl.h>
#include <aidl/rockchip/hwc/proxy/aidl/BnRkHwcProxyAidlCallback.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <stdlib.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

#include <log/log.h>
#include <DrmApi.h>
#include "utils/worker.h"

#include "rockchip/utils/drmdebug.h"

using ::aidl::rockchip::hwc::proxy::aidl::IRkHwcProxyAidl;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlRequest;
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyAidlResponse;
using ::aidl::rockchip::hwc::proxy::aidl::IRkHwcProxyAidlCallback;

using ::ndk::SpAIBinder;
using namespace std;

using ndk::SharedRefBase;
using ndk::ScopedAStatus;

namespace aidl::rockchip::hwc::proxy::aidl {

using namespace android;

class RkHwcProxyClient {
public:
	// callback define
	class RkHwcProxyAidlCB : public BnRkHwcProxyAidlCallback {
		public:
			RkHwcProxyClient& client_;
			DrmApi drmApi;
			int fd_ = 0;

			RkHwcProxyAidlCB(RkHwcProxyClient& client, int fd):client_(client), fd_(fd){};
			::ndk::ScopedAStatus asyncDone(const RkHwcProxyAidlResponse& response) override {
				client_.notify();
				HWC2_ALOGD_IF_DEBUG("Received %s", response.toString().c_str());
				return ScopedAStatus::ok();
			}

			::ndk::ScopedAStatus onCallback(const RkHwcProxyAidlRequest& request, RkHwcProxyAidlResponse* response) override {
				drmApi.drm_proc(fd_, request, response);
				return ScopedAStatus::ok();
			}
	};

	RkHwcProxyClient& operator = (const RkHwcProxyClient& other) {
		// define how to copy over data members from `other` to `this`
		return *this;
	}

	int Setup(int fd) {
		const std::string instance = std::string() + IRkHwcProxyAidl::descriptor + "/default";
		client = IRkHwcProxyAidl::fromBinder(
			SpAIBinder(AServiceManager_waitForService(instance.c_str())));

		if (client == nullptr) {
			HWC2_ALOGE("Failed to get service Demo");
			return -1;
		}

		// register callback
		cb = SharedRefBase::make<RkHwcProxyAidlCB>(*this, fd);
		client->registerCallback(cb);
		HWC2_ALOGI("registerCallback ok");
		return 0;
	}

	// call func via aidl
	void run(const RkHwcProxyAidlRequest& req, RkHwcProxyAidlResponse* resp) {
		client->run(req, resp);
	}

	// Used as a mechanism to inform the test about data/event callback.
	inline void notify() {
		std::unique_lock<std::mutex> lock(client_mtx);
		client_count++;
		client_cv.notify_one();
	}

	// Test code calls this function to wait for data/event callback.
	inline std::cv_status wait() {
		std::unique_lock<std::mutex> lock(client_mtx);
		std::cv_status status = std::cv_status::no_timeout;
		auto now = std::chrono::system_clock::now();
		while (client_count == 0) {
			status =
				client_cv.wait_until(lock, now + std::chrono::seconds(20));
			if (status == std::cv_status::timeout) {
				HWC2_ALOGE("wait callback timeout");
				return status;
			}
		}
		client_count--;
		return status;
	}

	// member
	std::shared_ptr<IRkHwcProxyAidl> client;
	std::shared_ptr<IRkHwcProxyAidlCallback> cb;
	std::mutex client_mtx;
	std::condition_variable client_cv;
	int client_count = 0;
};

} //namespace aidl::rockchip::hwc::proxy::aidl

namespace android {

using aidl::rockchip::hwc::proxy::aidl::RkHwcProxyClient;

class RkHwcProxyClientWorker : public Worker {
public:
	RkHwcProxyClientWorker();
	~RkHwcProxyClientWorker() override;

	int Init(int fd);

protected:
	void Routine() override;

private:
	RkHwcProxyClient mRkHwcProxyClient_;
	bool bRkHwcProxyClientWorkerInit = false;
};

}  // namespace android
