/*
 * Copyright (C) 2022 The Android Open Source Project
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

package com.android.tv.settings.display;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.SystemProperties;
import android.util.Log;

import androidx.annotation.NonNull;

import java.util.List;

/** The {@BroadcastReceiver} for performing actions upon device boot. */
public class HotPlugsReceiver extends BroadcastReceiver {

    private static final String TAG = "HotPlugsReceiver";
    public static final String HDMI_PLUG_ACTION = "android.intent.action.HDMI_PLUGGED";
    public static final String DP_PLUG_ACTION = "android.intent.action.DP_PLUGGED";

    public static final String EXTRA_DP_PLUGGED_STATE = "state";
    public static final String EXTRA_MULTI_DP_PLUGGED_NAME = "extcon_name";

    private static final String PROP_VIVID_SYS_HDR_CAPACITY = "persist.sys.vivid.hdr_capacity";

    private static final int SDR = 0x01;
    private static final int HDR10 = 0x02;
    private static final int HLG = 0x04;
    private static final int VIVID = 0x08;

    public static final int MESSAGE_HDMI = 0;

    private int mAttemptsCount = 0;

    private Handler mHandler = new Handler(Looper.getMainLooper()) {
        @Override
        public void handleMessage(@NonNull Message msg) {
            super.handleMessage(msg);
            switch(msg.what) {
                case MESSAGE_HDMI:
                    DisplayInfo mHDMIDisplayInfo = getHDMIDisplayInfo();
                    if (mHDMIDisplayInfo == null) {
                        if (mAttemptsCount < 3) {
                            mHandler.removeCallbacksAndMessages(null);
                            mHandler.sendMessageDelayed(mHandler.obtainMessage(MESSAGE_HDMI), 1000);
                            mAttemptsCount++;
                        } else {
                            mAttemptsCount = 0;
                            // SystemProperties.set(PROP_VIVID_SYS_HDR_CAPACITY, "0");
                            DrmDisplaySetting.setHDRVividCapacity("0");
                        }
                        Log.i(TAG, "MESSAGE_HDMI: HDMI mAttemptsCount = " + mAttemptsCount);
                    } else {
                        String mSupport = getCurrentSupported(mHDMIDisplayInfo);
                        Log.d(TAG, "MESSAGE_HDMI: mSupport = " + mSupport);
                        // SystemProperties.set(PROP_VIVID_SYS_HDR_CAPACITY, mSupport);
                        DrmDisplaySetting.setHDRVividCapacity(mSupport);
                    }
                break;
            }
        }
    };

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent != null) {
            if (intent.getAction().equals(HDMI_PLUG_ACTION)) {
                boolean state = intent.getBooleanExtra(EXTRA_DP_PLUGGED_STATE, true);
                String extconName = intent.getStringExtra(EXTRA_MULTI_DP_PLUGGED_NAME);
                if (state) {
                    mAttemptsCount = 0;
                    mHandler.removeCallbacksAndMessages(null);
                    mHandler.sendMessage(mHandler.obtainMessage(MESSAGE_HDMI));
                } else {
                    // SystemProperties.set(PROP_VIVID_SYS_HDR_CAPACITY, "0");
                    DrmDisplaySetting.setHDRVividCapacity("0");
                }
                Log.d(TAG, "onReceive: " + intent.getAction() + " state: " + state + " extconName: " + extconName);
            } else if (intent.getAction().equals(DP_PLUG_ACTION)) {
                Log.d(TAG, "onReceive: " + intent.getAction());
            }
        } else {
            Log.i(TAG, "onReceiver intent is null");
        }
    }

    private String getCurrentSupported(DisplayInfo mDisplayInfo) {
        int mSupport = DrmDisplaySetting.getHdrResolutionSupport(mDisplayInfo.getDisplayId(), DrmDisplaySetting.getCurDisplayMode(mDisplayInfo));
        int result = SDR;
        if ((mSupport & DrmDisplaySetting.HDR10) > 0) {
            result |= HDR10;
        }
        if ((mSupport & DrmDisplaySetting.HLG) > 0) {
            result |= HLG;
        }
        return String.valueOf(result);
    }

    private DisplayInfo getHDMIDisplayInfo() {
        DisplayInfo mDisplayInfo = null;
        List<DisplayInfo> mDisplayInfos = DrmDisplaySetting.getDisplayInfoList();
        for(DisplayInfo tempDisplayInfo : mDisplayInfos) {
            if (tempDisplayInfo.getType() == 11) {
                mDisplayInfo = tempDisplayInfo;
                break;
            }
        }
        return mDisplayInfo;
    }
}
