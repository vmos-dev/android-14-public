/*
 * Copyright (C) 2015 The Android Open Source Project
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
package android.car.media;

import static com.android.car.internal.ExcludeFromCodeCoverageGeneratedReport.BOILERPLATE_CODE;
import static com.android.car.internal.ExcludeFromCodeCoverageGeneratedReport.DEPRECATED_CODE;
import static com.android.car.internal.util.VersionUtils.assertPlatformVersionAtLeastU;

import android.annotation.CallbackExecutor;
import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.annotation.TestApi;
import android.car.Car;
import android.car.CarLibLog;
import android.car.CarManagerBase;
import android.car.CarOccupantZoneManager;
import android.car.CarOccupantZoneManager.OccupantZoneInfo;
import android.car.annotation.AddedInOrBefore;
import android.car.annotation.ApiRequirements;
import android.media.AudioAttributes;
import android.media.AudioDeviceAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.os.Binder;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;
import static android.media.AudioAttributes.USAGE_MEDIA;

import com.android.car.internal.ExcludeFromCodeCoverageGeneratedReport;
import com.android.car.internal.annotation.AttributeUsage;
import com.android.internal.annotations.GuardedBy;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.Executor;

/**
 * APIs for handling audio in a car.
 *
 * In a car environment, we introduced the support to turn audio dynamic routing on /off by
 * setting the "audioUseDynamicRouting" attribute in config.xml
 *
 * When audio dynamic routing is enabled:
 * - Audio devices are grouped into zones
 * - There is at least one primary zone, and extra secondary zones such as RSE
 *   (Rear Seat Entertainment)
 * - Within each zone, audio devices are grouped into volume groups for volume control
 * - Audio is assigned to an audio device based on its AudioAttributes usage
 *
 * When audio dynamic routing is disabled:
 * - There is exactly one audio zone, which is the primary zone
 * - Each volume group represents a controllable STREAM_TYPE, same as AudioManager
 */
public final class CarAudioManager extends CarManagerBase {

    private static final String TAG = CarAudioManager.class.getSimpleName();

    /**
     * Zone id of the primary audio zone.
     * @hide
     */
    @SystemApi
    @AddedInOrBefore(majorVersion = 33)
    public static final int PRIMARY_AUDIO_ZONE = 0x0;

    /**
     * Zone id of the invalid audio zone.
     * @hide
     */
    @SystemApi
    @AddedInOrBefore(majorVersion = 33)
    public static final int INVALID_AUDIO_ZONE = 0xffffffff;

