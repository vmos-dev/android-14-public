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

package com.android.car.settings.system;

import android.content.Context;
import android.os.UserManager;
import android.view.Display;

import com.android.car.settings.R;
import com.android.car.settings.common.CarSettingActivities.SystemSettingsActivity;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.development.DevelopmentSettingsUtil;
import com.android.car.settings.search.CarBaseSearchIndexProvider;
import com.android.settingslib.search.SearchIndexable;

import java.util.List;

/**
 * Shows basic info about the system and provide some actions like update, reset etc.
 */
@SearchIndexable
public class SystemSettingsFragment extends SettingsFragment {
    private UserManager mUserManager;

    @Override
    protected int getPreferenceScreenResId() {
        mUserManager = getContext().getSystemService(UserManager.class);
        if (mUserManager.isAdminUser() || mUserManager.isSystemUser()) {
            return R.xml.system_settings_fragment;
        } else {
            return R.xml.system_settings_fragment_no_admin;
        }
    }

    /**
     * Data provider for Settings Search.
     */
    public static final CarBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new CarBaseSearchIndexProvider(R.xml.system_settings_fragment,
                    SystemSettingsActivity.class) {
                @Override
                public List<String> getNonIndexableKeys(Context context) {
                    List<String> nonIndexableKeys = super.getNonIndexableKeys(context);
                    if (!DevelopmentSettingsUtil.isDevelopmentSettingsEnabled(context,
                            UserManager.get(context))) {
                        nonIndexableKeys.add(
                                context.getString(R.string.pk_developer_options_entry));
                    }
                    return nonIndexableKeys;
                }
            };
}
