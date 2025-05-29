/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.settings;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.os.Bundle;

import com.android.settings.dashboard.DashboardFragment;
import com.android.settings.display.BrightnessLevelPreferenceController;
import com.android.settings.display.CameraGesturePreferenceController;
import com.android.settings.display.LiftToWakePreferenceController;
import com.android.settings.display.ShowOperatorNamePreferenceController;
import com.android.settings.display.TapToWakePreferenceController;
import com.android.settings.display.ThemePreferenceController;
import com.android.settings.display.VrDisplayPreferenceController;
import com.android.settings.display.EyeCarePreferenceController;
import com.android.settings.display.EyeCaraGainPreferenceController;
import com.android.settings.display.EBookModePreferenceController;
import com.android.settings.display.PictureModePreferenceController;
import com.android.settings.display.DisplayHuePreferenceController;
import com.android.settings.display.DisplaySaturationPreferenceController;
import com.android.settings.display.DisplayContrastPreferenceController;
import com.android.settings.search.BaseSearchIndexProvider;
import com.android.settingslib.core.AbstractPreferenceController;
import com.android.settingslib.core.lifecycle.Lifecycle;
import com.android.settingslib.search.SearchIndexable;

import java.util.ArrayList;
import java.util.List;

//-----------------------rk code----------
import com.android.settings.display.HdmiSettingsPreferenceController;
//----------------------------------------

@SearchIndexable(forTarget = SearchIndexable.ALL & ~SearchIndexable.ARC)
public class DisplaySettings extends DashboardFragment {
    private static final String TAG = "DisplaySettings";

    //-----------------------rk code----------
    private static final String KET_HDMI_SETTINGS = "hdmi_settings";
    private static final String KET_EYE_CARE_SETTING = "eye_care_setting";
    private static final String KET_EYE_CARE_GAIN_SETTING = "eye_care_gain";
    private static final String KET_EBOOK_MODE_SETTING = "ebook_mode_setting";
    private static final String KET_PICTURE_MODE_SETTING = "picture_mode_setting";
    private static final String KET_DISPLAY_HUE_SETTING = "display_hue_setting";
    private static final String KET_DISPLAY_SATURATION_SETTING = "display_saturation_setting";
    private static final String KET_DISPLAY_CONTRAST_SETTING = "display_contrast_setting";
    public static final String PROPERTY_SHOW_PICTURE_SETTING = "ro.vendor.picture_settings";
    //----------------------------------------

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.DISPLAY;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.display_settings;
    }

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
    }

    @Override
    protected List<AbstractPreferenceController> createPreferenceControllers(Context context) {
        return buildPreferenceControllers(context, getSettingsLifecycle());
    }

    @Override
    public int getHelpResource() {
        return R.string.help_uri_display;
    }

    private static List<AbstractPreferenceController> buildPreferenceControllers(
            Context context, Lifecycle lifecycle) {
        final List<AbstractPreferenceController> controllers = new ArrayList<>();
        controllers.add(new CameraGesturePreferenceController(context));
        controllers.add(new LiftToWakePreferenceController(context));
        controllers.add(new TapToWakePreferenceController(context));
        controllers.add(new VrDisplayPreferenceController(context));
        controllers.add(new ShowOperatorNamePreferenceController(context));
        controllers.add(new ThemePreferenceController(context));
        controllers.add(new BrightnessLevelPreferenceController(context, lifecycle));
        //-----------------------rk code----------
        controllers.add(new HdmiSettingsPreferenceController(context, KET_HDMI_SETTINGS));
        controllers.add(new EyeCarePreferenceController(context, KET_EYE_CARE_SETTING));
        controllers.add(new EyeCaraGainPreferenceController(context, KET_EYE_CARE_GAIN_SETTING));
        controllers.add(new EBookModePreferenceController(context, KET_EBOOK_MODE_SETTING));
        controllers.add(new PictureModePreferenceController(context, KET_PICTURE_MODE_SETTING));
        controllers.add(new DisplayHuePreferenceController(context, KET_DISPLAY_HUE_SETTING));
        controllers.add(new DisplaySaturationPreferenceController(context, KET_DISPLAY_SATURATION_SETTING));
        controllers.add(new DisplayContrastPreferenceController(context, KET_DISPLAY_CONTRAST_SETTING));
        //----------------------------------------
        return controllers;
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(R.xml.display_settings) {

                @Override
                public List<AbstractPreferenceController> createPreferenceControllers(
                        Context context) {
                    return buildPreferenceControllers(context, null);
                }
            };
}
