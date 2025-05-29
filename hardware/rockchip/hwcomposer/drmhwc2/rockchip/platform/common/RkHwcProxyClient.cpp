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

#include "rockchip/platform/RkHwcProxyClient.h"
#include <hardware/hardware.h>

using aidl::rockchip::hwc::proxy::aidl::RkHwcProxyClient;

namespace android {

RkHwcProxyClientWorker::RkHwcProxyClientWorker()
	: Worker("RkHwcProxyClientWorker", HAL_PRIORITY_URGENT_DISPLAY) {
}

RkHwcProxyClientWorker::~RkHwcProxyClientWorker() {
}

int RkHwcProxyClientWorker::Init(int fd) {
	// ABinderProcess_setThreadPoolMaxThreadCount(2);
	// default thread pool is 10
	ABinderProcess_startThreadPool();

	// Initial and set callback
	mRkHwcProxyClient_ = RkHwcProxyClient();
	int ret = mRkHwcProxyClient_.Setup(fd);
	if (ret < 0)
		HWC2_ALOGE("RkHwcProxyClient Setup failed");

	return InitWorker();
}

void RkHwcProxyClientWorker::Routine () {
	// Verify: only register callback, do not call function
	// waiting for the data from callback
	std::cv_status waitStatus = mRkHwcProxyClient_.wait();
	while (waitStatus == std::cv_status::no_timeout)
		waitStatus = mRkHwcProxyClient_.wait();
}

}  // namespace android
