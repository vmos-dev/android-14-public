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

package com.android.car.settings.display;

import static android.os.UserManager.DISALLOW_CONFIG_BRIGHTNESS;

import static com.android.car.settings.enterprise.ActionDisabledByAdminDialogFragment.DISABLED_BY_ADMIN_CONFIRM_DIALOG_TAG;
import static com.android.car.settings.enterprise.EnterpriseUtils.hasUserRestrictionByDpm;
import static com.android.car.settings.enterprise.EnterpriseUtils.hasUserRestrictionByUm;
import static com.android.settingslib.display.BrightnessUtils.GAMMA_SPACE_MAX;
import static com.android.settingslib.display.BrightnessUtils.convertGammaToLinear;
import static com.android.settingslib.display.BrightnessUtils.convertLinearToGamma;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.database.ContentObserver;
import android.hardware.display.DisplayManager;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.PowerManager;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.widget.Toast;

import androidx.annotation.VisibleForTesting;

import com.android.car.settings.CarSettingsApplication;
import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.car.settings.common.SeekBarPreference;
import com.android.car.settings.enterprise.EnterpriseUtils;
import com.android.internal.display.BrightnessSynchronizer;

/** Business logic for changing the brightness of the display. */
public class BrightnessLevelPreferenceController extends PreferenceController<SeekBarPreference> {

    private static final Logger LOG = new Logger(BrightnessLevelPreferenceController.class);
    private static final Uri BRIGHTNESS_URI = Settings.System.getUriFor(
            Settings.System.SCREEN_BRIGHTNESS);
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Context mContextForUser;
    private final Context mContext;
    private final boolean mIsVisibleBackgroundUsersSupported;
    private DisplayManager mDisplayManager;

    private final ContentObserver mBrightnessObserver = new ContentObserver(mHandler) {
        @Override
        public void onChange(boolean selfChange) {
            refreshUi();
        }
    };

    @VisibleForTesting
    final int mMaximumBacklight;
    @VisibleForTesting
    final int mMinimumBacklight;

    public BrightnessLevelPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);

        PowerManager powerManager = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
        mMaximumBacklight = powerManager.getMaximumScreenBrightnessSetting();
        mMinimumBacklight = powerManager.getMinimumScreenBrightnessSetting();
        mContext = context;
        mContextForUser = context
                .createContextAsUser(
                        UserHandle.of(UserHandle.myUserId()), /* flags= */ 0);
        UserManager userManager = context.getSystemService(UserManager.class);
        mIsVisibleBackgroundUsersSupported = userManager != null ? userManager.isVisibleBackgroundUsersSupported()
                : false;
        if (mIsVisibleBackgroundUsersSupported) {
            mDisplayManager = context.getSystemService(DisplayManager.class);
        }
    }

    @Override
    protected Class<SeekBarPreference> getPreferenceType() {
        return SeekBarPreference.class;
    }

    @Override
    protected void onCreateInternal() {
        super.onCreateInternal();
        setClickableWhileDisabled(getPreference(), /* clickable= */ true, p -> {
            if (hasUserRestrictionByDpm(getContext(), DISALLOW_CONFIG_BRIGHTNESS)) {
                showActionDisabledByAdminDialog();
            } else {
                Toast.makeText(getContext(),
                        getContext().getString(R.string.action_unavailable),
                        Toast.LENGTH_LONG).show();
            }
        });
    }

    @Override
    protected void onStartInternal() {
        super.onStartInternal();
        getContext().getContentResolver().registerContentObserver(BRIGHTNESS_URI,
                /* notifyForDescendants= */ false, mBrightnessObserver);
    }

    @Override
    protected void onStopInternal() {
        super.onStopInternal();
        getContext().getContentResolver().unregisterContentObserver(mBrightnessObserver);
    }

    @Override
    protected void updateState(SeekBarPreference preference) {
        preference.setMax(GAMMA_SPACE_MAX);
        preference.setValue(getSeekbarValue());
        preference.setContinuousUpdate(true);
    }

    @Override
    protected boolean handlePreferenceChanged(SeekBarPreference preference, Object newValue) {
        int gamma = (Integer) newValue;
        int linear = convertGammaToLinear(gamma, mMinimumBacklight, mMaximumBacklight);
        saveScreenBrightnessLinearValue(linear);
        return true;
    }

    private int getSeekbarValue() {
        int gamma = GAMMA_SPACE_MAX;
        if (mIsVisibleBackgroundUsersSupported) {
            if (mDisplayManager != null) {
                float linearFloat = mDisplayManager.getBrightness(getMyOccupantZoneDisplayId());
                int linear = BrightnessSynchronizer.brightnessFloatToInt(linearFloat);
                gamma = convertLinearToGamma(linear, mMinimumBacklight, mMaximumBacklight);
            }
        } else {
            try {
                int linear = getScreenBrightnessLinearValue();
                gamma = convertLinearToGamma(linear, mMinimumBacklight, mMaximumBacklight);
            } catch (Settings.SettingNotFoundException e) {
                LOG.w("Can't find setting for SCREEN_BRIGHTNESS.");
            }
        }
        return gamma;
    }

    @Override
    public int getDefaultAvailabilityStatus() {
        if (hasUserRestrictionByUm(getContext(), DISALLOW_CONFIG_BRIGHTNESS)
                || hasUserRestrictionByDpm(getContext(), DISALLOW_CONFIG_BRIGHTNESS)) {
            return AVAILABLE_FOR_VIEWING;
        }
        return AVAILABLE;
    }

    private void showActionDisabledByAdminDialog() {
        getFragmentController().showDialog(
                EnterpriseUtils.getActionDisabledByAdminDialog(getContext(),
                        DISALLOW_CONFIG_BRIGHTNESS),
                DISABLED_BY_ADMIN_CONFIRM_DIALOG_TAG);
    }

    @VisibleForTesting
    int getScreenBrightnessLinearValue() throws Settings.SettingNotFoundException {
        return Settings.System.getIntForUser(getContext().getContentResolver(),
                Settings.System.SCREEN_BRIGHTNESS, UserHandle.myUserId());
    }

    @VisibleForTesting
    void saveScreenBrightnessLinearValue(int linear) {
        if (mIsVisibleBackgroundUsersSupported) {
            if (mDisplayManager != null) {
                float linearFloat = BrightnessSynchronizer.brightnessIntToFloat(linear);
                mDisplayManager.setBrightness(getMyOccupantZoneDisplayId(), linearFloat);
            }
        } else {
            Settings.System.putIntForUser(getContext().getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS, linear, UserHandle.myUserId());
        }
    }

    private int getMyOccupantZoneDisplayId() {
        return ((CarSettingsApplication) mContext.getApplicationContext())
                .getMyOccupantZoneDisplayId();
    }
}
