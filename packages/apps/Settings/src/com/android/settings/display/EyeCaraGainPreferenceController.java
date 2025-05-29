/*
 * Copyright (C) 2023 The Android Open Source Project
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

import android.content.Context;
import android.text.TextUtils;
import android.util.Log;
import android.os.RkDisplayOutputManager;
import android.os.SystemProperties;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.android.settings.R;
import com.android.settings.core.SliderPreferenceController;
import com.android.settings.widget.SeekBarPreference;
import static com.android.settings.display.EyeCarePreferenceController.PROPERTY_EYE_CARE_MODE;
import static com.android.settings.DisplaySettings.PROPERTY_SHOW_PICTURE_SETTING;

public class EyeCaraGainPreferenceController extends SliderPreferenceController {

    private RkDisplayOutputManager mRkDisplayOutputManager;

    public EyeCaraGainPreferenceController(Context context, String key) {
        super(context, key);
        mRkDisplayOutputManager = new RkDisplayOutputManager();
    }

    @Override
    public int getAvailabilityStatus() {
        if ("true".equals(SystemProperties.get(PROPERTY_SHOW_PICTURE_SETTING))) {
            return SystemProperties.getInt(PROPERTY_EYE_CARE_MODE, 0) == 1 ? AVAILABLE : DISABLED_DEPENDENT_SETTING;
        } else {
            return UNSUPPORTED_ON_DEVICE;
        }
    }

    @Override
    public boolean isSliceable() {
        return true;
    }

    @Override
    public boolean isPublicSlice() {
        return true;
    }

    @Override
    public int getSliceHighlightMenuRes() {
        return R.string.menu_key_display;
    }

    @Override
    public int getSliderPosition() {
        return 0;
    }

    @Override
    public boolean setSliderPosition(int position) {
        position = 100 - position;
        int rgain, ggain, bgain;
        rgain = 256;
        ggain = (int)(256 * (0.8 + 0.0941 * position / 100));
        bgain = (int)(256 * (0.49 + 0.126 * position / 100));
        Log.d("EyeCaraGain", "rgain " + rgain + " ggain " + ggain + " bgain " + bgain);

        int[][] rgb;
        rgb = ColorTempUtil.gammaColorTempAdjust(rgain, ggain, bgain);
        mRkDisplayOutputManager.setGamma(0, 1024, rgb[0], rgb[1], rgb[2]);
        return true;
    }

    @Override
    public int getMax() {
        return 100;
    }

    @Override
    public int getMin() {
        return 0;
    }
}