    /**
     * This is used to determine if dynamic routing is enabled via
     * {@link #isAudioFeatureEnabled(int)}
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final int AUDIO_FEATURE_DYNAMIC_ROUTING = 1;

    /**
     * This is used to determine if volume group muting is enabled via
     * {@link #isAudioFeatureEnabled(int)}
     *
     * <p>
     * If enabled, car volume group muting APIs can be used to mute each volume group,
     * also car volume group muting changed callback will be called upon group mute changes. If
     * disabled, car volume will toggle master mute instead.
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final int AUDIO_FEATURE_VOLUME_GROUP_MUTING = 2;

    /**
     * This is used to determine if the OEM audio service is enabled via
     * {@link #isAudioFeatureEnabled(int)}
     *
     * <p>If enabled, car audio focus, car audio volume, and ducking control behaviour can change
     * as it can be OEM dependent.
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.TIRAMISU_0)
    public static final int AUDIO_FEATURE_OEM_AUDIO_SERVICE = 3;

    /**
     * This is used to determine if volume group events is supported via
     * {@link #isAudioFeatureEnabled(int)}
     *
     * <p>If enabled, the car volume group event callback can be used to receive event changes
     * to volume, mute, attenuation.
     * If disabled, the register/unregister APIs will return {@code false}.
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    public static final int AUDIO_FEATURE_VOLUME_GROUP_EVENTS = 4;

    /**
     * This is used to determine if audio mirroring is supported via
     * {@link #isAudioFeatureEnabled(int)}
     *
     * <p>If enabled, audio mirroring can be managed by using the following APIs:
     * {@code setAudioZoneMirrorStatusCallback(Executor, AudioZonesMirrorStatusCallback)},
     * {@code clearAudioZonesMirrorStatusCallback()}, {@code canEnableAudioMirror()},
     * {@code enableMirrorForAudioZones(List)}, {@code extendAudioMirrorRequest(long, List)},
     * {@code disableAudioMirrorForZone(int)}, {@code disableAudioMirror(long)},
     * {@code getMirrorAudioZonesForAudioZone(int)},
     * {@code getMirrorAudioZonesForMirrorRequest(long)}
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    public static final int AUDIO_FEATURE_AUDIO_MIRRORING = 5;

    /** @hide */
    @IntDef(flag = false, prefix = "AUDIO_FEATURE", value = {
            AUDIO_FEATURE_DYNAMIC_ROUTING,
            AUDIO_FEATURE_VOLUME_GROUP_MUTING,
            AUDIO_FEATURE_OEM_AUDIO_SERVICE,
            AUDIO_FEATURE_VOLUME_GROUP_EVENTS,
            AUDIO_FEATURE_AUDIO_MIRRORING
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CarAudioFeature {}

    /**
     * Volume Group ID when volume group not found.
     * @hide
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final int INVALID_VOLUME_GROUP_ID = -1;

    /**
     * Use to identify if the request from {@link #requestMediaAudioOnPrimaryZone} is invalid
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.TIRAMISU_0)
    public static final long INVALID_REQUEST_ID = -1;

    /**
     * Extra for {@link android.media.AudioAttributes.Builder#addBundle(Bundle)}: when used in an
     * {@link android.media.AudioFocusRequest}, the requester should receive all audio focus events,
     * including {@link android.media.AudioManager#AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK}.
     * The requester must hold {@link Car#PERMISSION_RECEIVE_CAR_AUDIO_DUCKING_EVENTS}; otherwise,
     * this extra is ignored.
     *
     * @hide
     */
    @SystemApi
    @AddedInOrBefore(majorVersion = 33)
    public static final String AUDIOFOCUS_EXTRA_RECEIVE_DUCKING_EVENTS =
            "android.car.media.AUDIOFOCUS_EXTRA_RECEIVE_DUCKING_EVENTS";

    /**
     * Extra for {@link android.media.AudioAttributes.Builder#addBundle(Bundle)}: when used in an
     * {@link android.media.AudioFocusRequest}, the requester should receive all audio focus for the
     * the zone. If the zone id is not defined: the audio focus request will default to the
     * currently mapped zone for the requesting uid or {@link CarAudioManager.PRIMARY_AUDIO_ZONE}
     * if no uid mapping currently exist.
     *
     * @hide
     */
    @AddedInOrBefore(majorVersion = 33)
    public static final String AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID =
            "android.car.media.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID";

    /**
     * Use to inform media request callbacks about approval of a media request
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_REQUEST_STATUS_APPROVED = 1;

    /**
     * Use to inform media request callbacks about rejection of a media request
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_REQUEST_STATUS_REJECTED = 2;

    /**
     * Use to inform media request callbacks about cancellation of a pending request
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_REQUEST_STATUS_CANCELLED = 3;

    /**
     * Use to inform media request callbacks about the stop of a media request
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_REQUEST_STATUS_STOPPED = 4;

    /** @hide */
    @IntDef(flag = false, prefix = "AUDIO_REQUEST_STATUS", value = {
            AUDIO_REQUEST_STATUS_APPROVED,
            AUDIO_REQUEST_STATUS_REJECTED,
            AUDIO_REQUEST_STATUS_CANCELLED,
            AUDIO_REQUEST_STATUS_STOPPED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaAudioRequestStatus {}

    /**
     * This will be returned by {@link #canEnableAudioMirror()} in case there is an error when
     * calling the car audio service
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_MIRROR_INTERNAL_ERROR = -1;

    /**
     * This will be returned by {@link #canEnableAudioMirror()} and determines that it is possible
     * to enable audio mirroring using the {@link #enableMirrorForAudioZones(List)}
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_MIRROR_CAN_ENABLE = 1;

    /**
     * This will be returned by {@link #canEnableAudioMirror()} and determines that it is not
     * possible to enable audio mirroring using the {@link #enableMirrorForAudioZones(List)}.
     * This informs that there are no more audio mirror output devices available to route audio.
     *
     * @hide
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @SystemApi
    public static final int AUDIO_MIRROR_OUT_OF_OUTPUT_DEVICES = 2;

    /** @hide */
    @IntDef(flag = false, prefix = "AUDIO_MIRROR_", value = {
            AUDIO_MIRROR_INTERNAL_ERROR,
            AUDIO_MIRROR_CAN_ENABLE,
            AUDIO_MIRROR_OUT_OF_OUTPUT_DEVICES,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AudioMirrorStatus {}

    private final ICarAudio mService;
    private final CopyOnWriteArrayList<CarVolumeCallback> mCarVolumeCallbacks;
    private final CopyOnWriteArrayList<CarVolumeGroupEventCallbackWrapper>
            mCarVolumeEventCallbacks = new CopyOnWriteArrayList<>();
    private final AudioManager mAudioManager;

    private final EventHandler mEventHandler;

    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private PrimaryZoneMediaAudioRequestCallback mPrimaryZoneMediaAudioRequestCallback;
    @GuardedBy("mLock")
    private Executor mPrimaryZoneMediaAudioRequestCallbackExecutor;

    @GuardedBy("mLock")
    private AudioZonesMirrorStatusCallbackWrapper mAudioZonesMirrorStatusCallbackWrapper;

    private final ConcurrentHashMap<Long, MediaAudioRequestStatusCallbackWrapper>
            mRequestIdToMediaAudioRequestStatusCallbacks = new ConcurrentHashMap<>();

    private final IPrimaryZoneMediaAudioRequestCallback mIPrimaryZoneMediaAudioRequestCallback =
            new IPrimaryZoneMediaAudioRequestCallback.Stub() {
                @Override
                public void onRequestMediaOnPrimaryZone(OccupantZoneInfo info,
                        long requestId) {
                    runOnExecutor((callback) ->
                            callback.onRequestMediaOnPrimaryZone(info, requestId));
                }

                @Override
                public void onMediaAudioRequestStatusChanged(
                        @NonNull CarOccupantZoneManager.OccupantZoneInfo info,
                        long requestId, int status) throws RemoteException {
                    runOnExecutor((callback) ->
                            callback.onMediaAudioRequestStatusChanged(info, requestId, status));
                }

                private void runOnExecutor(PrimaryZoneMediaAudioRequestCallbackRunner runner) {
                    PrimaryZoneMediaAudioRequestCallback callback;
                    Executor executor;
                    synchronized (mLock) {
                        if (mPrimaryZoneMediaAudioRequestCallbackExecutor == null
                                || mPrimaryZoneMediaAudioRequestCallback == null) {
                            Log.w(TAG, "Media request removed before change dispatched");
                            return;
                        }
                        callback = mPrimaryZoneMediaAudioRequestCallback;
                        executor = mPrimaryZoneMediaAudioRequestCallbackExecutor;
                    }

                    long identity = Binder.clearCallingIdentity();
                    try {
                        executor.execute(() -> runner.runOnCallback(callback));
                    } finally {
                        Binder.restoreCallingIdentity(identity);
                    }
                }
            };

    private interface PrimaryZoneMediaAudioRequestCallbackRunner {
        void runOnCallback(PrimaryZoneMediaAudioRequestCallback callback);
    }

    private final ICarVolumeCallback mCarVolumeCallbackImpl =
            new android.car.media.ICarVolumeCallback.Stub() {
        @Override
        public void onGroupVolumeChanged(int zoneId, int groupId, int flags) {
            mEventHandler.dispatchOnGroupVolumeChanged(zoneId, groupId, flags);
        }

        @Override
        public void onGroupMuteChanged(int zoneId, int groupId, int flags) {
            mEventHandler.dispatchOnGroupMuteChanged(zoneId, groupId, flags);
        }

        @Override
        public void onMasterMuteChanged(int zoneId, int flags) {
            mEventHandler.dispatchOnMasterMuteChanged(zoneId, flags);
        }
    };

    private final ICarVolumeEventCallback mCarVolumeEventCallbackImpl =
            new android.car.media.ICarVolumeEventCallback.Stub() {
        @Override
        public void onVolumeGroupEvent(@NonNull List<CarVolumeGroupEvent> events) {
            mEventHandler.dispatchOnVolumeGroupEvent(events);
        }

        @Override
        public void onMasterMuteChanged(int zoneId, int flags) {
            mEventHandler.dispatchOnMasterMuteChanged(zoneId, flags);
        }
    };

    /**
     * @return Whether dynamic routing is enabled or not.
     *
     * @deprecated use {@link #isAudioFeatureEnabled(int AUDIO_FEATURE_DYNAMIC_ROUTING)} instead.
     *
     * @hide
     */
    @TestApi
    @Deprecated
    @ExcludeFromCodeCoverageGeneratedReport(reason = DEPRECATED_CODE)
    @AddedInOrBefore(majorVersion = 33)
    public boolean isDynamicRoutingEnabled() {
        return isAudioFeatureEnabled(AUDIO_FEATURE_DYNAMIC_ROUTING);
    }

    /**
     * Determines if an audio feature is enabled.
     *
     * @param audioFeature audio feature to query, can be {@link #AUDIO_FEATURE_DYNAMIC_ROUTING},
     *                     {@link #AUDIO_FEATURE_VOLUME_GROUP_MUTING} or
     *                     {@link #AUDIO_FEATURE_VOLUME_GROUP_EVENTS}
     * @return Returns {@code true} if the feature is enabled, {@code false} otherwise.
     */
    @AddedInOrBefore(majorVersion = 33)
    public boolean isAudioFeatureEnabled(@CarAudioFeature int audioFeature) {
        try {
            return mService.isAudioFeatureEnabled(audioFeature);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Sets the volume index for a volume group in primary zone.
     *
     * @see {@link #setGroupVolume(int, int, int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public void setGroupVolume(int groupId, int index, int flags) {
        setGroupVolume(PRIMARY_AUDIO_ZONE, groupId, index, flags);
    }

private void syncSystemVolumeToCarVolume(int zoneId, int volumeGroupId, int carVolume) {
        int mediaGroupId = 0;
        try {
            mediaGroupId = mService.getVolumeGroupIdForUsage(zoneId, USAGE_MEDIA);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
        if (mediaGroupId != volumeGroupId) {
            return;
        }

        int streamType = AudioManager.STREAM_MUSIC;
        int streamVolume = mAudioManager.getStreamVolume(streamType);
        int minStreamVolume = mAudioManager.getStreamMinVolume(streamType);
        int maxStreamVolume = mAudioManager.getStreamMaxVolume(streamType);
        int minCarVolume = getGroupMinVolume(zoneId, volumeGroupId);
        int maxCarVolume = getGroupMaxVolume(zoneId, volumeGroupId);
        int transStreamVolume = minStreamVolume + (int)((float)(carVolume  - minCarVolume) / (float)(maxCarVolume - minCarVolume) * (maxStreamVolume - minStreamVolume));
        if (Math.abs(transStreamVolume - streamVolume) > 1) {
            mAudioManager.setStreamVolume(streamType, transStreamVolume, 0);
        }
    }

    /**
     * Sets the volume index for a volume group.
     *
     * @param zoneId The zone id whose volume group is affected.
     * @param groupId The volume group id whose volume index should be set.
     * @param index The volume index to set. See
     *            {@link #getGroupMaxVolume(int, int)} for the largest valid value.
     * @param flags One or more flags (e.g., {@link android.media.AudioManager#FLAG_SHOW_UI},
     *              {@link android.media.AudioManager#FLAG_PLAY_SOUND})
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public void setGroupVolume(int zoneId, int groupId, int index, int flags) {
        try {
            syncSystemVolumeToCarVolume(zoneId, groupId, index);
            mService.setGroupVolume(zoneId, groupId, index, flags);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Returns the maximum volume index for a volume group in primary zone.
     *
     * @see {@link #getGroupMaxVolume(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getGroupMaxVolume(int groupId) {
        return getGroupMaxVolume(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Returns the maximum volume index for a volume group.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose maximum volume index is returned.
     * @return The maximum valid volume index for the given group.
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getGroupMaxVolume(int zoneId, int groupId) {
        try {
            return mService.getGroupMaxVolume(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Returns the minimum volume index for a volume group in primary zone.
     *
     * @see {@link #getGroupMinVolume(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getGroupMinVolume(int groupId) {
        return getGroupMinVolume(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Returns the minimum volume index for a volume group.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose minimum volume index is returned.
     * @return The minimum valid volume index for the given group, non-negative
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getGroupMinVolume(int zoneId, int groupId) {
        try {
            return mService.getGroupMinVolume(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Returns the current volume index for a volume group in primary zone.
     *
     * @see {@link #getGroupVolume(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getGroupVolume(int groupId) {
        return getGroupVolume(PRIMARY_AUDIO_ZONE, groupId);
    }

    private void syncCarVolumeToSystemVolume(int zoneId, int volumeGroupId) {

        try {
            int mediaGroupId = mService.getVolumeGroupIdForUsage(zoneId, USAGE_MEDIA);
            if (mediaGroupId != volumeGroupId) {
                return;
            }

            int streamType = AudioManager.STREAM_MUSIC;
            int currentStreamVolume = mAudioManager.getStreamVolume(streamType);
            int minStreamVolume = mAudioManager.getStreamMinVolume(streamType);
            int maxStreamVolume = mAudioManager.getStreamMaxVolume(streamType);
            int currentCarVolume = mService.getGroupVolume(zoneId, volumeGroupId);
            int minCarVolume = getGroupMinVolume(zoneId, volumeGroupId);
            int maxCarVolume = getGroupMaxVolume(zoneId, volumeGroupId);
            int transCarVolume = minCarVolume + (int)((float)(currentStreamVolume  - minStreamVolume) / (float)(maxStreamVolume - minStreamVolume) * (maxCarVolume - minCarVolume));
            if (Math.abs(transCarVolume - currentCarVolume) > 1) {
                mService.setGroupVolume(zoneId, volumeGroupId, transCarVolume, 0);
            }
        }  catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Returns the current volume index for a volume group.
     *
     * @param zoneId The zone id whose volume groups is queried.
     * @param groupId The volume group id whose volume index is returned.
     * @return The current volume index for the given group.
     *
     * @see #getGroupMaxVolume(int, int)
     * @see #setGroupVolume(int, int, int, int)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getGroupVolume(int zoneId, int groupId) {
        try {
            syncCarVolumeToSystemVolume(zoneId, groupId);
            return mService.getGroupVolume(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Adjust the relative volume in the front vs back of the vehicle cabin.
     *
     * @param value in the range -1.0 to 1.0 for fully toward the back through
     *              fully toward the front.  0.0 means evenly balanced.
     *
     * @throws IllegalArgumentException if {@code value} is less than -1.0 or
     *                                  greater than 1.0
     * @see #setBalanceTowardRight(float)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public void setFadeTowardFront(float value) {
        try {
            mService.setFadeTowardFront(value);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Adjust the relative volume on the left vs right side of the vehicle cabin.
     *
     * @param value in the range -1.0 to 1.0 for fully toward the left through
     *              fully toward the right.  0.0 means evenly balanced.
     *
     * @throws IllegalArgumentException if {@code value} is less than -1.0 or
     *                                  greater than 1.0
     * @see #setFadeTowardFront(float)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public void setBalanceTowardRight(float value) {
        try {
            mService.setBalanceTowardRight(value);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Queries the system configuration in order to report the available, non-microphone audio
     * input devices.
     *
     * @return An array of strings representing the available input ports.
     * Each port is identified by it's "address" tag in the audioPolicyConfiguration xml file.
     * Empty array if we find nothing.
     *
     * @see #createAudioPatch(String, int, int)
     * @see #releaseAudioPatch(CarAudioPatchHandle)
     *
     * @deprecated use {@link AudioManager#getDevices(int)} with
     * {@link AudioManager#GET_DEVICES_INPUTS} instead
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @Deprecated
    @ExcludeFromCodeCoverageGeneratedReport(reason = DEPRECATED_CODE)
    @AddedInOrBefore(majorVersion = 33)
    public @NonNull String[] getExternalSources() {
        try {
            return mService.getExternalSources();
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
            return new String[0];
        }
    }

    /**
     * Given an input port identified by getExternalSources(), request that it's audio signal
     * be routed below the HAL to the output port associated with the given usage.  For example,
     * The output of a tuner might be routed directly to the output buss associated with
     * AudioAttributes.USAGE_MEDIA while the tuner is playing.
     *
     * @param sourceAddress the input port name obtained from getExternalSources().
     * @param usage the type of audio represented by this source (usually USAGE_MEDIA).
     * @param gainInMillibels How many steps above the minimum value defined for the source port to
     *                       set the gain when creating the patch.
     *                       This may be used for source balancing without affecting the user
     *                       controlled volumes applied to the destination ports.  A value of
     *                       0 indicates no gain change is requested.
     * @return A handle for the created patch which can be used to later remove it.
     *
     * @see #getExternalSources()
     * @see #releaseAudioPatch(CarAudioPatchHandle)
     *
     * @deprecated use {@link android.media.HwAudioSource} instead
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @Deprecated
    @ExcludeFromCodeCoverageGeneratedReport(reason = DEPRECATED_CODE)
    @AddedInOrBefore(majorVersion = 33)
    public CarAudioPatchHandle createAudioPatch(String sourceAddress, @AttributeUsage int usage,
            int gainInMillibels) {
        try {
            return mService.createAudioPatch(sourceAddress, usage, gainInMillibels);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Removes the association between an input port and an output port identified by the provided
     * handle.
     *
     * @param patch CarAudioPatchHandle returned from createAudioPatch().
     *
     * @see #getExternalSources()
     * @see #createAudioPatch(String, int, int)
     *
     * @deprecated use {@link android.media.HwAudioSource} instead
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @Deprecated
    @ExcludeFromCodeCoverageGeneratedReport(reason = DEPRECATED_CODE)
    @AddedInOrBefore(majorVersion = 33)
    public void releaseAudioPatch(CarAudioPatchHandle patch) {
        try {
            mService.releaseAudioPatch(patch);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Gets the count of available volume groups in primary zone.
     *
     * @see {@link #getVolumeGroupCount(int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getVolumeGroupCount() {
        return getVolumeGroupCount(PRIMARY_AUDIO_ZONE);
    }

    /**
     * Gets the count of available volume groups in the system.
     *
     * @param zoneId The zone id whois count of volume groups is queried.
     * @return Count of volume groups
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getVolumeGroupCount(int zoneId) {
        try {
            return mService.getVolumeGroupCount(zoneId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Gets the volume group id for a given {@link AudioAttributes} usage in primary zone.
     *
     * @see {@link #getVolumeGroupIdForUsage(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getVolumeGroupIdForUsage(@AttributeUsage int usage) {
        return getVolumeGroupIdForUsage(PRIMARY_AUDIO_ZONE, usage);
    }

    /**
     * Gets the volume group id for a given {@link AudioAttributes} usage.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param usage The {@link AudioAttributes} usage to get a volume group from.
     * @return The volume group id where the usage belongs to
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public int getVolumeGroupIdForUsage(int zoneId, @AttributeUsage int usage) {
        try {
            return mService.getVolumeGroupIdForUsage(zoneId, usage);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Gets array of {@link AudioAttributes} usages for a volume group in primary zone.
     *
     * @see {@link #getUsagesForVolumeGroupId(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public @NonNull int[] getUsagesForVolumeGroupId(int groupId) {
        return getUsagesForVolumeGroupId(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Returns the volume group info associated with the zone id and group id.
     *
     * <p>The volume information, including mute, blocked, limited state will reflect the state
     * of the volume group at the time of query.
     *
     * @param zoneId zone id for the group to query
     * @param groupId group id for the group to query
     * @throws IllegalArgumentException if the audio zone or group id are invalid
     *
     * @return the current volume group info
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.TIRAMISU_3,
            minPlatformVersion = ApiRequirements.PlatformVersion.TIRAMISU_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @Nullable
    public CarVolumeGroupInfo getVolumeGroupInfo(int zoneId, int groupId) {
        try {
            return mService.getVolumeGroupInfo(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Returns a list of volume group info associated with the zone id.
     *
     * <p>The volume information, including mute, blocked, limited state will reflect the state
     * of the volume group at the time of query.
     *
     * @param zoneId zone id for the group to query
     * @throws IllegalArgumentException if the audio zone is invalid
     *
     * @return all the current volume group info's for the zone id
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.TIRAMISU_3,
            minPlatformVersion = ApiRequirements.PlatformVersion.TIRAMISU_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @NonNull
    public List<CarVolumeGroupInfo> getVolumeGroupInfosForZone(int zoneId) {
        try {
            return mService.getVolumeGroupInfosForZone(zoneId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Returns a list of audio attributes associated with the volume group info.
     *
     * @param groupInfo group info to query
     * @throws NullPointerException if the volume group info is {@code null}
     *
     * @return list of audio attributes associated with the volume group info
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.TIRAMISU_3,
            minPlatformVersion = ApiRequirements.PlatformVersion.TIRAMISU_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @NonNull
    public List<AudioAttributes> getAudioAttributesForVolumeGroup(
            @NonNull CarVolumeGroupInfo groupInfo) {
        try {
            return mService.getAudioAttributesForVolumeGroup(groupInfo);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Gets array of {@link AudioAttributes} usages for a volume group in a zone.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose associated audio usages is returned.
     * @return Array of {@link AudioAttributes} usages for a given volume group id
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public @NonNull int[] getUsagesForVolumeGroupId(int zoneId, int groupId) {
        try {
            return mService.getUsagesForVolumeGroupId(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, new int[0]);
        }
    }

    /**
     * Determines if a particular volume group has any audio playback in a zone
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose associated audio usages is returned.
     * @return {@code true} if the group has active playback, {@code false} otherwise
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public boolean isPlaybackOnVolumeGroupActive(int zoneId, int groupId) {
        try {
            return mService.isPlaybackOnVolumeGroupActive(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns the current car audio zone configuration info associated with the zone id
     *
     * <p>If the car audio configuration does not include zone configurations, a default
     * configuration consisting current output devices for the zone is returned.
     *
     * @param zoneId Zone id for the configuration to query
     * @return the current car audio zone configuration info, or {@code null} if
     *         {@link CarAudioService} throws {@link RemoteException}
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalArgumentException if the audio zone id is invalid
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @Nullable
    public CarAudioZoneConfigInfo getCurrentAudioZoneConfigInfo(int zoneId) {
        assertPlatformVersionAtLeastU();
        try {
            return mService.getCurrentAudioZoneConfigInfo(zoneId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Returns a list of car audio zone configuration info associated with the zone id
     *
     * <p>If the car audio configuration does not include zone configurations, a default
     * configuration consisting current output devices for each zone is returned.
     *
     * <p>There exists exactly one zone configuration in primary zone.
     *
     * @param zoneId Zone id for the configuration to query
     * @return all the car audio zone configuration info for the zone id
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalArgumentException if the audio zone id is invalid
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @NonNull
    public List<CarAudioZoneConfigInfo> getAudioZoneConfigInfos(int zoneId) {
        assertPlatformVersionAtLeastU();
        try {
            return mService.getAudioZoneConfigInfos(zoneId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Switches the car audio zone configuration
     *
     * <p>To receive the volume group change after configuration is changed, a
     * {@code CarVolumeGroupEventCallback} must be registered through
     * {@link #registerCarVolumeGroupEventCallback(Executor, CarVolumeGroupEventCallback)} first.
     *
     * @param zoneConfig Audio zone configuration to switch to
     * @param executor Executor on which callback will be invoked
     * @param callback Callback that will report the result of switching car audio zone
     *                 configuration
     * @throws NullPointerException if either executor or callback are {@code null}
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if no user is assigned to the audio zone
     * @throws IllegalStateException if the audio zone is currently in a mirroring configuration
     *                               or sharing audio with primary audio zone
     * @throws IllegalArgumentException if the audio zone configuration is invalid
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void switchAudioZoneToConfig(@NonNull CarAudioZoneConfigInfo zoneConfig,
            @NonNull @CallbackExecutor Executor executor,
            @NonNull SwitchAudioZoneConfigCallback callback) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(zoneConfig, "Audio zone configuration can not be null");
        Objects.requireNonNull(executor, "Executor can not be null");
        Objects.requireNonNull(callback,
                "Switching audio zone configuration result callback can not be null");
        SwitchAudioZoneConfigCallbackWrapper wrapper =
                new SwitchAudioZoneConfigCallbackWrapper(executor, callback);
        try {
            mService.switchZoneToConfig(zoneConfig, wrapper);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Gets the audio zones currently available
     *
     * @return audio zone ids
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @AddedInOrBefore(majorVersion = 33)
    public @NonNull List<Integer> getAudioZoneIds() {
        try {
            return asList(mService.getAudioZoneIds());
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.emptyList());
        }
    }

    /**
     * Gets the audio zone id currently mapped to uId,
     * defaults to PRIMARY_AUDIO_ZONE if no mapping exist
     *
     * @param uid The uid to map
     * @return zone id mapped to uid
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @AddedInOrBefore(majorVersion = 33)
    public int getZoneIdForUid(int uid) {
        try {
            return mService.getZoneIdForUid(uid);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Maps the audio zone id to uid
     *
     * @param zoneId The audio zone id
     * @param uid The uid to map
     * @return true if the uid is successfully mapped
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @AddedInOrBefore(majorVersion = 33)
    public boolean setZoneIdForUid(int zoneId, int uid) {
        try {
            return mService.setZoneIdForUid(zoneId, uid);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Clears the current zone mapping of the uid
     *
     * @param uid The uid to clear
     * @return true if the zone was successfully cleared
     *
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @AddedInOrBefore(majorVersion = 33)
    public boolean clearZoneIdForUid(int uid) {
        try {
            return mService.clearZoneIdForUid(uid);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Sets a {@code PrimaryZoneMediaAudioRequestStatusCallback} to listen for request to play
     * media audio in primary audio zone
     *
     * @param executor Executor on which callback will be invoked
     * @param callback Media audio request callback to monitor for audio requests
     * @return {@code true} if the callback is successfully registered, {@code false} otherwise
     * @throws NullPointerException if either executor or callback are {@code null}
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if there is a callback already set
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean setPrimaryZoneMediaAudioRequestCallback(
            @NonNull @CallbackExecutor Executor executor,
            @NonNull PrimaryZoneMediaAudioRequestCallback callback) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(executor, "Executor can not be null");
        Objects.requireNonNull(callback, "Audio media request callback can not be null");
        synchronized (mLock) {
            if (mPrimaryZoneMediaAudioRequestCallback != null) {
                throw new IllegalStateException("Primary zone media audio request is already set");
            }
        }

        try {
            if (!mService.registerPrimaryZoneMediaAudioRequestCallback(
                    mIPrimaryZoneMediaAudioRequestCallback)) {
                return false;
            }
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, /* returnValue= */ false);
        }

        synchronized (mLock) {
            mPrimaryZoneMediaAudioRequestCallback = callback;
            mPrimaryZoneMediaAudioRequestCallbackExecutor = executor;
        }

        return true;
    }

    /**
     * Clears the currently set {@code PrimaryZoneMediaAudioRequestCallback}
     *
     * @throws IllegalStateException if dynamic audio routing is not enabled
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void clearPrimaryZoneMediaAudioRequestCallback() {
        assertPlatformVersionAtLeastU();
        synchronized (mLock) {
            if (mPrimaryZoneMediaAudioRequestCallback == null) {
                return;
            }
        }

        try {
            mService.unregisterPrimaryZoneMediaAudioRequestCallback(
                    mIPrimaryZoneMediaAudioRequestCallback);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }

        synchronized (mLock) {
            mPrimaryZoneMediaAudioRequestCallback = null;
            mPrimaryZoneMediaAudioRequestCallbackExecutor = null;
        }
    }

    /**
     * Cancels a request set by {@code requestMediaAudioOnPrimaryZone}
     *
     * @param requestId Request id to cancel
     * @return {@code true} if request is successfully cancelled
     * @throws IllegalStateException if dynamic audio routing is not enabled
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean cancelMediaAudioOnPrimaryZone(long requestId) {
        assertPlatformVersionAtLeastU();
        try {
            if (removeMediaRequestCallback(requestId)) {
                return mService.cancelMediaAudioOnPrimaryZone(requestId);
            }
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, /* returnValue= */ false);
        }

        return true;
    }

    private boolean removeMediaRequestCallback(long requestId) {
        return mRequestIdToMediaAudioRequestStatusCallbacks.remove(requestId) != null;
    }

    /**
     * Requests to play audio in primary zone with information contained in {@code request}
     *
     * @param info Occupant zone info whose media audio should be shared to primary zone
     * @param executor Executor on which callback will be invoked
     * @param callback Callback that will report the status changes of the request
     * @return returns a valid request id if successful or {@code INVALID_REQUEST_ID} otherwise
     * @throws NullPointerException if any of info, executor, or callback parameters are
     * {@code null}
     * @throws IllegalStateException if dynamic audio routing is not enabled, or if audio mirroring
     * is currently enabled for the audio zone owned by the occupant as configured by
     * {@link #enableMirrorForAudioZones(List)}
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public long requestMediaAudioOnPrimaryZone(@NonNull OccupantZoneInfo info,
            @NonNull @CallbackExecutor Executor executor,
            @NonNull MediaAudioRequestStatusCallback callback) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(info, "Occupant zone info can not be null");
        Objects.requireNonNull(executor, "Executor can not be null");
        Objects.requireNonNull(callback, "Media audio request status callback can not be null");

        MediaAudioRequestStatusCallbackWrapper wrapper =
                new MediaAudioRequestStatusCallbackWrapper(executor, callback);

        long requestId;
        try {
            requestId = mService.requestMediaAudioOnPrimaryZone(wrapper, info);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, INVALID_REQUEST_ID);
        }

        if (requestId == INVALID_REQUEST_ID) {
            return requestId;
        }

        mRequestIdToMediaAudioRequestStatusCallbacks.put(requestId, wrapper);
        return requestId;
    }

    /**
     * Allow/rejects audio to play for a request
     * {@code requestMediaAudioOnPrimaryZone(MediaRequest, Handler)}
     *
     * @param requestId Request id to approve
     * @param allow Boolean indicating to allow or reject, {@code true} to allow audio
     * playback on primary zone, {@code false} otherwise
     * @return {@code false} if media is not successfully allowed/rejected for the request,
     * including the case when the request id is {@link #INVALID_REQUEST_ID}
     * @throws IllegalStateException if no {@code PrimaryZoneMediaAudioRequestCallback} is
     * registered prior to calling this method.
     * @throws IllegalStateException if dynamic audio routing is not enabled, or if audio mirroring
     * is currently enabled for the audio zone owned by the occupant as configured by
     * {@link #enableMirrorForAudioZones(List)}
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean allowMediaAudioOnPrimaryZone(long requestId, boolean allow) {
        assertPlatformVersionAtLeastU();
        synchronized (mLock) {
            if (mPrimaryZoneMediaAudioRequestCallback == null) {
                throw new IllegalStateException("Primary zone media audio request callback must be "
                        + "registered to allow/reject playback");
            }
        }

        try {
            return mService.allowMediaAudioOnPrimaryZone(
                    mIPrimaryZoneMediaAudioRequestCallback.asBinder(), requestId, allow);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, /* returnValue= */ false);
        }
    }

