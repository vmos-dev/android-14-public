/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.systemui.car.volume;

import static android.car.media.CarAudioManager.AUDIO_FEATURE_VOLUME_GROUP_EVENTS;
import static android.car.media.CarAudioManager.INVALID_AUDIO_ZONE;
import static android.car.media.CarVolumeGroupEvent.EXTRA_INFO_SHOW_UI;
import static android.car.media.CarVolumeGroupEvent.EXTRA_INFO_VOLUME_INDEX_CHANGED_BY_AUDIO_SYSTEM;

import android.car.Car;
import android.car.CarOccupantZoneManager;
import android.car.media.CarAudioManager;
import android.car.media.CarVolumeGroupEvent;
import android.car.media.CarVolumeGroupEventCallback;
import android.car.media.CarVolumeGroupInfo;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.media.AudioManager;
import android.os.Handler;
import android.os.HandlerExecutor;
import android.util.Log;

import com.android.systemui.CoreStartable;
import com.android.systemui.R;
import com.android.systemui.car.CarServiceProvider;
import com.android.systemui.dagger.SysUISingleton;
import com.android.systemui.dagger.qualifiers.Main;
import com.android.systemui.settings.UserTracker;
import com.android.systemui.volume.VolumeDialogComponent;

import dagger.Lazy;

import java.io.PrintWriter;
import java.util.List;
import java.util.concurrent.Executor;

import javax.inject.Inject;

/** The entry point for controlling the volume ui in cars. */
@SysUISingleton
public class VolumeUI implements CoreStartable {

    private static final String TAG = "VolumeUI";
    private final Resources mResources;
    private final Handler mMainHandler;
    private final CarServiceProvider mCarServiceProvider;
    private final Lazy<VolumeDialogComponent> mVolumeDialogComponentLazy;
    private final UserTracker mUserTracker;
    private int mAudioZoneId = INVALID_AUDIO_ZONE;

    private final CarAudioManager.CarVolumeCallback mVolumeChangeCallback =
            new CarAudioManager.CarVolumeCallback() {
                @Override
                public void onGroupVolumeChanged(int zoneId, int groupId, int flags) {
                    handleVolumeCallback(zoneId, flags);
                }

                @Override
                public void onMasterMuteChanged(int zoneId, int flags) {
                    handleVolumeCallback(zoneId, flags);
                }

                private void handleVolumeCallback(int zoneId, int flags) {
                    if (mAudioZoneId != zoneId || (flags & AudioManager.FLAG_SHOW_UI) == 0) {
                        // only initialize for current audio zone when show requested
                        return;
                    }
                    initVolumeDialogComponent();
                    unregistarCarAudioManagerCallbacks();
                }

            };

    private final CarVolumeGroupEventCallback mCarVolumeGroupEventCallback =
            new CarVolumeGroupEventCallback() {
                @Override
                public void onVolumeGroupEvent(List<CarVolumeGroupEvent> volumeGroupEvents) {
                    if (!hasEventsForZone(volumeGroupEvents)) {
                        return;
                    }
                    initVolumeDialogComponent();
                    unregistarCarAudioManagerCallbacks();
                }

                private boolean hasEventsForZone(List<CarVolumeGroupEvent> events) {
                    for (int index = 0; index < events.size(); index++) {
                        List<CarVolumeGroupInfo> infos = events.get(index).getCarVolumeGroupInfos();
                        if (!shouldShowUi(events.get(index).getExtraInfos())) {
                            continue;
                        }

                        for (int infoIndex = 0; infoIndex < infos.size(); infoIndex++) {
                            if (infos.get(infoIndex).getZoneId() == mAudioZoneId) {
                                // at least one event for this zone exists that needs to show UI
                                return true;
                            }
                        }
                    }
                    return false;
                }

                private boolean shouldShowUi(List<Integer> extraInfos) {
                    if (extraInfos.contains(EXTRA_INFO_SHOW_UI)
                            || extraInfos.contains(
                            EXTRA_INFO_VOLUME_INDEX_CHANGED_BY_AUDIO_SYSTEM)) {
                        return true;
                    }
                    return false;
                }
            };

    private boolean mEnabled;
    private Car mCar;
    private CarAudioManager mCarAudioManager;
    private CarOccupantZoneManager mCarOccupantZoneManager;
    private VolumeDialogComponent mVolumeDialogComponent;
    private final Executor mExecutor;

