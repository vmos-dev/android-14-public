/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.tv.settings.display;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.SystemProperties;
import android.os.ServiceManager;
import android.util.Log;
import android.view.Display.Mode;
import android.view.View;
import android.view.IWindowManager;
import android.view.WindowManager;
import android.view.Surface;
import android.widget.TextView;

import androidx.annotation.Keep;
import androidx.fragment.app.DialogFragment;
import androidx.preference.CheckBoxPreference;
import androidx.preference.SeekBarPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.settingslib.core.AbstractPreferenceController;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@Keep
public class AIDisplayFragment extends SettingsPreferenceFragment implements Preference.OnPreferenceChangeListener,
        Preference.OnPreferenceClickListener {
    protected static final String TAG = "AIDisplayFragment";

    // needed keys
    public static final String KEY_AI_SVEP_MODE = "ai_svep_mode";
    public static final String KEY_AI_SVEP_GLOBAL_UI = "ai_svep_global_ui";
    public static final String KEY_AI_SVEP_CONTRAST_MODE = "ai_svep_contrast_mode";
    public static final String KEY_AI_SVEP_CONTRAST_RATIO  = "ai_svep_contrast_ratio";
    public static final String KEY_AI_SVEP_ENHANCEMENT  = "ai_svep_enhance_rate";

    // needed properties
    public static final String PROP_AI_SVEP_MODE  = "persist.sys.svep.mode";
    public static final String PROP_AI_SVEP_CONTRAST_MODE  = "persist.sys.svep.contrast_mode";
    public static final String PROP_AI_SVEP_CONTRAST_RATIO  = "persist.sys.svep.contrast_offset_ratio";
    public static final String PROP_AI_SVEP_ENHANCE_RATE  = "persist.sys.svep.enhancement_rate";

    // needed preferences
    protected CheckBoxPreference mCheckBoxSvepMode;
    protected CheckBoxPreference mCheckBoxSvepContrastMode;
    protected CheckBoxPreference mCheckBoxSvepGobalUI;
    protected SeekBarPreference  mSeekBarSvepContrastRatio;
    protected SeekBarPreference  mSeekBarSvepEnhanceRate;

    public static AIDisplayFragment newInstance() {
        return new AIDisplayFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.ai_display_settings, null);
        initData();
        initEvent();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
    }

    @Override
    public void onResume() {
        super.onResume();
        updateAIlWidgets();
    }

    @Override
    public void onPause() {
        super.onPause();
    }

    private void initData() {
        mCheckBoxSvepMode = (CheckBoxPreference) findPreference(KEY_AI_SVEP_MODE);
        mCheckBoxSvepContrastMode = (CheckBoxPreference) findPreference(KEY_AI_SVEP_CONTRAST_MODE);
        mCheckBoxSvepGobalUI  = (CheckBoxPreference) findPreference(KEY_AI_SVEP_GLOBAL_UI);
        mSeekBarSvepContrastRatio  = (SeekBarPreference) findPreference(KEY_AI_SVEP_CONTRAST_RATIO);
        mSeekBarSvepEnhanceRate = (SeekBarPreference) findPreference(KEY_AI_SVEP_ENHANCEMENT);
    }

    private void initEvent() {
        mCheckBoxSvepMode.setOnPreferenceClickListener(this);
        mCheckBoxSvepGobalUI.setOnPreferenceClickListener(this);
        mCheckBoxSvepContrastMode.setOnPreferenceClickListener(this);
        mSeekBarSvepContrastRatio.setOnPreferenceChangeListener(this);
        mSeekBarSvepEnhanceRate.setOnPreferenceChangeListener(this);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object object) {
        int value = Integer.parseInt(object.toString());
        if (preference == mSeekBarSvepContrastRatio) {
            int ratio = Math.min(Math.max(value, 10), 90);    // range: 10~90, default: 50
            SystemProperties.set(PROP_AI_SVEP_CONTRAST_RATIO, String.valueOf(ratio));
        }
        if (preference == mSeekBarSvepEnhanceRate) {
            int enhancement = Math.min(Math.max(value, 0), 10);    // range: 0~10, default: 5
            SystemProperties.set(PROP_AI_SVEP_ENHANCE_RATE, String.valueOf(enhancement));
        }
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mCheckBoxSvepMode) {
            setAiImageSuperResolution(mCheckBoxSvepMode.isChecked());
        } else if (preference == mCheckBoxSvepContrastMode) {
            setAiImageSplitMode(mCheckBoxSvepContrastMode.isChecked());
        } else if (preference == mCheckBoxSvepGobalUI) {
            setGlobalUISvep(mCheckBoxSvepGobalUI.isChecked());
        }
        return true;
    }

    private void updateSeekBar(SeekBarPreference seekbar, int min, int max, int value) {
        seekbar.setMin(min);
        seekbar.setMax(max);
        seekbar.setValue(value);
    }

    private void setAiImageSuperResolution(boolean enabled) {
        SystemProperties.set(PROP_AI_SVEP_MODE, String.valueOf(enabled ? 1 : 0));
        updateAIlWidgets();
    }

    private void setAiImageSplitMode(boolean enabled) {
        SystemProperties.set(PROP_AI_SVEP_CONTRAST_MODE, String.valueOf(enabled ? 1 : 0));
        updateAIlWidgets();
    }

    private void setGlobalUISvep(boolean enabled) {
        SystemProperties.set(PROP_AI_SVEP_MODE, String.valueOf(enabled ? 2 : 1));
        updateAIlWidgets();
    }

    private void updateAIlWidgets() {
        updateAISvepProps();

        // update CheckBoxes.
        boolean status = false;
        status = SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) > 0;
        mCheckBoxSvepMode.setChecked(status);
        status = SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) == 2;
        mCheckBoxSvepGobalUI.setChecked(status);
        status = SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_MODE, 0) == 1;
        mCheckBoxSvepContrastMode.setChecked(status);

        // update SeekBars.
        status = SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_MODE, 0) > 0;
        mSeekBarSvepContrastRatio.setEnabled(status);
        this.updateSeekBar(mSeekBarSvepContrastRatio, 10, 90, 50);

        status = SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) > 0;
        mSeekBarSvepEnhanceRate.setEnabled(status);
        this.updateSeekBar(mSeekBarSvepEnhanceRate, 0, 10, 5);
    }

    private void updateAISvepProps() {
        // update SVEP properties.
        if ((SystemProperties.getInt(PROP_AI_SVEP_MODE, 0) == 0) && 
            (SystemProperties.getInt(PROP_AI_SVEP_CONTRAST_MODE, 0) == 1)) {
            SystemProperties.set(PROP_AI_SVEP_CONTRAST_MODE, String.valueOf(0));
        }  
    }
}
