/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.settings.sound;

import static android.car.media.CarAudioManager.AUDIO_FEATURE_DYNAMIC_ROUTING;
import static android.car.media.CarAudioManager.AUDIO_FEATURE_VOLUME_GROUP_EVENTS;
import static android.car.media.CarAudioManager.AUDIO_FEATURE_VOLUME_GROUP_MUTING;
import static android.car.media.CarAudioManager.PRIMARY_AUDIO_ZONE;
import static android.car.media.CarVolumeGroupEvent.EVENT_TYPE_MUTE_CHANGED;
import static android.car.media.CarVolumeGroupEvent.EVENT_TYPE_VOLUME_GAIN_INDEX_CHANGED;
import static android.car.media.CarVolumeGroupEvent.EVENT_TYPE_VOLUME_MAX_INDEX_CHANGED;
import static android.car.media.CarVolumeGroupEvent.EVENT_TYPE_VOLUME_MIN_INDEX_CHANGED;
import static android.os.UserManager.DISALLOW_ADJUST_VOLUME;
import static android.os.Process.myUid;

import static com.android.car.settings.enterprise.ActionDisabledByAdminDialogFragment.DISABLED_BY_ADMIN_CONFIRM_DIALOG_TAG;
import static com.android.car.settings.enterprise.EnterpriseUtils.hasUserRestrictionByDpm;
import static com.android.car.settings.enterprise.EnterpriseUtils.hasUserRestrictionByUm;

import android.car.CarNotConnectedException;
import android.car.drivingstate.CarUxRestrictions;
import android.car.media.CarAudioManager;
import android.car.media.CarVolumeGroupEvent;
import android.car.media.CarVolumeGroupEventCallback;
import android.car.media.CarVolumeGroupInfo;
import android.media.AudioManager;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.SparseArray;
import android.widget.Toast;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.XmlRes;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.CarSettingsApplication;
import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.car.settings.common.SeekBarPreference;
import com.android.car.settings.enterprise.EnterpriseUtils;
import com.android.car.settings.sound.VolumeItemParser.VolumeItem;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;

/**
 * Business logic which parses car volume items into groups, creates a seek bar preference for each
 * group, and interfaces with the ringtone manager and audio manager.
 *
 * @see VolumeSettingsRingtoneManager
 * @see android.car.media.CarAudioManager
 */
public class VolumeSettingsPreferenceController extends PreferenceController<PreferenceGroup> {
    private static final Logger LOG = new Logger(VolumeSettingsPreferenceController.class);
    private static final String VOLUME_GROUP_KEY = "volume_group_key";
    private static final String VOLUME_USAGE_KEY = "volume_usage_key";

    private final SparseArray<VolumeItem> mVolumeItems;
    private final List<VolumeSeekBarPreference> mVolumePreferences = new ArrayList<>();
    private final VolumeSettingsRingtoneManager mRingtoneManager;
    private final AudioManager mAudioManager;
    private final Handler mUiHandler;
    private final Executor mExecutor;
    private int mZoneId;
    private CarAudioManager mCarAudioManager;
    private Context mContext;

    // only handle stream_music
    private void syncCarVolumeToSystemVolume(int zoneId, int volumeGroupId) {
        if (volumeGroupId != 0) {
            return;
        }
        int streamType = AudioManager.STREAM_MUSIC;
        int currentStreamVolume = mAudioManager.getStreamVolume(streamType);
        int minStreamVolume = mAudioManager.getStreamMinVolume(streamType);
        int maxStreamVolume = mAudioManager.getStreamMaxVolume(streamType);
        int currentCarVolume = mCarAudioManager.getGroupVolume(zoneId, volumeGroupId);
        int minCarVolume = mCarAudioManager.getGroupMinVolume(zoneId, volumeGroupId);
        int maxCarVolume = mCarAudioManager.getGroupMaxVolume(zoneId, volumeGroupId);
        int transCarVolume = minCarVolume + (int)((float)(currentStreamVolume  - minStreamVolume) / (float)(maxStreamVolume - minStreamVolume) * (maxCarVolume - minCarVolume));
        if (Math.abs(transCarVolume - currentCarVolume) > 1) {
            setGroupVolume(volumeGroupId, transCarVolume);
        }

    }

