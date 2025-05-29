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
import static com.android.settings.display.PictureModePreferenceController.PROPERTY_PICTURE_MODE;
import static com.android.settings.DisplaySettings.PROPERTY_SHOW_PICTURE_SETTING;

public class DisplayHuePreferenceController extends SliderPreferenceController {

    private RkDisplayOutputManager mRkDisplayOutputManager;

    public DisplayHuePreferenceController(Context context, String key) {
        super(context, key);
        mRkDisplayOutputManager = new RkDisplayOutputManager();
    }

    @Override
    public int getAvailabilityStatus() {
        if ("true".equals(SystemProperties.get(PROPERTY_SHOW_PICTURE_SETTING))) {
            return SystemProperties.getInt(PROPERTY_PICTURE_MODE, 0) == 1 ? AVAILABLE : DISABLED_DEPENDENT_SETTING;
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
        return mRkDisplayOutputManager.getHue(0);
    }

    @Override
    public boolean setSliderPosition(int position) {
        Log.d("SettingDisplay", "setHue " + position);
        mRkDisplayOutputManager.setHue(0, position);
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