    /**
     * Resets the media audio playback in primary zone from occupant
     *
     * @param info Occupant's audio to reset in primary zone
     * @return {@code true} if audio is successfully reset, {@code false} otherwise including case
     * where audio is not currently assigned
     * @throws IllegalStateException if dynamic audio routing is not enabled
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean resetMediaAudioOnPrimaryZone(@NonNull OccupantZoneInfo info) {
        assertPlatformVersionAtLeastU();
        try {
            return mService.resetMediaAudioOnPrimaryZone(info);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, /* returnValue= */ false);
        }
    }

    /**
     * Determines if audio from occupant is allowed in primary zone
     *
     * @param info Occupant zone info to query
     * @return {@code true} if audio playback from occupant is allowed in primary zone
     * @throws IllegalStateException if dynamic audio routing is not enabled
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean isMediaAudioAllowedInPrimaryZone(@NonNull OccupantZoneInfo info) {
        assertPlatformVersionAtLeastU();
        try {
            return mService.isMediaAudioAllowedInPrimaryZone(info);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, /* returnValue= */ false);
        }
    }

    /**
     * Registers audio mirror status callback
     *
     * @param executor Executor on which the callback will be invoked
     * @param callback Callback to inform about audio mirror status changes
     * @return {@code true} if audio zones mirror status is set successfully, or {@code false}
     * otherwise
     * @throws NullPointerException if {@link AudioZonesMirrorStatusCallback} or {@link Executor}
     * passed in are {@code null}
     * @throws IllegalStateException if dynamic audio routing is not enabled, also if
     * there is a callback already set
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean setAudioZoneMirrorStatusCallback(@NonNull @CallbackExecutor Executor executor,
            @NonNull AudioZonesMirrorStatusCallback callback) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(executor, "Executor can not be null");
        Objects.requireNonNull(callback, "Audio zones mirror status callback can not be null");

        synchronized (mLock) {
            if (mAudioZonesMirrorStatusCallbackWrapper != null) {
                throw new IllegalStateException("Audio zones mirror status "
                        + "callback is already set");
            }
        }
        AudioZonesMirrorStatusCallbackWrapper wrapper =
                new AudioZonesMirrorStatusCallbackWrapper(executor, callback);

        boolean succeeded;
        try {
            succeeded = mService.registerAudioZonesMirrorStatusCallback(wrapper);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }

        if (!succeeded) {
            return false;
        }
        boolean error;
        synchronized (mLock) {
            // Unless there is a race condition mAudioZonesMirrorStatusCallbackWrapper
            // should not be set
            error = mAudioZonesMirrorStatusCallbackWrapper != null;
            if (!error) {
                mAudioZonesMirrorStatusCallbackWrapper = wrapper;
            }
        }

        // In case there was an error, unregister the listener and throw an exception
        if (error) {
            try {
                mService.unregisterAudioZonesMirrorStatusCallback(wrapper);
            } catch (RemoteException e) {
                handleRemoteExceptionFromCarService(e);
            }

            throw new IllegalStateException("Audio zones mirror status callback is already set");
        }
        return true;
    }

    /**
     * Clears the currently set {@code AudioZonesMirrorStatusCallback}
     *
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void clearAudioZonesMirrorStatusCallback() {
        assertPlatformVersionAtLeastU();
        AudioZonesMirrorStatusCallbackWrapper wrapper;

        synchronized (mLock) {
            if (mAudioZonesMirrorStatusCallbackWrapper == null) {
                return;
            }
            wrapper = mAudioZonesMirrorStatusCallbackWrapper;
            mAudioZonesMirrorStatusCallbackWrapper = null;
        }

        try {
            mService.unregisterAudioZonesMirrorStatusCallback(wrapper);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Determines if it is possible to enable audio mirror
     *
     * @return returns status to determine if it is possible to enable audio mirror using the
     * {@link #enableMirrorForAudioZones(List)} API, if audio mirror can be enabled this will
     * return {@link #AUDIO_MIRROR_CAN_ENABLE}, or {@link #AUDIO_MIRROR_OUT_OF_OUTPUT_DEVICES} if
     * there are no more output devices currently available to mirror.
     * {@link #AUDIO_MIRROR_INTERNAL_ERROR} can also be returned in case there is an error when
     * communicating with the car audio service
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public @AudioMirrorStatus int canEnableAudioMirror() {
        assertPlatformVersionAtLeastU();
        try {
            return mService.canEnableAudioMirror();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, AUDIO_MIRROR_INTERNAL_ERROR);
        }
    }

    /**
     * Enables audio mirror for a set of audio zones
     *
     * <p><b>Note:<b/> The results will be notified in the {@link AudioZonesMirrorStatusCallback}
     * set via {@link #setAudioZoneMirrorStatusCallback(Executor, AudioZonesMirrorStatusCallback)}
     *
     * @param audioZonesToMirror List of audio zones that should have audio mirror enabled,
     * a minimum of two audio zones are needed to enable mirroring
     * @return returns a valid mirror request id if successful or {@code INVALID_REQUEST_ID}
     * otherwise
     * @throws NullPointerException if the audio mirror list is {@code null}
     * @throws IllegalArgumentException if the audio mirror list size is less than 2, if a zone id
     * repeats within the list, or if the list contains the {@link #PRIMARY_AUDIO_ZONE}
     * @throws IllegalStateException if dynamic audio routing is not enabled, or there is an
     * attempt to merge zones from two different mirroring request, or any of the zone ids
     * are currently sharing audio to primary zone as allowed via
     * {@link #allowMediaAudioOnPrimaryZone(long, boolean)}
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public long enableMirrorForAudioZones(@NonNull List<Integer> audioZonesToMirror) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(audioZonesToMirror, "Audio zones to mirror should not be null");

        try {
            return mService.enableMirrorForAudioZones(toIntArray(audioZonesToMirror));
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, INVALID_REQUEST_ID);
        }
    }

    /**
     * Extends the audio zone mirroring request by appending new zones to the mirroring
     * configuration. The zones previously mirroring in the audio mirroring configuration, will
     * continue to mirror and the mirroring will be further extended to the new zones.
     *
     * <p><b>Note:<b/> The results will be notified in the {@link AudioZonesMirrorStatusCallback}
     * set via {@link #setAudioZoneMirrorStatusCallback(Executor, AudioZonesMirrorStatusCallback)}.
     * For example, to further extend a mirroring request currently containing zones 1 and 2, with
     * a new zone (3) Simply call the API with zone 3 in the list, after the completion of audio
     * mirroring extension, zones 1, 2, and 3 will now have mirroring enabled.
     *
     * @param audioZonesToMirror List of audio zones that will be added to the mirroring request
     * @param mirrorId Audio mirroring request to expand with more audio zones
     * @throws NullPointerException if the audio mirror list is {@code null}
     * @throws IllegalArgumentException if a zone id repeats within the list, or if the list
     * contains the {@link #PRIMARY_AUDIO_ZONE}, or if the request id to expand is no longer valid
     * @throws IllegalStateException if dynamic audio routing is not enabled, or there is an
     * attempt to merge zones from two different mirroring request, or any of the zone ids
     * are currently sharing audio to primary zone as allowed via
     * {@link #allowMediaAudioOnPrimaryZone(long, boolean)}
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void extendAudioMirrorRequest(long mirrorId, @NonNull List<Integer> audioZonesToMirror) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(audioZonesToMirror, "Audio zones to mirror should not be null");

        try {
            mService.extendAudioMirrorRequest(mirrorId, toIntArray(audioZonesToMirror));
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Disables audio mirror for a particular audio zone
     *
     * <p><b>Note:<b/> The results will be notified in the {@link AudioZonesMirrorStatusCallback}
     * set via {@link #setAudioZoneMirrorStatusCallback(Executor, AudioZonesMirrorStatusCallback)}.
     * The results will contain the information for the audio zones whose mirror was cancelled.
     * For example, if the mirror configuration only has two zones, mirroring will be undone for
     * both zones and the callback will have both zones. On the other hand, if the mirroring
     * configuration contains three zones, then this API will only cancel mirroring for one zone
     * and the other two zone will continue mirroring. In this case, the callback will only have
     * information about the cancelled zone
     *
     * @param zoneId Zone id where audio mirror should be disabled
     * @throws IllegalArgumentException if the zoneId is invalid
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void disableAudioMirrorForZone(int zoneId) {
        assertPlatformVersionAtLeastU();
        try {
            mService.disableAudioMirrorForZone(zoneId);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Disables audio mirror for all the zones mirroring in a particular request
     *
     * <p><b>Note:<b/> The results will be notified in the {@link AudioZonesMirrorStatusCallback}
     * set via {@link #setAudioZoneMirrorStatusCallback(Executor, AudioZonesMirrorStatusCallback)}
     *
     * @param mirrorId Whose audio mirroring should be disabled as obtained via
     * {@link #enableMirrorForAudioZones(List)}
     * @throws IllegalArgumentException if the request id is no longer valid
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void disableAudioMirror(long mirrorId) {
        assertPlatformVersionAtLeastU();
        try {
            mService.disableAudioMirror(mirrorId);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Determines the current mirror configuration for an audio zone as set by
     * {@link #enableMirrorForAudioZones(List)} or extended via
     * {@link #extendAudioMirrorRequest(long, List)}
     *
     * @param zoneId The audio zone id where mirror audio should be queried
     * @return A list of audio zones where the queried audio zone is mirroring or empty if the
     * audio zone is not mirroring with any other audio zone. The list of zones will contain the
     * queried zone if audio mirroring is enabled for that zone.
     * @throws IllegalArgumentException if the audio zone id is invalid
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @NonNull
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public List<Integer> getMirrorAudioZonesForAudioZone(int zoneId) {
        assertPlatformVersionAtLeastU();
        try {
            return asList(mService.getMirrorAudioZonesForAudioZone(zoneId));
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Determines the current mirror configuration for a mirror id
     *
     * @param mirrorId The request id that should be queried
     * @return A list of audio zones where the queried audio zone is mirroring or empty if the
     * request id is no longer valid.
     * @throws IllegalArgumentException if mirror request id is {@link #INVALID_REQUEST_ID}
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if audio mirroring feature is disabled, which can be verified
     * using {@link #isAudioFeatureEnabled(int)} with the {@link #AUDIO_FEATURE_AUDIO_MIRRORING}
     * feature flag
     *
     * @hide
     */
    @SystemApi
    @NonNull
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public List<Integer> getMirrorAudioZonesForMirrorRequest(long mirrorId) {
        assertPlatformVersionAtLeastU();
        try {
            return asList(mService.getMirrorAudioZonesForMirrorRequest(mirrorId));
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /**
     * Gets the output device for a given {@link AudioAttributes} usage in zoneId.
     *
     * <p><b>Note:</b> To be used for routing to a specific device. Most applications should
     * use the regular routing mechanism, which is to set audio attribute usage to
     * an audio track.
     *
     * @param zoneId zone id to query for device
     * @param usage usage where audio is routed
     * @return Audio device info, returns {@code null} if audio device usage fails to map to
     * an active audio device. This is different from the using an invalid value for
     * {@link AudioAttributes} usage. In the latter case the query will fail with a
     * RuntimeException indicating the issue.
     *
     * @hide
     */
    @SystemApi
    @Nullable
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @AddedInOrBefore(majorVersion = 33)
    public AudioDeviceInfo getOutputDeviceForUsage(int zoneId, @AttributeUsage int usage) {
        try {
            String deviceAddress = mService.getOutputDeviceAddressForUsage(zoneId, usage);
            if (deviceAddress == null) {
                return null;
            }
            AudioDeviceInfo[] outputDevices =
                    mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
            for (AudioDeviceInfo info : outputDevices) {
                if (info.getAddress().equals(deviceAddress)) {
                    return info;
                }
            }
            return null;
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Gets the input devices for an audio zone
     *
     * @return list of input devices
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    @AddedInOrBefore(majorVersion = 33)
    public @NonNull List<AudioDeviceInfo> getInputDevicesForZoneId(int zoneId) {
        try {
            return convertInputDevicesToDeviceInfos(
                    mService.getInputDevicesForZoneId(zoneId),
                    AudioManager.GET_DEVICES_INPUTS);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.EMPTY_LIST);
        }
    }

    /** @hide */
    @Override
    @AddedInOrBefore(majorVersion = 33)
    public void onCarDisconnected() {
        if (mService == null) {
            return;
        }

        if (!mCarVolumeCallbacks.isEmpty()) {
            unregisterVolumeCallback();
        }
        if (!mCarVolumeEventCallbacks.isEmpty()) {
            unregisterVolumeGroupEventCallback();
        }
    }

    /** @hide */
    public CarAudioManager(Car car, IBinder service) {
        super(car);
        mService = ICarAudio.Stub.asInterface(service);
        mAudioManager = getContext().getSystemService(AudioManager.class);
        mCarVolumeCallbacks = new CopyOnWriteArrayList<>();
        mEventHandler = new EventHandler(getEventHandler().getLooper());
    }

    /**
     * Registers a {@link CarVolumeCallback} to receive volume change callbacks
     * @param callback {@link CarVolumeCallback} instance, can not be null
     * <p>
     * Requires permission Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME
     */
    @AddedInOrBefore(majorVersion = 33)
    public void registerCarVolumeCallback(@NonNull CarVolumeCallback callback) {
        Objects.requireNonNull(callback);

        if (mCarVolumeCallbacks.isEmpty()) {
            registerVolumeCallback();
        }

        mCarVolumeCallbacks.add(callback);
    }

    /**
     * Unregisters a {@link CarVolumeCallback} from receiving volume change callbacks
     * @param callback {@link CarVolumeCallback} instance previously registered, can not be null
     * <p>
     * Requires permission Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME
     */
    @AddedInOrBefore(majorVersion = 33)
    public void unregisterCarVolumeCallback(@NonNull CarVolumeCallback callback) {
        Objects.requireNonNull(callback);
        if (mCarVolumeCallbacks.contains(callback) && (mCarVolumeCallbacks.size() == 1)) {
            unregisterVolumeCallback();
        }

        mCarVolumeCallbacks.remove(callback);
    }

    private void registerVolumeCallback() {
        try {
            mService.registerVolumeCallback(mCarVolumeCallbackImpl.asBinder());
        } catch (RemoteException e) {
            Log.e(CarLibLog.TAG_CAR, "registerVolumeCallback failed", e);
        }
    }

    private void unregisterVolumeCallback() {
        try {
            mService.unregisterVolumeCallback(mCarVolumeCallbackImpl.asBinder());
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Registers a {@code CarVolumeGroupEventCallback} to receive volume group event callbacks
     *
     * @param executor Executor on which callback will be invoked
     * @param callback Callback that will report volume group events
     * @return {@code true} if the callback is successfully registered, {@code false} otherwise
     * @throws NullPointerException if executor or callback parameters is {@code null}
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if volume group events are not enabled
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    public boolean registerCarVolumeGroupEventCallback(
            @NonNull @CallbackExecutor Executor executor,
            @NonNull CarVolumeGroupEventCallback callback) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(executor, "Executor can not be null");
        Objects.requireNonNull(callback, "Car volume event callback can not be null");

        if (mCarVolumeEventCallbacks.isEmpty()) {
            if (!registerVolumeGroupEventCallback()) {
                return false;
            }
        }

        return mCarVolumeEventCallbacks.addIfAbsent(
                new CarVolumeGroupEventCallbackWrapper(executor, callback));
    }

    private boolean registerVolumeGroupEventCallback() {
        try {
            if (!mService.registerCarVolumeEventCallback(mCarVolumeEventCallbackImpl)) {
                return false;
            }
        } catch (RemoteException e) {
            Log.e(CarLibLog.TAG_CAR, "registerCarVolumeEventCallback failed", e);
            return handleRemoteExceptionFromCarService(e, /* returnValue= */ false);
        }

        return true;
    }

    /**
     * Unregisters a {@code CarVolumeGroupEventCallback} registered via
     * {@link #registerCarVolumeGroupEventCallback}
     *
     * @param callback The callback to be removed
     * @throws NullPointerException if callback is {@code null}
     * @throws IllegalStateException if dynamic audio routing is not enabled
     * @throws IllegalStateException if volume group events are not enabled
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_0,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_0)
    public void unregisterCarVolumeGroupEventCallback(
            @NonNull CarVolumeGroupEventCallback callback) {
        assertPlatformVersionAtLeastU();
        Objects.requireNonNull(callback, "Car volume event callback can not be null");

        CarVolumeGroupEventCallbackWrapper callbackWrapper =
                new CarVolumeGroupEventCallbackWrapper(/* executor= */ null, callback);
        if (mCarVolumeEventCallbacks.contains(callbackWrapper)
                && (mCarVolumeEventCallbacks.size() == 1)) {
            unregisterVolumeGroupEventCallback();
        }

        mCarVolumeEventCallbacks.remove(callbackWrapper);
    }

    private boolean unregisterVolumeGroupEventCallback() {
        try {
            if (!mService.unregisterCarVolumeEventCallback(mCarVolumeEventCallbackImpl)) {
                Log.e(CarLibLog.TAG_CAR,
                        "unregisterCarVolumeEventCallback failed with service");
                return false;
            }
        } catch (RemoteException e) {
            Log.e(CarLibLog.TAG_CAR,
                    "unregisterCarVolumeEventCallback failed with exception", e);
            handleRemoteExceptionFromCarService(e);
        }

        return true;
    }

    /**
     * Returns the whether a volume group is muted
     *
     * <p><b>Note:<b/> If {@link #AUDIO_FEATURE_VOLUME_GROUP_MUTING} is disabled this will always
     * return {@code false} as group mute is disabled.
     *
     * @param zoneId The zone id whose volume groups is queried.
     * @param groupId The volume group id whose mute state is returned.
     * @return {@code true} if the volume group is muted, {@code false}
     * otherwise
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public boolean isVolumeGroupMuted(int zoneId, int groupId) {
        try {
            return mService.isVolumeGroupMuted(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Sets a volume group mute
     *
     * <p><b>Note:<b/> If {@link #AUDIO_FEATURE_VOLUME_GROUP_MUTING} is disabled this will throw an
     * error indicating the issue.
     *
     * @param zoneId The zone id whose volume groups will be changed.
     * @param groupId The volume group id whose mute state will be changed.
     * @param mute {@code true} to mute volume group, {@code false} otherwise
     * @param flags One or more flags (e.g., {@link android.media.AudioManager#FLAG_SHOW_UI},
     * {@link android.media.AudioManager#FLAG_PLAY_SOUND})
     *
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    @AddedInOrBefore(majorVersion = 33)
    public void setVolumeGroupMute(int zoneId, int groupId, boolean mute, int flags) {
        try {
            mService.setVolumeGroupMute(zoneId, groupId, mute, flags);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    private List<AudioDeviceInfo> convertInputDevicesToDeviceInfos(
            List<AudioDeviceAttributes> devices, int flag) {
        int addressesSize = devices.size();
        Set<String> deviceAddressMap = new HashSet<>(addressesSize);
        for (int i = 0; i < addressesSize; ++i) {
            AudioDeviceAttributes device = devices.get(i);
            deviceAddressMap.add(device.getAddress());
        }
        List<AudioDeviceInfo> deviceInfoList = new ArrayList<>(devices.size());
        AudioDeviceInfo[] inputDevices = mAudioManager.getDevices(flag);
        for (int i = 0; i < inputDevices.length; ++i) {
            AudioDeviceInfo info = inputDevices[i];
            if (info.isSource() && deviceAddressMap.contains(info.getAddress())) {
                deviceInfoList.add(info);
            }
        }
        return deviceInfoList;
    }

    private final class EventHandler extends Handler {
        private static final int MSG_GROUP_VOLUME_CHANGE = 1;
        private static final int MSG_GROUP_MUTE_CHANGE = 2;
        private static final int MSG_MASTER_MUTE_CHANGE = 3;
        private static final int MSG_VOLUME_GROUP_EVENT = 4;

        private EventHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_GROUP_VOLUME_CHANGE:
                    VolumeGroupChangeInfo volumeInfo = (VolumeGroupChangeInfo) msg.obj;
                    handleOnGroupVolumeChanged(volumeInfo.mZoneId, volumeInfo.mGroupId,
                            volumeInfo.mFlags);
                    break;
                case MSG_GROUP_MUTE_CHANGE:
                    VolumeGroupChangeInfo muteInfo = (VolumeGroupChangeInfo) msg.obj;
                    handleOnGroupMuteChanged(muteInfo.mZoneId, muteInfo.mGroupId, muteInfo.mFlags);
                    break;
                case MSG_MASTER_MUTE_CHANGE:
                    handleOnMasterMuteChanged(msg.arg1, msg.arg2);
                    break;
                case MSG_VOLUME_GROUP_EVENT:
                    List<CarVolumeGroupEvent> events = (List<CarVolumeGroupEvent>) msg.obj;
                    handleOnVolumeGroupEvent(events);
                default:
                    Log.e(CarLibLog.TAG_CAR, "Unknown message not handled:" + msg.what);
                    break;
            }
        }

        private void dispatchOnGroupVolumeChanged(int zoneId, int groupId, int flags) {
            VolumeGroupChangeInfo volumeInfo = new VolumeGroupChangeInfo(zoneId, groupId, flags);
            sendMessage(obtainMessage(MSG_GROUP_VOLUME_CHANGE, volumeInfo));
        }

        private void dispatchOnMasterMuteChanged(int zoneId, int flags) {
            sendMessage(obtainMessage(MSG_MASTER_MUTE_CHANGE, zoneId, flags));
        }

        private void dispatchOnGroupMuteChanged(int zoneId, int groupId, int flags) {
            VolumeGroupChangeInfo volumeInfo = new VolumeGroupChangeInfo(zoneId, groupId, flags);
            sendMessage(obtainMessage(MSG_GROUP_MUTE_CHANGE, volumeInfo));
        }

        private void dispatchOnVolumeGroupEvent(List<CarVolumeGroupEvent> events) {
            sendMessage(obtainMessage(MSG_VOLUME_GROUP_EVENT, events));
        }

        private class VolumeGroupChangeInfo {
            public int mZoneId;
            public int mGroupId;
            public int mFlags;

            VolumeGroupChangeInfo(int zoneId, int groupId, int flags) {
                mZoneId = zoneId;
                mGroupId = groupId;
                mFlags = flags;
            }
        }
    }

    private void handleOnGroupVolumeChanged(int zoneId, int groupId, int flags) {
        for (CarVolumeCallback callback : mCarVolumeCallbacks) {
            callback.onGroupVolumeChanged(zoneId, groupId, flags);
        }
    }

    private void handleOnMasterMuteChanged(int zoneId, int flags) {
        for (CarVolumeCallback callback : mCarVolumeCallbacks) {
            callback.onMasterMuteChanged(zoneId, flags);
        }
    }

    private void handleOnGroupMuteChanged(int zoneId, int groupId, int flags) {
        for (CarVolumeCallback callback : mCarVolumeCallbacks) {
            callback.onGroupMuteChanged(zoneId, groupId, flags);
        }
    }


    private void handleOnVolumeGroupEvent(List<CarVolumeGroupEvent> events) {
        for (CarVolumeGroupEventCallbackWrapper wr : mCarVolumeEventCallbacks) {
            wr.mExecutor.execute(() -> wr.mCallback.onVolumeGroupEvent(events));
        }
    }

    private static int[] toIntArray(List<Integer> list) {
        int size = list.size();
        int[] array = new int[size];
        for (int i = 0; i < size; ++i) {
            array[i] = list.get(i);
        }
        return array;
    }

    private static List<Integer> asList(int[] intArray) {
        List<Integer> zoneIdList = new ArrayList<Integer>(intArray.length);
        for (int index = 0; index < intArray.length; index++) {
            zoneIdList.add(intArray[index]);
        }
        return zoneIdList;
    }

    /**
     * Callback interface to receive volume change events in a car.
     * Extend this class and register it with {@link #registerCarVolumeCallback(CarVolumeCallback)}
     * and unregister it via {@link #unregisterCarVolumeCallback(CarVolumeCallback)}
     */
    public abstract static class CarVolumeCallback {
        /**
         * This is called whenever a group volume is changed.
         *
         * The changed-to volume index is not included, the caller is encouraged to
         * get the current group volume index via CarAudioManager.
         *
         * <p><b>Notes:</b>
         * <ul>
         *     <li>If both {@link CarVolumeCallback} and {@code CarVolumeGroupEventCallback}
         *     are registered by the same app, then volume group index changes are <b>only</b>
         *     propagated through CarVolumeGroupEventCallback (until it is unregistered)</li>
         *     <li>Apps are encouraged to migrate to the new callback
         *     {@link CarVolumeGroupInfoCallback}</li>
         * </ul>
         *
         * @param zoneId Id of the audio zone that volume change happens
         * @param groupId Id of the volume group that volume is changed
         * @param flags see {@link android.media.AudioManager} for flag definitions
         */
        @ExcludeFromCodeCoverageGeneratedReport(reason = BOILERPLATE_CODE)
        @AddedInOrBefore(majorVersion = 33)
        public void onGroupVolumeChanged(int zoneId, int groupId, int flags) {}

        /**
         * This is called whenever the global mute state is changed.
         * The changed-to global mute state is not included, the caller is encouraged to
         * get the current global mute state via AudioManager.
         *
         * <p><b>Note:<b/> If {@link CarAudioManager#AUDIO_FEATURE_VOLUME_GROUP_MUTING} is disabled
         * this will be triggered on mute changes. Otherwise, car audio mute changes will trigger
         * {@link #onGroupMuteChanged(int, int, int)}
         *
         * @param zoneId Id of the audio zone that global mute state change happens
         * @param flags see {@link android.media.AudioManager} for flag definitions
         */
        @ExcludeFromCodeCoverageGeneratedReport(reason = BOILERPLATE_CODE)
        @AddedInOrBefore(majorVersion = 33)
        public void onMasterMuteChanged(int zoneId, int flags) {}

        /**
         * This is called whenever a group mute state is changed.
         *
         * The changed-to mute state is not included, the caller is encouraged to
         * get the current group mute state via CarAudioManager.
         *
         * <p><b>Notes:</b>
         * <ul>
         *     <li>If {@link CarAudioManager#AUDIO_FEATURE_VOLUME_GROUP_MUTING} is enabled
         *     this will be triggered on mute changes. Otherwise, car audio mute changes will
         *     trigger {@link #onMasterMuteChanged(int, int)}</li>
         *     <li>If both {@link CarVolumeCallback} and {@code CarVolumeGroupEventCallback}
         *     are registered by the same app, then volume group mute changes are <b>only</b>
         *     propagated through CarVolumeGroupEventCallback (until it is unregistered)</li>
         *     <li>Apps are encouraged to migrate to the new callback
         *     {@link CarVolumeGroupInfoCallback}</li>
         * </ul>
         *
         * @param zoneId Id of the audio zone that volume change happens
         * @param groupId Id of the volume group that volume is changed
         * @param flags see {@link android.media.AudioManager} for flag definitions
         */
        @ExcludeFromCodeCoverageGeneratedReport(reason = BOILERPLATE_CODE)
        @AddedInOrBefore(majorVersion = 33)
        public void onGroupMuteChanged(int zoneId, int groupId, int flags) {}
    }

    private static final class MediaAudioRequestStatusCallbackWrapper
            extends IMediaAudioRequestStatusCallback.Stub {

        private final Executor mExecutor;
        private final MediaAudioRequestStatusCallback mCallback;

        MediaAudioRequestStatusCallbackWrapper(Executor executor,
                MediaAudioRequestStatusCallback callback) {
            mExecutor = executor;
            mCallback = callback;
        }

        @Override
        public void onMediaAudioRequestStatusChanged(CarOccupantZoneManager.OccupantZoneInfo info,
                long requestId,
                @CarAudioManager.MediaAudioRequestStatus int status) throws RemoteException {
            long identity = Binder.clearCallingIdentity();
            try {
                mExecutor.execute(() ->
                        mCallback.onMediaAudioRequestStatusChanged(info, requestId, status));
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
    }

    private static final class SwitchAudioZoneConfigCallbackWrapper
            extends ISwitchAudioZoneConfigCallback.Stub {
        private final Executor mExecutor;
        private final SwitchAudioZoneConfigCallback mCallback;

        SwitchAudioZoneConfigCallbackWrapper(Executor executor,
                SwitchAudioZoneConfigCallback callback) {
            mExecutor = executor;
            mCallback = callback;
        }

        @Override
        public void onAudioZoneConfigSwitched(CarAudioZoneConfigInfo zoneConfig,
                boolean isSuccessful) {
            long identity = Binder.clearCallingIdentity();
            try {
                mExecutor.execute(() ->
                        mCallback.onAudioZoneConfigSwitched(zoneConfig, isSuccessful));
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
    }

    private static final class CarVolumeGroupEventCallbackWrapper {
        private final Executor mExecutor;
        private final CarVolumeGroupEventCallback mCallback;

        CarVolumeGroupEventCallbackWrapper(Executor executor,
                CarVolumeGroupEventCallback callback) {
            mExecutor = executor;
            mCallback = callback;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) {
                return true;
            }

            if (!(o instanceof CarVolumeGroupEventCallbackWrapper)) {
                return false;
            }

            CarVolumeGroupEventCallbackWrapper rhs = (CarVolumeGroupEventCallbackWrapper) o;
            return mCallback == rhs.mCallback;
        }

        @Override
        public int hashCode() {
            return mCallback.hashCode();
        }
    }

    private static final class AudioZonesMirrorStatusCallbackWrapper
            extends IAudioZonesMirrorStatusCallback.Stub {

        private final Executor mExecutor;
        private final AudioZonesMirrorStatusCallback mCallback;

        AudioZonesMirrorStatusCallbackWrapper(Executor executor,
                AudioZonesMirrorStatusCallback callback) {
            mExecutor = executor;
            mCallback = callback;
        }

        public void onAudioZonesMirrorStatusChanged(int[] mirroredAudioZones,
                int status) {
            long identity = Binder.clearCallingIdentity();
            try {
                mExecutor.execute(() -> mCallback.onAudioZonesMirrorStatusChanged(
                        asList(mirroredAudioZones), status));
            } finally {
                Binder.restoreCallingIdentity(identity);
            }
        }
    }
}
