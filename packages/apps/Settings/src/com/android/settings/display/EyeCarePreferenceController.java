/*
 * Copyright (C) 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
package com.android.settings.display;

import android.app.ActivityManager;
import android.content.Context;
import android.os.IBinder;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemProperties;
import android.os.RkDisplayOutputManager;
import android.util.Log;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;
import com.android.settings.widget.SeekBarPreference;
import com.android.settings.R;
import com.android.settings.core.TogglePreferenceController;
import com.android.settingslib.core.lifecycle.LifecycleObserver;
import static com.android.settings.display.EBookModePreferenceController.PROPERTY_EBOOK_MODE;
import static com.android.settings.display.PictureModePreferenceController.PROPERTY_PICTURE_MODE;
import static com.android.settings.DisplaySettings.PROPERTY_SHOW_PICTURE_SETTING;

public class EyeCarePreferenceController extends TogglePreferenceController
    implements LifecycleObserver {

    public static final String PROPERTY_EYE_CARE_MODE = "persist.sys.eyecare.mode.enable";

    private SwitchPreference mPictureModePreference;
    private SwitchPreference mEBookPreference;
    private SeekBarPreference mEyeCareGainPreference;
    private RkDisplayOutputManager mRkDisplayOutputManager;

    public EyeCarePreferenceController(Context context, String key) {
        super(context, key);
        mRkDisplayOutputManager = new RkDisplayOutputManager();
    }

    @Override
    public int getAvailabilityStatus() {
        if ("true".equals(SystemProperties.get(PROPERTY_SHOW_PICTURE_SETTING))) {
            return AVAILABLE;
        } else {
            return UNSUPPORTED_ON_DEVICE;
        }
    }

    @Override
    public boolean isChecked() {
        return SystemProperties.getInt(PROPERTY_EYE_CARE_MODE, 0) == 1;
    }

    @Override
    public boolean setChecked(boolean isChecked) {
        int[][] rgb;
        if(isChecked) {
            SystemProperties.set(PROPERTY_EBOOK_MODE, "0");
            SystemProperties.set(PROPERTY_PICTURE_MODE, "0");
            mEBookPreference.setChecked(false);
            mPictureModePreference.setChecked(false);
            mEyeCareGainPreference.setProgress(100);
            rgb = ColorTempUtil.gammaColorTempAdjust(256, 204, 125);
            SystemProperties.set(PROPERTY_EYE_CARE_MODE, "1");
        } else {
            SystemProperties.set(PROPERTY_EYE_CARE_MODE, "0");
            rgb = ColorTempUtil.gammaColorTempAdjust(256, 256, 256);

        }
        mRkDisplayOutputManager.setGamma(0, 1024, rgb[0], rgb[1], rgb[2]);
        mEyeCareGainPreference.setEnabled(isChecked);
        return true;
    }

    @Override
    public int getSliceHighlightMenuRes() {
        return R.string.menu_key_display;
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        mEyeCareGainPreference = (SeekBarPreference)screen.findPreference("eye_care_gain");
        mEBookPreference = (SwitchPreference)screen.findPreference("ebook_mode_setting");
        mPictureModePreference = (SwitchPreference)screen.findPreference("picture_mode_setting");
    }
}
