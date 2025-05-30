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

package com.android.car.settings.home;

import android.app.ActivityManager;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserManager;
import android.provider.Settings;
import android.util.Log;
import android.view.Display;

import androidx.annotation.NonNull;
import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.NavButtonMode;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.Collections;
import java.util.List;

/**
 * Homepage for settings for car.
 */
public class HomepageFragment extends SettingsFragment {
    private static final int REQUEST_CODE = 501;

    private MenuItem mSearchButton;
    private UserManager mUserManager;

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        mUserManager = (UserManager) getContext().getSystemService(UserManager.class);
        if (mUserManager.isSystemUser() || mUserManager.isAdminUser()) {
            return R.xml.homepage_fragment;
        } else {
            return R.xml.homepage_fragment_no_manager;
        }
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        // TODO: Re-enable suggestions once more use cases are supported.
        // use(SuggestionsPreferenceController.class, R.string.pk_suggestions).setLoaderManager(
        //        LoaderManager.getInstance(/* owner= */ this));
    }

    @Override
    protected List<MenuItem> getToolbarMenuItems() {
        if (mSearchButton ==  null) {
            return null;
        }
        return Collections.singletonList(mSearchButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mUserManager = (UserManager) getContext().getSystemService(UserManager.class);
        if (mUserManager.isSystemUser() || mUserManager.isAdminUser()) {
            mSearchButton = new MenuItem.Builder(getContext())
                    .setToSearch()
                    .setOnClickListener(i -> onSearchButtonClicked())
                    .setUxRestrictions(CarUxRestrictions.UX_RESTRICTIONS_NO_KEYBOARD)
                    .build();
        } else {
            Log.d("HomepageFragment", " current User = " + mUserManager.getUserHandle());
        }
    }

    @Override
    protected void setupToolbar(@NonNull ToolbarController toolbar) {
        super.setupToolbar(toolbar);
        toolbar.setNavButtonMode(NavButtonMode.BACK);
    }

    private void onSearchButtonClicked() {
        Intent intent = new Intent(Settings.ACTION_APP_SEARCH_SETTINGS)
                .setPackage(getSettingsIntelligencePkgName(getContext()));
        if (intent.resolveActivity(getContext().getPackageManager()) == null) {
            return;
        }
        startActivityForResult(intent, REQUEST_CODE);
    }

    private String getSettingsIntelligencePkgName(Context context) {
        return context.getString(R.string.config_settingsintelligence_package_name);
    }

}
