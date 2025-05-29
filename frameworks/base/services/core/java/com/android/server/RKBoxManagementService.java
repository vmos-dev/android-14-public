/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd
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
package com.android.server;

import android.content.Context;
import android.os.IRKBoxManagementService;
import android.util.Log;

/**
 * @hide
 */
class RKBoxManagementService extends IRKBoxManagementService.Stub {
    private static final String TAG = "RKBoxManagementService";

    private static native int nativeInit();

    /**
     * Binder context for this service
     */
    private Context mContext;

    public RKBoxManagementService(Context context) {
        mContext = context;
        Log.d(TAG, "Init RKBox service!");
        nativeInit();
    }

}
