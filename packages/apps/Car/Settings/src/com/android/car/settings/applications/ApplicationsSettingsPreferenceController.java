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

package com.android.car.settings.applications;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.pm.UserInfo;
import android.graphics.drawable.Drawable;
import android.os.UserHandle;
import android.os.UserManager;

import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiPreference;
import com.android.settingslib.applications.ApplicationsState;

import java.util.ArrayList;

/** Business logic which populates the applications in this setting. */
public class ApplicationsSettingsPreferenceController extends
        PreferenceController<PreferenceGroup> implements
        ApplicationListItemManager.AppListItemListener {

    private UserManager mUserManager;

    public ApplicationsSettingsPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mUserManager = context.getSystemService(UserManager.class);
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    public void onDataLoaded(ArrayList<ApplicationsState.AppEntry> apps) {
        getPreference().removeAll();
        for (ApplicationsState.AppEntry appEntry : apps) {
            UserInfo mUserInfo = mUserManager.getUserInfo(UserHandle.getUserHandleForUid(appEntry.info.uid).getIdentifier());
            getPreference().addPreference(
                    createPreference(mUserInfo.name + ":" + appEntry.label, appEntry.sizeStr, appEntry.icon,
                            appEntry.info.packageName, appEntry));
        }
    }

    private Preference createPreference(String title, String summary, Drawable icon,
            String packageName, ApplicationsState.AppEntry appEntry) {
        CarUiPreference preference = new CarUiPreference(getContext());
        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setIcon(icon);
        preference.setKey(packageName);
        preference.setOnPreferenceClickListener(p -> {
            ApplicationDetailsFragment mAppDetailsFragment = ApplicationDetailsFragment.getInstance(packageName);
            mAppDetailsFragment.getArguments().putInt(ApplicationDetailsFragment.EXTRA_APP_ENTRY_UID, appEntry.info.uid);
            mAppDetailsFragment.setAppEntry(appEntry);
            getFragmentController().launchFragment(mAppDetailsFragment);
            return true;
        });
        return preference;
    }
}