    private void syncSystemVolumeToCarVolume(int zoneId, int volumeGroupId, int carVolume) {
        if (volumeGroupId != 0) {
            return;
        }
        int streamType = AudioManager.STREAM_MUSIC;
        int streamVolume = mAudioManager.getStreamVolume(streamType);
        int minStreamVolume = mAudioManager.getStreamMinVolume(streamType);
        int maxStreamVolume = mAudioManager.getStreamMaxVolume(streamType);
        int minCarVolume = mCarAudioManager.getGroupMinVolume(zoneId, volumeGroupId);
        int maxCarVolume = mCarAudioManager.getGroupMaxVolume(zoneId, volumeGroupId);
        int transStreamVolume = minStreamVolume + (int)((float)(carVolume  - minCarVolume) / (float)(maxCarVolume - minCarVolume) * (maxStreamVolume - minStreamVolume));
        if (Math.abs(transStreamVolume - streamVolume) > 1) {
            mAudioManager.setStreamVolume(streamType, transStreamVolume, 0);
        }
    }

    @VisibleForTesting
    final CarAudioManager.CarVolumeCallback mVolumeChangeCallback =
            new CarAudioManager.CarVolumeCallback() {
                @Override
                public void onGroupVolumeChanged(int zoneId, int groupId, int flags) {
                    updateVolumeAndMute(zoneId, groupId, EVENT_TYPE_VOLUME_GAIN_INDEX_CHANGED);
                }

                @Override
                public void onMasterMuteChanged(int zoneId, int flags) {

                    // Mute is not being used yet
                }

                @Override
                public void onGroupMuteChanged(int zoneId, int groupId, int flags) {
                    updateVolumeAndMute(zoneId, groupId, EVENT_TYPE_MUTE_CHANGED);
                }
            };

    @VisibleForTesting
    final CarVolumeGroupEventCallback mCarVolumeGroupEventCallback =
            new CarVolumeGroupEventCallback() {
        @Override
        public void onVolumeGroupEvent(List<CarVolumeGroupEvent> volumeGroupEvents) {
            updateVolumeGroupForEvents(volumeGroupEvents);
        }
    };

