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

package com.android.settings.display;

import static android.app.admin.DevicePolicyResources.Strings.Settings.DISABLED_BY_IT_ADMIN_TITLE;
import static android.provider.Settings.System.SCREEN_OFF_TIMEOUT;

import static com.android.settings.display.ScreenTimeoutSettings.FALLBACK_SCREEN_TIMEOUT_VALUE;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;

import androidx.annotation.Nullable;
import androidx.preference.Preference;

import com.android.settings.R;
import com.android.settings.core.BasePreferenceController;
import com.android.settingslib.RestrictedLockUtils;
import com.android.settingslib.RestrictedLockUtilsInternal;
import com.android.settingslib.RestrictedPreference;

/**
 * The controller of {@link ScreenTimeoutSettings}.
 */
public class ScreenTimeoutPreferenceController extends BasePreferenceController {
    public static String PREF_NAME = "screen_timeout";

    private final CharSequence[] mTimeoutEntries;
    private final CharSequence[] mTimeoutValues;

    public ScreenTimeoutPreferenceController(Context context, String key) {
        super(context, key);
        //-----------------------rk code----------
        CharSequence[] tempEntries = context.getResources().getStringArray(R.array.screen_timeout_entries);
        CharSequence[] tempValues = context.getResources().getStringArray(R.array.screen_timeout_values);
        mTimeoutEntries = new CharSequence[tempEntries.length + 1];
        mTimeoutEntries[0] = context.getResources().getString(R.string.dark_ui_auto_mode_never);
        System.arraycopy(tempEntries, 0, mTimeoutEntries, 1, tempEntries.length);
        mTimeoutValues = new CharSequence[tempValues.length + 1];
        mTimeoutValues[0] = String.valueOf(Integer.MAX_VALUE);
        System.arraycopy(tempValues, 0, mTimeoutValues, 1, tempValues.length);
        //----------------------------------------
    }

    @Override
    public int getAvailabilityStatus() {
        return AVAILABLE;
    }

    @Override
    public void updateState(Preference preference) {
        final long maxTimeout = getMaxScreenTimeout();
        //-----------------------rk code----------
        int startPos = 0;
        if (maxTimeout != Long.MAX_VALUE) {
            //fix set maximum time to lock test of CtsVerifier
            startPos = 1;
        }
        final RestrictedLockUtils.EnforcedAdmin admin = getPreferenceDisablingAdmin(maxTimeout, startPos);
        //----------------------------------------
        if (admin != null) {
            preference.setEnabled(false);
            preference.setSummary(mContext.getSystemService(DevicePolicyManager.class)
                    .getResources()
                    .getString(DISABLED_BY_IT_ADMIN_TITLE,
                            () -> mContext.getString(R.string.disabled_by_policy_title)));
            ((RestrictedPreference) preference).setDisabledByAdmin(admin);
        } else {
            preference.setSummary(getTimeoutSummary(maxTimeout));
        }
    }

    private CharSequence getTimeoutSummary(long maxTimeout) {
        final long currentTimeout = getCurrentScreenTimeout();
        //-----------------------rk code----------
        final CharSequence description = Integer.MAX_VALUE == currentTimeout? null : getTimeoutDescription(currentTimeout, maxTimeout);
        //----------------------------------------
        return description == null ? mContext.getString(
                R.string.screen_timeout_summary_not_set) : mContext.getString(
                R.string.screen_timeout_summary, description);
    }

    private Long getMaxScreenTimeout() {
        if (RestrictedLockUtilsInternal.checkIfMaximumTimeToLockIsSet(mContext) != null) {
            final DevicePolicyManager dpm = mContext.getSystemService(DevicePolicyManager.class);
            if (dpm != null) {
                return dpm.getMaximumTimeToLock(null /* admin */, UserHandle.myUserId());
            }
        }
        return Long.MAX_VALUE;
    }

    /**
     * Returns the admin that causes the preference to be disabled completely. This could be due to
     * either an admin that has set the {@link UserManager.DISALLOW_CONFIG_SCREEN_TIMEOUT}
     * restriction, or an admin that has set a very small MaximumTimeToLock timeout resulting in
     * no possible options for the user.
     */
    //-----------------------rk code----------
    private RestrictedLockUtils.EnforcedAdmin getPreferenceDisablingAdmin(long maxTimeout, int startPos) {
    //----------------------------------------
        final DevicePolicyManager dpm = mContext.getSystemService(DevicePolicyManager.class);
        RestrictedLockUtils.EnforcedAdmin admin = null;
        if (dpm != null) {
            admin = RestrictedLockUtilsInternal.checkIfRestrictionEnforced(
                    mContext, UserManager.DISALLOW_CONFIG_SCREEN_TIMEOUT,
                    UserHandle.myUserId());
            //-----------------------rk code----------
            if (admin == null && getLargestTimeout(maxTimeout, startPos) == null) {
            //----------------------------------------
                admin = RestrictedLockUtilsInternal.checkIfMaximumTimeToLockIsSet(mContext);
            }
        }
        return admin;
    }

    private long getCurrentScreenTimeout() {
        return Settings.System.getLong(mContext.getContentResolver(),
                SCREEN_OFF_TIMEOUT, FALLBACK_SCREEN_TIMEOUT_VALUE);
    }

    @Nullable
    private CharSequence getTimeoutDescription(long currentTimeout, long maxTimeout) {
        if (currentTimeout < 0 || mTimeoutEntries == null || mTimeoutValues == null
                || mTimeoutValues.length != mTimeoutEntries.length) {
            return null;
        }

        //-----------------------rk code----------
        if (currentTimeout == Integer.MAX_VALUE && maxTimeout != Long.MAX_VALUE) {
            return getLargestTimeout(maxTimeout, 1);
        }
        //----------------------------------------

        if (currentTimeout > maxTimeout) {
            // The selected time out value is longer than the max timeout allowed by the admin.
            // Select the largest value from the list by default.
            //-----------------------rk code----------
            return getLargestTimeout(maxTimeout, 0);
            //----------------------------------------
        } else {
            return getCurrentTimeout(currentTimeout);
        }
    }

    @Nullable
    private CharSequence getCurrentTimeout(long currentTimeout) {
        for (int i = 0; i < mTimeoutValues.length; i++) {
            if (currentTimeout == Long.parseLong(mTimeoutValues[i].toString())) {
                return mTimeoutEntries[i];
            }
        }
        return null;
    }

    @Nullable
    //-----------------------rk code----------
    private CharSequence getLargestTimeout(long maxTimeout, int startPos) {
    //----------------------------------------
        CharSequence largestTimeout = null;
        // The list of timeouts is sorted
        //-----------------------rk code----------
        for (int i = startPos; i < mTimeoutValues.length; ++i) {
        //----------------------------------------
            if (Long.parseLong(mTimeoutValues[i].toString()) <= maxTimeout) {
                largestTimeout = mTimeoutEntries[i];
            }
        }
        return largestTimeout;
    }
}
