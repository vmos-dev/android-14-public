/*
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.task;


import static android.app.admin.DevicePolicyManager.DEVICE_OWNER_TYPE_FINANCED;

import static com.android.internal.util.Preconditions.checkNotNull;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;

import com.android.internal.annotations.VisibleForTesting;
import com.android.managedprovisioning.analytics.MetricsWriterFactory;
import com.android.managedprovisioning.analytics.ProvisioningAnalyticsTracker;
import com.android.managedprovisioning.common.ManagedProvisioningSharedPreferences;
import com.android.managedprovisioning.common.ProvisionLogger;
import com.android.managedprovisioning.common.SettingsFacade;
import com.android.managedprovisioning.common.Utils;
import com.android.managedprovisioning.model.ProvisioningParams;

/**
 * This tasks sets a given component as the device owner. It also enables the management
 * app if it's not currently enabled and sets the component as active admin.
 */
public class SetDeviceOwnerPolicyTask extends AbstractProvisioningTask {

    private final PackageManager mPackageManager;
    private final DevicePolicyManager mDevicePolicyManager;
    private final Utils mUtils;

    public SetDeviceOwnerPolicyTask(
            Context context,
            ProvisioningParams params,
            Callback callback) {
        this(new Utils(), context, params, callback,
                new ProvisioningAnalyticsTracker(
                        MetricsWriterFactory.getMetricsWriter(context, new SettingsFacade()),
                        new ManagedProvisioningSharedPreferences(context)));
    }

    @VisibleForTesting
    SetDeviceOwnerPolicyTask(Utils utils,
                        Context context,
                        ProvisioningParams params,
                        Callback callback,
                        ProvisioningAnalyticsTracker provisioningAnalyticsTracker) {
        super(context, params, callback, provisioningAnalyticsTracker);

        mUtils = checkNotNull(utils);
        mPackageManager = mContext.getPackageManager();
        mDevicePolicyManager = (DevicePolicyManager) context.getSystemService(
                Context.DEVICE_POLICY_SERVICE);
    }

    @Override
    public void run(int userId) {
        boolean success;
        try {
            ComponentName adminComponent =
                    mProvisioningParams.inferDeviceAdminComponentName(mUtils, mContext, userId);
            String adminPackage = adminComponent.getPackageName();

            enableDevicePolicyApp(adminPackage);
            setActiveAdmin(adminComponent, userId);
            success = setDeviceOwner(adminComponent, userId);

            if (success && mUtils.isFinancedDeviceAction(mProvisioningParams.provisioningAction)) {
                mDevicePolicyManager.setDeviceOwnerType(adminComponent, DEVICE_OWNER_TYPE_FINANCED);
            }
        } catch (Exception e) {
            ProvisionLogger.loge("Failure setting device owner", e);
            error(0);
            return;
        }

        if (success) {
            success();
        } else {
            ProvisionLogger.loge("Error when setting device owner.");
            error(0);
        }
    }

    private void enableDevicePolicyApp(String packageName) {
        int enabledSetting = mPackageManager.getApplicationEnabledSetting(packageName);
        if (enabledSetting != PackageManager.COMPONENT_ENABLED_STATE_DEFAULT
                && enabledSetting != PackageManager.COMPONENT_ENABLED_STATE_ENABLED) {
            mPackageManager.setApplicationEnabledSetting(packageName,
                    PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                    // Device policy app may have launched ManagedProvisioning, play nice and don't
                    // kill it as a side-effect of this call.
                    PackageManager.DONT_KILL_APP);
        }
    }

    private void setActiveAdmin(ComponentName component, int userId) {
        ProvisionLogger.logd("Setting " + component + " as active admin for user: " + userId);
        mDevicePolicyManager.setActiveAdmin(component, true, userId);
    }

    private boolean setDeviceOwner(ComponentName component, int userId) {
        ProvisionLogger.logd("Setting " + component + " as device owner of user " + userId);
        if (!component.equals(mUtils.getCurrentDeviceOwnerComponentName(mDevicePolicyManager))) {
            return mDevicePolicyManager.setDeviceOwner(component, userId);
        }
        return true;
    }
}
