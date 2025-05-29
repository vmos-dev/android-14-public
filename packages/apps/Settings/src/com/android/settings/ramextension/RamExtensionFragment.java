/*
 * Copyright 2023 Rockchip Electronics S.LSI Co. LTD
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

package com.android.settings.ramextension;

import android.app.ProgressDialog;
import android.content.Context;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.storage.StorageManager;
import android.text.format.Formatter;
import android.util.Log;
import android.widget.Toast;

import androidx.fragment.app.DialogFragment;
import androidx.preference.ListPreference;
import androidx.preference.Preference;

import com.android.settings.Utils;
import com.android.settings.dashboard.DashboardFragment;
import com.android.settings.R;
import com.android.settings.development.MemoryUsagePreferenceController;
import com.android.settingslib.core.AbstractPreferenceController;
import com.android.settingslib.deviceinfo.PrivateStorageInfo;
import com.android.settingslib.deviceinfo.StorageManagerVolumeProvider;
import com.android.settingslib.utils.ThreadUtils;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.List;

public class RamExtensionFragment extends DashboardFragment
        implements Preference.OnPreferenceChangeListener {
    private static final String TAG = "RamExtensionSettings";
    private static final String KEY_RAM_EXTENSION_LIST = "ram_extension_list";
    private static final long MIN_STORAGE_REMAIN = 1000000000;//1G

    private ListPreference mRamExtensionList;

    private Context mContext;
    private ProgressDialog mProgressDialog;
    private boolean mIsDestory;
    private StorageManager mStorageManager;
    private StorageManagerVolumeProvider mStorageManagerVolumeProvider;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mContext = getActivity();
        mStorageManager = mContext.getSystemService(StorageManager.class);
        mStorageManagerVolumeProvider = new StorageManagerVolumeProvider(mStorageManager);

        mRamExtensionList = (ListPreference) findPreference(KEY_RAM_EXTENSION_LIST);
        updateCurrentUI();
        mRamExtensionList.setOnPreferenceChangeListener(this);
    }

    @Override
    public int getMetricsCategory() {
        return 1000;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.ram_extension_settings;
    }

    @Override
    protected List<AbstractPreferenceController> createPreferenceControllers(Context context) {
        final List<AbstractPreferenceController> controllers = new ArrayList<>();

        controllers.add(new MemoryUsagePreferenceController(context));
        return controllers;
    }

    private void showConfirmSetDialog() {
        DialogFragment df = ConfirmDialogFragment.newInstance(new ConfirmDialogFragment.OnDialogDismissListener() {
            @Override
            public void onDismiss(boolean isok) {
                Log.i(TAG, "showConfirmSetDialog->onDismiss->isok:" + isok);
                if (isok) {
                    Utils.setRamExtensionValue(mRamExtensionList.getValue());
                    PowerManager powerManager = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
                    powerManager.reboot(null);
                    return;
                }
                updateCurrentUI();
            }
        });
        df.show(getFragmentManager(), "ConfirmDialog");
    }

    private void showWaitingDialog(int msgResId) {
        if (mIsDestory) {
            return;
        }
        if (null == mProgressDialog) {
            mProgressDialog = new ProgressDialog(mContext);
            mProgressDialog.setCanceledOnTouchOutside(false);
            mProgressDialog.setCancelable(false);
        }
        mProgressDialog.setMessage(getContext().getString(msgResId));
        if (!mProgressDialog.isShowing()) {
            mProgressDialog.show();
        }
    }

    private void hideWaitingDialog() {
        if (null != mProgressDialog && mProgressDialog.isShowing()) {
            mProgressDialog.cancel();
            mProgressDialog = null;
        }
    }

    private void updateCurrentUI() {
        String value = Utils.getRamExtensionValue();
        mRamExtensionList.setValue(value);
        mRamExtensionList.setSummary(Utils.getRamExtensionSummary(getContext(), value));
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mIsDestory = true;
        hideWaitingDialog();
    }

    @Override
    public boolean onPreferenceChange(final Preference preference, final Object obj) {
        String key = preference.getKey();
        Log.i(TAG, key + " onPreferenceChange:" + obj);
        if (KEY_RAM_EXTENSION_LIST.equals(key)) {
            setNewValue(obj.toString());
        }
        return true;
    }

    private void setNewValue(String value) {
        String strLastRamExtensionValue = Utils.getRamExtensionValue();
        long lastRamExtensionValue = 0;
        if (!Utils.isNoneRamExtensionValue(mContext, strLastRamExtensionValue)) {
            lastRamExtensionValue = Long.parseLong(strLastRamExtensionValue);
        }
        long newRamExtensionValue = 0;
        if (!Utils.isNoneRamExtensionValue(mContext, value)) {
            newRamExtensionValue = Long.parseLong(value);
        }
        final long dValue = newRamExtensionValue - lastRamExtensionValue;
        if (dValue > 0) {
            showWaitingDialog(R.string.ram_extension);
            ThreadUtils.postOnBackgroundThread(() -> {
                final PrivateStorageInfo info = PrivateStorageInfo.getPrivateStorageInfo(
                        mStorageManagerVolumeProvider);
                ThreadUtils.postOnMainThread(() -> {
                    if (mIsDestory) {
                        return;
                    }
                    hideWaitingDialog();
                    Log.w(TAG, "storage freebytes=" + info.freeBytes + ", dValue=" + dValue);
                    if (info.freeBytes - dValue > MIN_STORAGE_REMAIN) {
                        showConfirmSetDialog();
                    } else {
                        updateCurrentUI();
                        Toast.makeText(mContext,
                                mContext.getResources().getString(R.string.ram_extension_remain_warn,
                                        Formatter.formatFileSize(mContext, MIN_STORAGE_REMAIN)),
                                Toast.LENGTH_SHORT).show();
                    }
                });
            });
        } else {
            showConfirmSetDialog();
        }
    }

}