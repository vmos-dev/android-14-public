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

package com.android.systemui;

import static android.car.CarOccupantZoneManager.DISPLAY_TYPE_MAIN;

import android.annotation.Nullable;
import android.car.Car;
import android.car.Car.CarServiceLifecycleListener;
import android.car.CarOccupantZoneManager;
import android.car.CarOccupantZoneManager.OccupantZoneConfigChangeListener;
import android.car.CarOccupantZoneManager.OccupantZoneInfo;
import android.car.media.CarAudioManager;
import android.util.Log;
import android.view.Display;

import androidx.annotation.GuardedBy;

import com.android.systemui.car.users.CarSystemUIUserUtil;

import java.lang.System;

/**
 * Application class for CarSystemUI.
 */
public class CarSystemUIApplication extends SystemUIApplication {

    private boolean mIsSecondaryUserSystemUI;

    private CarOccupantZoneManager mCarOccupantZoneManager;

    private final Object mInfoLock = new Object();
    private final Object mCarAudioManagerLock = new Object();

    @GuardedBy("mInfoLock")
    private int mOccupantZoneDisplayId = Display.INVALID_DISPLAY;
    @GuardedBy("mInfoLock")
    private int mAudioZoneId = CarAudioManager.INVALID_AUDIO_ZONE;
    @GuardedBy("mInfoLock")
    private int mOccupantZoneType = CarOccupantZoneManager.OCCUPANT_TYPE_INVALID;
    @GuardedBy("mCarAudioManagerLock")
    private CarAudioManager mCarAudioManager = null;

    private boolean isInit = false;

    /**
     * Listener to monitor any Occupant Zone configuration change.
     */
    private final OccupantZoneConfigChangeListener mConfigChangeListener = flags -> {
        synchronized (mInfoLock) {
            updateZoneInfoLocked();
        }
    };

    /**
     * Listener to monitor the Lifecycle of car service.
     */
    private final CarServiceLifecycleListener mCarServiceLifecycleListener = (car, ready) -> {
        if (!ready) {
            mCarOccupantZoneManager = null;
            synchronized (mCarAudioManagerLock) {
                mCarAudioManager = null;
            }
            return;
        }
        mCarOccupantZoneManager =
                (CarOccupantZoneManager) car.getCarManager(Car.CAR_OCCUPANT_ZONE_SERVICE);
        if (mCarOccupantZoneManager != null) {
            mCarOccupantZoneManager.registerOccupantZoneConfigChangeListener(mConfigChangeListener);
        }
        synchronized (mCarAudioManagerLock) {
            mCarAudioManager = (CarAudioManager) car.getCarManager(Car.AUDIO_SERVICE);
        }
        synchronized (mInfoLock) {
            updateZoneInfoLocked();
        }
    };

    @Override
    public void onCreate() {
        mIsSecondaryUserSystemUI = CarSystemUIUserUtil.isSecondaryMUMDSystemUI();
        super.onCreate();
        if (mIsSecondaryUserSystemUI) {
            Car car = Car.createCar(this, null, Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER,
                    mCarServiceLifecycleListener);
        }
    }

    @Override
    void startSecondaryUserServicesIfNeeded() {
        if (mIsSecondaryUserSystemUI) {
            // Per-user services are not needed since this sysui process is running as the real user
            return;
        }
        super.startSecondaryUserServicesIfNeeded();
    }



    /**
     * Returns zone type assigned for the current user. The zone type is used to determine whether
     * the settings preferences should be available or not.
     */
    public final int getMyOccupantZoneType() {
        synchronized (mInfoLock) {
            return mOccupantZoneType;
        }
    }

    /**
     * Returns displayId assigned for the current user.
     */
    public final int getMyOccupantZoneDisplayId() {
        synchronized (mInfoLock) {
            return mOccupantZoneDisplayId;
        }
    }

    /**
     * Returns audio zone id assigned for the current user.
     */
    public final int getMyAudioZoneId() {
        synchronized (mInfoLock) {
            return mAudioZoneId;
        }
    }

    /**
     * Returns CarAudioManager instance.
     */
    @Nullable
    public final CarAudioManager getCarAudioManager() {
        synchronized (mCarAudioManagerLock) {
            return mCarAudioManager;
        }
    }

    @GuardedBy("mInfoLock")
    private void updateZoneInfoLocked() {
        if (mCarOccupantZoneManager == null) {
            return;
        }
        OccupantZoneInfo info = mCarOccupantZoneManager.getMyOccupantZone();
        if (info != null) {
            mOccupantZoneType = info.occupantType;
            Display display =
                    mCarOccupantZoneManager.getDisplayForOccupant(info, DISPLAY_TYPE_MAIN);
            Log.w(TAG, "info = " + info + ", display = " + display + ", mOccupantZoneDisplayId = " + mOccupantZoneDisplayId);
            if (display != null) {
                if (mOccupantZoneDisplayId == Display.INVALID_DISPLAY && !isInit) {
                    Log.w(TAG, "Occupant zone display init or not changed");
                    mOccupantZoneDisplayId = display.getDisplayId();
                    updateDisplay(mOccupantZoneDisplayId);
                    startServicesIfNeeded();
                    mAudioZoneId = mCarOccupantZoneManager.getAudioZoneIdForOccupant(info);
                } else if (mOccupantZoneDisplayId != display.getDisplayId()) {
                    Log.w(TAG, "Occupant zone display changed, restarting services");
                    System.exit(0);
                } else {
                    Log.w(TAG, "Occupant zone display not do anything");
                }
            } else {
                isInit = true;
            }
        }
    }
}
