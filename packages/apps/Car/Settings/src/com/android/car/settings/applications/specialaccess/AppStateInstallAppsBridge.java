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
import android.app.AppOpsManager;
import android.app.AppGlobals;
import android.content.Context;
import android.content.pm.IPackageManager;
import android.content.pm.PackageManager;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Log;

import com.android.internal.util.ArrayUtils;
import com.android.settingslib.applications.ApplicationsState;
import com.android.settingslib.applications.ApplicationsState.AppEntry;
import com.android.settingslib.applications.ApplicationsState.AppFilter;

import java.util.List;

/**
 * Bridges the value of {@link SmsManager#getPremiumSmsConsent(String)} into the {@link
 * ApplicationsState.AppEntry#extraInfo} for each entry's package name.
 */
public class AppStateInstallAppsBridge implements AppEntryListManager.ExtraInfoBridge {

    private static final String TAG = AppStateInstallAppsBridge.class.getSimpleName();

    private final IPackageManager mIpm;
    private final AppOpsManager mAppOpsManager;

    public AppStateInstallAppsBridge(AppOpsManager appOpsManager, IPackageManager ipm) {
        mIpm = AppGlobals.getPackageManager();
        mAppOpsManager = appOpsManager;
    }

    @Override
    public void loadExtraInfo(List<ApplicationsState.AppEntry> entries) {
        for (ApplicationsState.AppEntry entry : entries) {
            entry.extraInfo = createInstallAppsStateFor(entry.info.packageName, entry.info.uid);
        }
    }

    private boolean hasRequestedAppOpPermission(String permission, String packageName, int userId) {
        try {
            String[] packages = mIpm.getAppOpPermissionPackages(permission, userId);
            return ArrayUtils.contains(packages, packageName);
        } catch (RemoteException exc) {
            Log.e(TAG, "PackageManager dead. Cannot get permission info");
            return false;
        }
    }

    private boolean hasPermission(String permission, int uid) {
        try {
            int result = mIpm.checkUidPermission(permission, uid);
            return result == PackageManager.PERMISSION_GRANTED;
        } catch (RemoteException e) {
            Log.e(TAG, "PackageManager dead. Cannot get permission info");
            return false;
        }
    }

    private int getAppOpMode(int appOpCode, int uid, String packageName) {
        return mAppOpsManager.checkOpNoThrow(appOpCode, uid, packageName);
    }

    public InstallAppsState createInstallAppsStateFor(String packageName, int uid) {
        final InstallAppsState appState = new InstallAppsState();
        final int userId = UserHandle.getUserId(uid);
        appState.permissionRequested = hasRequestedAppOpPermission(
                Manifest.permission.REQUEST_INSTALL_PACKAGES, packageName, userId);
        appState.appOpMode = getAppOpMode(AppOpsManager.OP_REQUEST_INSTALL_PACKAGES, uid,
                packageName);
        return appState;
    }

    /**
     * Collection of information to be used as {@link AppEntry#extraInfo} objects
     */
    public static class InstallAppsState {
        boolean permissionRequested;
        int appOpMode;

        public InstallAppsState() {
            this.appOpMode = AppOpsManager.MODE_DEFAULT;
        }

        public boolean canInstallApps() {
            return appOpMode == AppOpsManager.MODE_ALLOWED;
        }

        public boolean isPotentialAppSource() {
            return appOpMode != AppOpsManager.MODE_DEFAULT || permissionRequested;
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("[permissionRequested: " + permissionRequested);
            sb.append(", appOpMode: " + appOpMode);
            sb.append("]");
            return sb.toString();
        }
    }
}