    public VolumeSettingsPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        this(context, preferenceKey, fragmentController, uxRestrictions,
                new VolumeSettingsRingtoneManager(context));
        mContext = context;
    }

    @VisibleForTesting
    VolumeSettingsPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions,
            VolumeSettingsRingtoneManager ringtoneManager) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mZoneId = PRIMARY_AUDIO_ZONE;
        mRingtoneManager = ringtoneManager;
        mAudioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        mVolumeItems = VolumeItemParser.loadAudioUsageItems(context, carVolumeItemsXml());
        mUiHandler = new Handler(Looper.getMainLooper());
        mExecutor = context.getMainExecutor();

        mCarAudioManager = getCarAudioManager();
        if (mCarAudioManager != null) {
            int zoneId = getMyAudioZoneId();
            int volumeGroupCount = mCarAudioManager.getVolumeGroupCount(zoneId);
            cleanUpVolumePreferences();
            // Populates volume slider items from volume groups to UI.
            for (int groupId = 0; groupId < volumeGroupCount; groupId++) {
                VolumeItem volumeItem = getVolumeItemForUsages(
                        mCarAudioManager.getUsagesForVolumeGroupId(zoneId, groupId));
                VolumeSeekBarPreference volumePreference = createVolumeSeekBarPreference(
                        groupId, volumeItem.getUsage(), volumeItem.getIcon(),
                        volumeItem.getMuteIcon(), volumeItem.getTitle());
                setClickableWhileDisabled(volumePreference, /* clickable= */ true, p -> {
                    if (hasUserRestrictionByDpm(getContext(), DISALLOW_ADJUST_VOLUME)) {
                        showActionDisabledByAdminDialog();
                    } else {
                        Toast.makeText(getContext(),
                                getContext().getString(R.string.action_unavailable),
                                Toast.LENGTH_LONG).show();
                    }
                });
                mVolumePreferences.add(volumePreference);
            }

            if (mCarAudioManager.isAudioFeatureEnabled(AUDIO_FEATURE_VOLUME_GROUP_EVENTS)) {
                mCarAudioManager.registerCarVolumeGroupEventCallback(mExecutor,
                        mCarVolumeGroupEventCallback);
            } else {
                mCarAudioManager.registerCarVolumeCallback(mVolumeChangeCallback);
            }
        }
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected void onStartInternal() {
        if (mCarAudioManager == null) {
            mCarAudioManager = getCarAudioManager();
            if (mCarAudioManager != null) {
                if (isAudioFeatureEnabled(AUDIO_FEATURE_DYNAMIC_ROUTING)) {
                    int uid = myUid();
                    mZoneId = getZoneIdForUid(uid);
                }
                int volumeGroupCount = mCarAudioManager.getVolumeGroupCount(mZoneId);
                cleanUpVolumePreferences();
                // Populates volume slider items from volume groups to UI.
                for (int groupId = 0; groupId < volumeGroupCount; groupId++) {
                    VolumeItem volumeItem = getVolumeItemForUsages(
                            mCarAudioManager.getUsagesForVolumeGroupId(mZoneId, groupId));
                    VolumeSeekBarPreference volumePreference = createVolumeSeekBarPreference(
                            groupId, volumeItem.getUsage(), volumeItem.getIcon(),
                            volumeItem.getMuteIcon(), volumeItem.getTitle());
                    setClickableWhileDisabled(volumePreference, /* clickable= */ true, p -> {
                        if (hasUserRestrictionByDpm(getContext(), DISALLOW_ADJUST_VOLUME)) {
                            showActionDisabledByAdminDialog();
                        } else {
                            Toast.makeText(getContext(),
                                    getContext().getString(R.string.action_unavailable),
                                    Toast.LENGTH_LONG).show();
                        }
                    });
                    mVolumePreferences.add(volumePreference);
                }
                mCarAudioManager.registerCarVolumeCallback(mVolumeChangeCallback);
            }
        }
    }

    @Override
    protected void onPauseInternal() {
        cleanUpVolumePreferences();
        refreshUi();
    }

    /** Disconnect from car on stop. */
    @Override
    protected void onStopInternal() {
        cleanupAudioManager();
    }

    /** Disconnect from car on destroy. */
    @Override
    protected void onDestroyInternal() {
    }

    @Override
    protected void updateState(PreferenceGroup preferenceGroup) {
        preferenceGroup.removeAll();
        for (SeekBarPreference preference : mVolumePreferences) {
            preferenceGroup.addPreference(preference);
        }
    }

    /**
     * The resource which lists the car volume resources associated with the various usage enums.
     */
    @XmlRes
    @VisibleForTesting
    int carVolumeItemsXml() {
        return R.xml.car_volume_items;
    }

    private VolumeSeekBarPreference createVolumeSeekBarPreference(
            int volumeGroupId, int usage, @DrawableRes int primaryIconResId,
            @DrawableRes int secondaryIconResId, @StringRes int titleId) {
        VolumeSeekBarPreference preference = new VolumeSeekBarPreference(getContext());
        preference.setTitle(getContext().getString(titleId));
        preference.setUnMutedIcon(getContext().getDrawable(primaryIconResId));
        preference.getUnMutedIcon().setTintList(
                getContext().getColorStateList(R.color.icon_color_default));
        preference.setMutedIcon(getContext().getDrawable(secondaryIconResId));
        preference.getMutedIcon().setTintList(
                getContext().getColorStateList(R.color.icon_color_default));

        int zoneId = getMyAudioZoneId();
        CarAudioManager carAudioManager = getCarAudioManager();
        try {
            if (carAudioManager != null) {
                syncCarVolumeToSystemVolume(zoneId, volumeGroupId);
                preference.setValue(carAudioManager.getGroupVolume(zoneId, volumeGroupId));
                preference.setMin(carAudioManager.getGroupMinVolume(zoneId, volumeGroupId));
                preference.setMax(carAudioManager.getGroupMaxVolume(zoneId, volumeGroupId));
                preference.setIsMuted(isGroupMuted(carAudioManager, volumeGroupId));
            }
        } catch (CarNotConnectedException e) {
            LOG.e("Car is not connected!", e);
        }
        preference.setContinuousUpdate(true);
        preference.setShowSeekBarValue(false);
        Bundle bundle = preference.getExtras();
        bundle.putInt(VOLUME_GROUP_KEY, volumeGroupId);
        bundle.putInt(VOLUME_USAGE_KEY, usage);
        preference.setOnPreferenceChangeListener((pref, newValue) -> {
            int prefGroup = pref.getExtras().getInt(VOLUME_GROUP_KEY);
            int prefUsage = pref.getExtras().getInt(VOLUME_USAGE_KEY);
            int newVolume = (Integer) newValue;
            syncSystemVolumeToCarVolume(zoneId, prefGroup, newVolume);
            setGroupVolume(prefGroup, newVolume);
            if (carAudioManager != null
                    && (!carAudioManager.isAudioFeatureEnabled(AUDIO_FEATURE_DYNAMIC_ROUTING)
                    || !carAudioManager.isPlaybackOnVolumeGroupActive(zoneId, volumeGroupId))) {
                mRingtoneManager.playAudioFeedback(prefGroup, prefUsage);
            }
            return true;
        });
        return preference;
    }

    private boolean isGroupMuted(CarAudioManager carAudioManager, int volumeGroupId) {
        if (!carAudioManager.isAudioFeatureEnabled(AUDIO_FEATURE_VOLUME_GROUP_MUTING)) {
            return false;
        }
        return carAudioManager.isVolumeGroupMuted(getMyAudioZoneId(), volumeGroupId);
    }

    private void updateVolumeAndMute(int zoneId, int groupId, int eventTypes) {
        if (zoneId != getMyAudioZoneId()) {
            return;
        }

        CarAudioManager carAudioManager = getCarAudioManager();
        if (carAudioManager != null) {
            updateVolumePreference(carAudioManager.getVolumeGroupInfo(zoneId, groupId), eventTypes);
        }
    }

    private void setGroupVolume(int volumeGroupId, int newVolume) {
        try {
            getCarAudioManager()
                    .setGroupVolume(getMyAudioZoneId(), volumeGroupId, newVolume, /* flags= */ 0);
        } catch (CarNotConnectedException e) {
            LOG.w("Ignoring volume change event because the car isn't connected", e);
        }
    }

    private void setGroupVolume(int zoneId, int volumeGroupId, int newVolume) {
        try {
            mCarAudioManager.setGroupVolume(zoneId, volumeGroupId, newVolume, /* flags= */ 0);
        } catch (CarNotConnectedException e) {
            LOG.w("Ignoring volume change event because the car isn't connected", e);
        }
    }

    private int getZoneIdForUid(int uid) {
        int zoneId = PRIMARY_AUDIO_ZONE;
        try {
            zoneId = mCarAudioManager.getZoneIdForUid(uid);
        } catch (CarNotConnectedException e) {
            LOG.w("getZoneIdForUid failed because the car isn't connected", e);
        }
        return zoneId;
    }

    private boolean isAudioFeatureEnabled(int audioFeature) {
        boolean isAudioFeatureSupport = false;
        try {
            isAudioFeatureSupport = mCarAudioManager.isAudioFeatureEnabled(audioFeature);
        } catch (CarNotConnectedException e) {
            LOG.w("isAudioFeatureEnabled failed because the car isn't connected", e);
        }
        return isAudioFeatureSupport;
    }

    private void cleanupAudioManager() {
        cleanUpVolumePreferences();
        CarAudioManager carAudioManager = getCarAudioManager();
        if (carAudioManager != null) {
            carAudioManager.unregisterCarVolumeCallback(mVolumeChangeCallback);
            if (carAudioManager.isAudioFeatureEnabled(AUDIO_FEATURE_VOLUME_GROUP_EVENTS)) {
                carAudioManager.unregisterCarVolumeGroupEventCallback(mCarVolumeGroupEventCallback);
            }
        }
    }

    private void cleanUpVolumePreferences() {
        mRingtoneManager.stopCurrentRingtone();
        mVolumePreferences.clear();
    }

    private VolumeItem getVolumeItemForUsages(int[] usages) {
        int rank = Integer.MAX_VALUE;
        VolumeItem result = null;
        for (int usage : usages) {
            VolumeItem volumeItem = mVolumeItems.get(usage);
            if (volumeItem.getRank() < rank) {
                rank = volumeItem.getRank();
                result = volumeItem;
            }
        }
        return result;
    }

    @Override
    public int getDefaultAvailabilityStatus() {
        if (hasUserRestrictionByUm(getContext(), DISALLOW_ADJUST_VOLUME)
                || hasUserRestrictionByDpm(getContext(), DISALLOW_ADJUST_VOLUME)) {
            return AVAILABLE_FOR_VIEWING;
        }
        return AVAILABLE;
    }

    private void showActionDisabledByAdminDialog() {
        getFragmentController().showDialog(
                EnterpriseUtils.getActionDisabledByAdminDialog(getContext(),
                        DISALLOW_ADJUST_VOLUME),
                DISABLED_BY_ADMIN_CONFIRM_DIALOG_TAG);
    }

    private int getMyAudioZoneId() {
        return ((CarSettingsApplication) getContext().getApplicationContext())
                .getMyAudioZoneId();
    }

    private CarAudioManager getCarAudioManager() {
        return ((CarSettingsApplication) getContext().getApplicationContext())
                .getCarAudioManager();
    }

    private void updateVolumeGroupForEvents(List<CarVolumeGroupEvent> volumeGroupEvents) {
        List<CarVolumeGroupEvent> filteredEvents =
                filterVolumeGroupEventForZoneId(getMyAudioZoneId(), volumeGroupEvents);
        for (int index = 0; index < filteredEvents.size(); index++) {
            CarVolumeGroupEvent event = filteredEvents.get(index);
            int eventTypes = event.getEventTypes();
            List<CarVolumeGroupInfo> infos = event.getCarVolumeGroupInfos();
            for (int infoIndex = 0; infoIndex < infos.size(); infoIndex++) {
                updateVolumePreference(infos.get(infoIndex), eventTypes);
            }
        }
    }

    private List<CarVolumeGroupEvent> filterVolumeGroupEventForZoneId(int zoneId,
            List<CarVolumeGroupEvent> volumeGroupEvents) {
        List<CarVolumeGroupEvent> filteredEvents = new ArrayList<>();
        for (int index = 0; index < volumeGroupEvents.size(); index++) {
            CarVolumeGroupEvent event = volumeGroupEvents.get(index);
            List<CarVolumeGroupInfo> infos = event.getCarVolumeGroupInfos();
            for (int infoIndex = 0; infoIndex < infos.size(); infoIndex++) {
                if (infos.get(infoIndex).getZoneId() == zoneId) {
                    filteredEvents.add(event);
                    break;
                }
            }
        }
        return filteredEvents;
    }

    private void updateVolumePreference(CarVolumeGroupInfo groupInfo, int eventTypes) {
        int groupId = groupInfo.getId();
        for (VolumeSeekBarPreference volumePreference : mVolumePreferences) {
            Bundle extras = volumePreference.getExtras();
            if (extras.getInt(VOLUME_GROUP_KEY) == groupId) {
                mUiHandler.post(() -> {
                    if ((eventTypes & EVENT_TYPE_VOLUME_GAIN_INDEX_CHANGED) != 0) {
                        volumePreference.setValue(groupInfo.getVolumeGainIndex());
                    }
                    if ((eventTypes & EVENT_TYPE_MUTE_CHANGED) != 0) {
                        volumePreference.setIsMuted(groupInfo.isMuted());
                    }
                    if ((eventTypes & EVENT_TYPE_VOLUME_MIN_INDEX_CHANGED) != 0) {
                        volumePreference.setMin(groupInfo.getMinVolumeGainIndex());
                    }
                    if ((eventTypes & EVENT_TYPE_VOLUME_MAX_INDEX_CHANGED) != 0) {
                        volumePreference.setMax(groupInfo.getMaxVolumeGainIndex());
                    }
                });
                break;
            }
        }
    }
}
