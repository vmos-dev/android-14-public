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

package com.android.car.settings.applications.specialaccess;

import android.Manifest;
import android.app.ActivityThread;
import android.app.AppOpsManager;
import android.content.pm.ApplicationInfo;
import android.content.pm.IPackageManager;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.Log;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.R;
import com.android.car.settings.applications.specialaccess.AppStateAppOpsBridge.PermissionState;
import com.android.car.settings.applications.specialaccess.AppStateInstallAppsBridge.InstallAppsState;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiSwitchPreference;
import com.android.internal.util.ArrayUtils;
import com.android.settingslib.applications.ApplicationsState;
import com.android.settingslib.applications.ApplicationsState.AppEntry;
import com.android.settingslib.applications.ApplicationsState.AppFilter;
import com.android.settingslib.applications.ApplicationsState.CompoundFilter;

import java.util.List;

public class RequestInstallPackageController extends PreferenceController<PreferenceGroup> {
    private static final String TAG = RequestInstallPackageController.class.getSimpleName();

    private IPackageManager mIPackageManager;
    private AppOpsManager mAppOpsManager;

    private AppEntryListManager mAppEntryListManager;
    private List<AppEntry> mEntries;

    private static final AppFilter FILTER_REQUEST_INSTALL_PACKAGE = new AppFilter() {
        @Override
        public void init() {
            // No op.
        }

        @Override
        public boolean filterApp(ApplicationsState.AppEntry info) {
            if (info.extraInfo == null || !(info.extraInfo instanceof InstallAppsState)) {
                return false;
            }
            InstallAppsState state = (InstallAppsState) info.extraInfo;
            return state.isPotentialAppSource();
        }
    };

    private final Preference.OnPreferenceChangeListener mOnPreferenceChangeListener = new Preference.OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            if (preference instanceof AppStateInstallPreference) {
                final boolean checked = (boolean) newValue;
                AppEntry mAppEntry = ((AppStateInstallPreference)preference).getAppEntry();
                InstallAppsState mInstallAppsState = (InstallAppsState)mAppEntry.extraInfo;
                if (mInstallAppsState != null && checked != mInstallAppsState.canInstallApps()) {
                    setCanInstallApps(mAppEntry, checked);
                    mAppEntryListManager.forceUpdate(mAppEntry);
                }
            }
            return true;
        }
    };

    private final AppEntryListManager.Callback mCallback = new AppEntryListManager.Callback() {
        @Override
        public void onAppEntryListChanged(List<AppEntry> entries) {
            mEntries = entries;
            refreshUi();
        }
    };

    public RequestInstallPackageController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        this(context, preferenceKey, fragmentController, uxRestrictions,
                context.getSystemService(AppOpsManager.class), new AppEntryListManager(context));
    }

    @VisibleForTesting
    RequestInstallPackageController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions,
            AppOpsManager appOpsManager, AppEntryListManager appEntryListManager) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mIPackageManager = ActivityThread.getPackageManager();
        mAppOpsManager = appOpsManager;
        mAppEntryListManager = appEntryListManager;
    }

    private void setCanInstallApps(AppEntry mAppEntry, boolean newState) {
        Log.d(TAG, "packageName = " + mAppEntry.info.packageName + ", newState = " + newState + ", uid = " + mAppEntry.info.uid);
        mAppOpsManager.setMode(AppOpsManager.OP_REQUEST_INSTALL_PACKAGES,
                mAppEntry.info.uid, mAppEntry.info.packageName,
                newState ? AppOpsManager.MODE_ALLOWED : AppOpsManager.MODE_ERRORED);
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected void onCreateInternal() {
        mAppEntryListManager.init(new AppStateInstallAppsBridge(mAppOpsManager, mIPackageManager),
                this::getAppFilter, mCallback);
    }

    @Override
    protected void onStartInternal() {
        mAppEntryListManager.start();
    }

    @Override
    protected void onStopInternal() {
        mAppEntryListManager.stop();
    }

    @Override
    protected void onDestroyInternal() {
        mAppEntryListManager.destroy();
    }

    @Override
    protected void updateState(PreferenceGroup preference) {
        if (mEntries == null) {
            // Still loading.
            return;
        }
        preference.removeAll();
        for (AppEntry entry : mEntries) {
            Preference appPreference = new AppStateInstallPreference(getContext(), entry);
            appPreference.setOnPreferenceChangeListener(mOnPreferenceChangeListener);
            preference.addPreference(appPreference);
        }
    }

    @CallSuper
    protected AppFilter getAppFilter() {
        return FILTER_REQUEST_INSTALL_PACKAGE;
    }

    private static class AppStateInstallPreference extends CarUiSwitchPreference {

        private final AppEntry mEntry;

        AppStateInstallPreference(Context context, AppEntry entry) {
            super(context);
            String key = entry.info.packageName + "|" + entry.info.uid;
            setKey(key);
            UserManager mUserManager = context.getSystemService(UserManager.class);
            if (mUserManager != null) {
                UserInfo mUserInfo = mUserManager
                        .getUserInfo(UserHandle.getUserHandleForUid(entry.info.uid).getIdentifier());
                setTitle(mUserInfo != null ? mUserInfo.name + ":" + entry.label : entry.label);
            } else {
                setTitle(entry.label);
            }
            setIcon(entry.icon);
            setPersistent(false);
            setSummary(getAppStateText(entry.info));
            InstallAppsState extraInfo = (InstallAppsState)entry.extraInfo;
            setChecked(extraInfo.canInstallApps());
            mEntry = entry;
        }

        private String getAppStateText(ApplicationInfo info) {
            if ((info.flags & ApplicationInfo.FLAG_INSTALLED) == 0) {
                return getContext().getString(R.string.not_installed);
            } else if (!info.enabled
                    || info.enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_DISABLED_UNTIL_USED) {
                return getContext().getString(R.string.disabled);
            }
            return null;
        }

        public AppEntry getAppEntry() {
            return mEntry;
        }
    }

}
