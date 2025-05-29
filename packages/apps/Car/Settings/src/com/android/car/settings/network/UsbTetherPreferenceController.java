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

package com.android.car.settings.network;

import android.car.drivingstate.CarUxRestrictions;
import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbManager;
import android.net.ConnectivityManager;
import android.net.TetheringManager;
import android.os.Handler;
import android.os.Environment;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.OnLifecycleEvent;
import androidx.preference.TwoStatePreference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiTwoActionSwitchPreference;

/**
 * This controller helps to manage the switch state and visibility of USB tether switch
 * preference.
 *
 */
public final class UsbTetherPreferenceController extends PreferenceController<TwoStatePreference> {

    private static final String TAG = "UsbTetherPrefController";

    private final TetheringManager mTetheringManager =
            getContext().getSystemService(TetheringManager.class);
    private final Handler mHandler = new Handler();

    private ConnectivityManager mCm = getContext().getSystemService(ConnectivityManager.class);
    private Context mContext;
    private boolean mUsbConnected = false;
    private boolean mMassStorageActive = false;
    private OnStartTetheringCallback mStartTetheringCallback;
    private String[] usbRegexs;

    public UsbTetherPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mContext = context;
        mStartTetheringCallback = new OnStartTetheringCallback();
        usbRegexs = mTetheringManager.getTetherableUsbRegexs();
    }

    @Override
    protected Class<TwoStatePreference> getPreferenceType() {
        return TwoStatePreference.class;
    }

    @Override
    protected void onCreateInternal() {
        Log.d(TAG, "onCreateInternal");
    }

    @Override
    protected void onStartInternal() {
        Log.d(TAG, "onStartInternal");
        mMassStorageActive = Environment.MEDIA_SHARED.equals(Environment.getExternalStorageState());
        IntentFilter filter = new IntentFilter(UsbManager.ACTION_USB_STATE);
        filter.addAction(Intent.ACTION_MEDIA_SHARED);
        filter.addAction(Intent.ACTION_MEDIA_UNSHARED);
        mContext.registerReceiver(mUsbChangeReceiver, filter);
        updateState(getPreference());
    }

    @Override
    protected void onStopInternal() {
        Log.d(TAG, "onStopInternal");
        mContext.unregisterReceiver(mUsbChangeReceiver);
    }

    @Override
    @AvailabilityStatus
    protected int getDefaultAvailabilityStatus() {
        return (usbRegexs != null && usbRegexs.length != 0 && !ActivityManager
                .isUserAMonkey()) ? AVAILABLE : UNSUPPORTED_ON_DEVICE;
    }

    @Override
    protected boolean handlePreferenceChanged(TwoStatePreference preference, Object newValue) {
        boolean isToggledOn = (boolean) newValue;
        Log.d(TAG, "preference = " + preference.toString() + ", newValue = " + isToggledOn);
        if (isToggledOn) {
            mCm.startTethering(TetheringManager.TETHERING_USB, true, mStartTetheringCallback, mHandler);
        } else {
            mCm.stopTethering(TetheringManager.TETHERING_USB);
        }
        return true;
    }

    final BroadcastReceiver mUsbChangeReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (TextUtils.equals(Intent.ACTION_MEDIA_SHARED, action)) {
                mMassStorageActive = true;
            } else if (TextUtils.equals(Intent.ACTION_MEDIA_UNSHARED, action)) {
                mMassStorageActive = false;
            } else if (TextUtils.equals(UsbManager.ACTION_USB_STATE, action)) {
                mUsbConnected = intent.getBooleanExtra(UsbManager.USB_CONNECTED, false);
            }
            mHandler.postDelayed(() -> {
                updateState(getPreference());
            }, 200);
        }
    };

    protected void updateState(TwoStatePreference preference) {
        super.updateState(preference);
        String[] tethered = mTetheringManager.getTetheredIfaces();
        boolean usbTethered = false;
        for (String s : tethered) {
            for (String regex : usbRegexs) {
                if (s.matches(regex))
                    usbTethered = true;
            }
        }
        Log.d(TAG, "updateState usbTethered = " + usbTethered);
        preference.setChecked(usbTethered);
    }

    private static final class OnStartTetheringCallback extends
            ConnectivityManager.OnStartTetheringCallback {

        OnStartTetheringCallback() {
        }

        @Override
        public void onTetheringStarted() {
            update();
        }

        @Override
        public void onTetheringFailed() {
            update();
        }

        private void update() {

        }
    }

}