    @Inject
    public VolumeUI(
            @Main Resources resources,
            @Main Handler mainHandler,
            CarServiceProvider carServiceProvider,
            Lazy<VolumeDialogComponent> volumeDialogComponentLazy,
            UserTracker userTracker
    ) {
        mResources = resources;
        mMainHandler = mainHandler;
        mCarServiceProvider = carServiceProvider;
        mVolumeDialogComponentLazy = volumeDialogComponentLazy;
        mUserTracker = userTracker;
        mExecutor = new HandlerExecutor(mainHandler);
    }

    /**
     * Listener to monitor any Occupant Zone configuration change.
     */
    private CarOccupantZoneManager.OccupantZoneConfigChangeListener mConfigChangeListener =
        new CarOccupantZoneManager.OccupantZoneConfigChangeListener() {
            @Override
            public void onOccupantZoneConfigChanged(int changeFlags) {
                obtainAudioZone();
            }
        };

    @Override
    public void start() {
        boolean enableVolumeUi = mResources.getBoolean(R.bool.enable_volume_ui);
        mEnabled = enableVolumeUi;
        if (!mEnabled) return;

        mCarServiceProvider.addListener(car -> {
            mCar = car;
            if (mCarOccupantZoneManager != null) {
                return;
            }

            mCarOccupantZoneManager =
                    (CarOccupantZoneManager) mCar.getCarManager(Car.CAR_OCCUPANT_ZONE_SERVICE);
            mCarAudioManager = (CarAudioManager) mCar.getCarManager(Car.AUDIO_SERVICE);
            if (mCarOccupantZoneManager != null && mCarAudioManager != null) {
                mCarOccupantZoneManager.registerOccupantZoneConfigChangeListener(
                        mConfigChangeListener);
                obtainAudioZone();
            } else {
                Log.w(TAG, "CarOccupantZoneManager or CarAudioManager not found, disabling volume ui.");
            }
        });
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (!mEnabled) return;
        if (mVolumeDialogComponent != null) {
            mVolumeDialogComponent.onConfigurationChanged(newConfig);
        }
    }

    @Override
    public void dump(PrintWriter pw, String[] args) {
        pw.print("mEnabled="); pw.println(mEnabled);
        if (!mEnabled) return;
        if (mVolumeDialogComponent != null) {
            mVolumeDialogComponent.dump(pw, args);
        }
    }

    private void initVolumeDialogComponent() {
        if (mVolumeDialogComponent == null) {
            mMainHandler.post(() -> {
                mVolumeDialogComponent = mVolumeDialogComponentLazy.get();
                mVolumeDialogComponent.register();
            });
        }
    }

    private void unregistarCarAudioManagerCallbacks() {
        if (mCarAudioManager != null) {
            mCarAudioManager.unregisterCarVolumeCallback(mVolumeChangeCallback);
            if (mCarAudioManager.isAudioFeatureEnabled(AUDIO_FEATURE_VOLUME_GROUP_EVENTS)) {
                mCarAudioManager.unregisterCarVolumeGroupEventCallback(mCarVolumeGroupEventCallback);
            }
        }
    }

    private void obtainAudioZone() {
        if (mCarOccupantZoneManager == null) {
            return;
        }
        CarOccupantZoneManager.OccupantZoneInfo info =
                mCarOccupantZoneManager.getOccupantZoneForUser(mUserTracker.getUserHandle());
        if (info != null) {
            int newAudioZoneId = mCarOccupantZoneManager.getAudioZoneIdForOccupant(info);
            if (newAudioZoneId == INVALID_AUDIO_ZONE) {
                Log.i(TAG, "Now no AudioZone found for user, disabling volume ui and return!.");
                return;
            }
            if (newAudioZoneId != mAudioZoneId) {
                Log.i(TAG, "AudioZone(" + mAudioZoneId + ") changed to " + newAudioZoneId + ", re-registering callbacks.");
                mAudioZoneId = newAudioZoneId;
                unregistarCarAudioManagerCallbacks();
            } else {
                return;
            }
            if (mCarAudioManager.isAudioFeatureEnabled(AUDIO_FEATURE_VOLUME_GROUP_EVENTS)) {
                Log.d(TAG, "Registering mCarVolumeGroupEventCallback.");
                mCarAudioManager.registerCarVolumeGroupEventCallback(mExecutor,
                        mCarVolumeGroupEventCallback);
            } else {
                Log.d(TAG, "Registering mVolumeChangeCallback.");
                // This volume call back is never unregistered because CarStatusBar is
                // never destroyed.
                mCarAudioManager.registerCarVolumeCallback(mVolumeChangeCallback);
            }
        }
    }
}
