/*
 * Copyright (C) 2022 The Android Open Source Project
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

/* Author : Weng Tao */

package com.android.tv.settings.display;

import android.os.Bundle;
import android.os.SystemProperties;
import android.util.Log;
import androidx.annotation.Keep;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import java.util.Arrays;

import com.android.tv.settings.R;
import com.android.tv.settings.SettingsPreferenceFragment;

@Keep
public class HdrVividFragment extends SettingsPreferenceFragment implements Preference.OnPreferenceChangeListener,
        Preference.OnPreferenceClickListener {
    protected static final String TAG = "HdrVividFragment";

    private static final String MAX_BRIGHTNESS_FOR_SDR[] = { "100", "200", "300", "400" };
    private static final String MAX_BRIGHTNESS_FOR_HDR[] = { "1000", "500", "800", "1200" };

    private static final String PROP_VIVID_SYS_HDR_MODE = "persist.sys.vivid.hdr_mode";
    private static final String PROP_VIVID_SYS_HDR_MAX_BRIGHTNESS = "persist.sys.vivid.max_brightness";

    private static final int INDEX_VIVID_AUTO = 2;
    private static final int INDEX_VIVID_HDR = 1;
    private static final int INDEX_VIVID_SDR = 0;

    private static final String KEY_HDR_VIVID_MODE = "hdr_mode";
    private static final String KEY_HDR_VIVID_MAX_BRIGHTNESS = "vivid_max_brightness";

    private ListPreference mHdrVividMode;
    private ListPreference mMaxBrightnessForHdr;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.display_hdr_vivid, null);
        initView();
        initData();
    }

    @Override
    public void onResume() {
        super.onResume();
        initData();
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        return true;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object obj) {
        Log.i(TAG, "onPreferenceChange:" + obj);
        if (preference == mHdrVividMode) {
            setHDRVividMode((String) obj);
        } else if (preference == mMaxBrightnessForHdr) {
            setBrightness((String) obj);
        }
        initData();
        return true;
    }

    private void initView() {
        mHdrVividMode = (ListPreference) findPreference(KEY_HDR_VIVID_MODE);
        mHdrVividMode.setOnPreferenceChangeListener(this);

        mMaxBrightnessForHdr = (ListPreference) findPreference(KEY_HDR_VIVID_MAX_BRIGHTNESS);
        mMaxBrightnessForHdr.setOnPreferenceChangeListener(this);
    }

    private void initData() {
        mHdrVividMode.setValue(String.valueOf(getHDRVividMode()));
        mHdrVividMode.setSummary(mHdrVividMode.getEntry());
        if (getMaxBrightnessForHdr() != null) {
            mMaxBrightnessForHdr.setEnabled(true);
            mMaxBrightnessForHdr.setEntries(getMaxBrightnessForHdr());
            mMaxBrightnessForHdr.setEntryValues(getMaxBrightnessForHdr());
            mMaxBrightnessForHdr.setValue(String.valueOf(getCurrentBrightness()));
            mMaxBrightnessForHdr.setSummary(mMaxBrightnessForHdr.getEntry());
        } else {
            mMaxBrightnessForHdr.setEnabled(false);
        }
    }

    private int getHDRVividMode() {
        int mSupport = INDEX_VIVID_AUTO;
        try {
            mSupport = Integer.parseInt(DrmDisplaySetting.getHDRVividStatus());
        } catch (Exception e) {
            e.printStackTrace();
        }
        Log.i(TAG, "getHDRVividMode: " + mSupport);
        return mSupport;
    }

    private String[] getMaxBrightnessForHdr() {
        switch (getHDRVividMode()) {
            case INDEX_VIVID_SDR:
                return MAX_BRIGHTNESS_FOR_SDR;
            case INDEX_VIVID_HDR:
                return MAX_BRIGHTNESS_FOR_HDR;
            default:
                return null;
        }
    }

    private String getCurrentBrightness() {
        String result = DrmDisplaySetting.getHDRVividCurrentBrightness();
        switch(getHDRVividMode()) {
            case INDEX_VIVID_SDR:
                if (!Arrays.asList(MAX_BRIGHTNESS_FOR_SDR).contains(result)) {
                    result = MAX_BRIGHTNESS_FOR_SDR[0];
                    setBrightness(result);
                }
                break;
            case INDEX_VIVID_HDR:
                if (!Arrays.asList(MAX_BRIGHTNESS_FOR_HDR).contains(result)) {
                    result = MAX_BRIGHTNESS_FOR_HDR[0];
                    setBrightness(result);
                }
                break;
            default :
                result = MAX_BRIGHTNESS_FOR_SDR[0];
        }
        return result;
    }

    private void setHDRVividMode(String mode) {
        DrmDisplaySetting.setHDRVividEnabled(mode);
        // SystemProperties.set(PROP_VIVID_SYS_HDR_MODE, mode);
    }

    private void setBrightness(String selectBrightness) {
        DrmDisplaySetting.setHDRVividMaxBrightness(selectBrightness);
        // SystemProperties.set(PROP_VIVID_SYS_HDR_MAX_BRIGHTNESS, selectBrightness);
    }
}
