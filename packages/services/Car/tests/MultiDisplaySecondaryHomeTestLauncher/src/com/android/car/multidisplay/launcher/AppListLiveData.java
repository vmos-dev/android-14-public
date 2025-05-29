/**
 * Copyright (c) 2018 The Android Open Source Project
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

package com.android.car.multidisplay.launcher;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.AsyncTask;
import android.os.UserHandle;
import android.util.Log;

import androidx.lifecycle.LiveData;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

class AppListLiveData extends LiveData<List<AppEntry>> {
    private static final String TAG = AppListLiveData.class.getSimpleName();

    private final PackageManager mPackageManager;
    private int mCurrentDataVersion;
    private int mAssignedUserId = UserHandle.USER_SYSTEM;

    AppListLiveData(Context context) {
        mPackageManager = context.getPackageManager();
        loadData();
    }

    AppListLiveData(Context context, int assignedUserId) {
        mPackageManager = context.getPackageManager();
        mAssignedUserId = assignedUserId;
        loadData();
    }

    void loadData() {
        final int loadDataVersion = ++mCurrentDataVersion;

        new AsyncTask<Void, Void, List<AppEntry>>() {
            @Override
            protected List<AppEntry> doInBackground(Void... voids) {
                Intent mainIntent = new Intent(Intent.ACTION_MAIN, null);
                mainIntent.addCategory(Intent.CATEGORY_LAUNCHER);

                List<ResolveInfo> apps = mPackageManager.queryIntentActivitiesAsUser(mainIntent,
                        PackageManager.GET_META_DATA, mAssignedUserId);

                List<AppEntry> entries = new ArrayList<>();
                if (apps != null) {
                    for (ResolveInfo app : apps) {
                        AppEntry entry = new AppEntry(app, mPackageManager);
                        entries.add(entry);
                    }
                    Log.d(TAG, "mAssignedUserId = " + mAssignedUserId + ", entries = "
                            + entries.toString());
                }
                Collections.sort(entries, AppEntry.LABEL_COMPARATOR);
                return entries;
            }

            @Override
            protected void onPostExecute(List<AppEntry> data) {
                if (mCurrentDataVersion == loadDataVersion) {
                    setValue(data);
                }
            }
        }.execute();
    }
}
