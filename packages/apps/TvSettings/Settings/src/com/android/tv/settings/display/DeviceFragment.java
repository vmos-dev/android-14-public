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
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import androidx.preference.ListPreference;
import androidx.preference.CheckBoxPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import android.util.Log;
import android.view.Display.Mode;
import android.view.View;
import android.widget.TextView;
import android.os.SystemProperties;
import android.text.TextUtils;
import androidx.annotation.Keep;
import androidx.fragment.app.DialogFragment;

import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.ShowDialogService;
import com.android.settingslib.core.AbstractPreferenceController;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import android.view.IWindowManager;
import android.view.WindowManager;
import android.view.Surface;
import android.os.ServiceManager;


@Keep
public class DeviceFragment extends SettingsPreferenceFragment implements Preference.OnPreferenceChangeListener,
        Preference.OnPreferenceClickListener {
    protected static final String TAG = "DeviceFragment";
    public static final String KEY_RESOLUTION = "resolution";
    public static final String KEY_COLOR = "color";
    public static final String KEY_HDR10 = "hdr10";
    public static final String KEY_ZOOM = "zoom";
    public static final String KEY_FIXED_ROTATION = "fixed_rotation";
    public static final boolean FIXROTATION = false;
    public static final String KEY_ROTATION = "rotation";
    public static final String KEY_ADVANCED_SETTINGS = "advanced_settings";
    public static final String KEY_AI_DISPLAY_SETTINGS = "ai_display_settings";
    public static final String HDMI_PLUG_ACTION = "android.intent.action.HDMI_PLUGGED";

    public static final String KEY_AI_DISPLAY_DIM = "dim";

    protected PreferenceScreen mPreferenceScreen;
    /**
     * 分辨率设置
     */
    protected ListPreference mResolutionPreference;
    /**
     * 屏幕颜色率设置
     */
    protected ListPreference mColorPreference;
    /**
     * HDR10支持能力
     */
    protected CheckBoxPreference mHDR10Preference;
    /**
     * 缩放设置
     */
    protected Preference mZoomPreference;
    /**
     * 屏幕锁定设置
     */
    protected CheckBoxPreference mFixedRotationPreference;
    /**
     * 屏幕旋转设置
     */
    protected ListPreference mRotationPreference;
    /**
     * 高级设置
     */
    protected Preference mAdvancedSettingsPreference;

    /**
     * AI显示设置
     */
    protected Preference mAiDisplaySettingsPreference;

    /**
     * 当前显示设备对应的信息
     */
    protected DisplayInfo mDisplayInfo;
    /**
     * 标题
     */
    protected TextView mTextTitle;
    /**
     * 标识平台
     */
    protected String mStrPlatform;
    protected boolean mIsUseDisplayd;
    /**
     * 显示管理
     */
    protected DisplayManager mDisplayManager;
    /**
     * 窗口管理
     */
    protected WindowManager mWindowManager;

    private IWindowManager wm;

    private ScreenStateReceiver mScreenStateReceiver = new ScreenStateReceiver();

    class ScreenStateReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.d(TAG, "ScreenStateReceiver: action = " + action);
            if (action.equals(Intent.ACTION_SCREEN_ON)) {
                rebuildView();
                updateResolutionValue();
                updateColorValue();
                if (FIXROTATION)
                    updateRotation();
            } else if (action.equals(Intent.ACTION_SCREEN_OFF)) {

            }
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        IntentFilter mScreenIntentFilter = new IntentFilter(Intent.ACTION_SCREEN_ON);
        mScreenIntentFilter.addAction(Intent.ACTION_SCREEN_OFF);
        getActivity().registerReceiver(mScreenStateReceiver, mScreenIntentFilter);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.display_device, null);
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
        rebuildView();
        updateResolutionValue();
        updateColorValue();
        if (FIXROTATION)
            updateRotation();
    }


    @Override
    public void onPause() {
        super.onPause();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        getActivity().unregisterReceiver(mScreenStateReceiver);
    }

    protected void initData() {
        mStrPlatform = SystemProperties.get("ro.board.platform");
        mIsUseDisplayd = false;//SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        mDisplayManager = (DisplayManager) getActivity().getSystemService(Context.DISPLAY_SERVICE);
        mWindowManager = (WindowManager) getActivity().getSystemService(Context.WINDOW_SERVICE);
        wm = IWindowManager.Stub.asInterface(
                    ServiceManager.getService(Context.WINDOW_SERVICE));
        mPreferenceScreen = getPreferenceScreen();
        mAdvancedSettingsPreference = findPreference(KEY_ADVANCED_SETTINGS);
        mAiDisplaySettingsPreference = findPreference(KEY_AI_DISPLAY_SETTINGS);
        mResolutionPreference = (ListPreference) findPreference(KEY_RESOLUTION);
        mColorPreference = (ListPreference) findPreference(KEY_COLOR);
        mHDR10Preference = (CheckBoxPreference)findPreference(KEY_HDR10);

        if ("false".equals(SystemProperties.get("persist.sys.show_color_option", "false"))) {
            getPreferenceScreen().removePreference(mColorPreference);
            mColorPreference = null;
        }

        mZoomPreference = findPreference(KEY_ZOOM);
        if (FIXROTATION) {
            mFixedRotationPreference = (CheckBoxPreference) findPreference(KEY_FIXED_ROTATION);
            mRotationPreference = (ListPreference) findPreference(KEY_ROTATION);
        }
        mTextTitle = (TextView) getActivity().findViewById(androidx.preference.R.id.decor_title);
        if (!mIsUseDisplayd) {
            mDisplayInfo = getDisplayInfo();
        }

        if (FIXROTATION) {
            int fixedToUserRotationMode = getFixedToUserRotation(mDisplayInfo.getDisplayId());

            if (fixedToUserRotationMode != IWindowManager.FIXED_TO_USER_ROTATION_ENABLED
                /* || !("2".equals(SystemProperties.get("persist.sys.forced_orient", "0"))) */) {
                mPreferenceScreen.removePreference(mRotationPreference);
                mFixedRotationPreference.setChecked(false);
            } else {
                mFixedRotationPreference.setChecked(true);
            }
            if (mDisplayInfo.getDisplayId() != 0) {
                getPreferenceScreen().removePreference(mFixedRotationPreference);
                getPreferenceScreen().removePreference(mRotationPreference);
            }
        }

        if ("rk3588".equals(SystemProperties.get("ro.board.platform", ""))) {
            Log.d(TAG, "show AISettings");
        } else {
            getPreferenceScreen().removePreference(mAiDisplaySettingsPreference);
        }

        /* if (isSupportedHDR10()) {
            mHDR10Preference.setEnabled(true);
            if (DrmDisplaySetting.isHDR10Status()) {
                mHDR10Preference.setChecked(true);
            } else {
                mHDR10Preference.setChecked(false);
            }
        } else {
            mHDR10Preference.setEnabled(false);
        } */
        getPreferenceScreen().removePreference(mHDR10Preference);
    }

    protected void rebuildView() {
        if (mDisplayInfo == null)
            return;
        mResolutionPreference.setEntries(mDisplayInfo.getModes());
        mResolutionPreference.setEntryValues(mDisplayInfo.getModes());
        if (mColorPreference != null) {
            mColorPreference.setEntries(mDisplayInfo.getColors());
            mColorPreference.setEntryValues(mDisplayInfo.getColors());
        }
        if(mTextTitle!=null)
            mTextTitle.setText(mDisplayInfo.getDescription());
    }

    protected void initEvent() {
        mResolutionPreference.setOnPreferenceChangeListener(this);
        if (mColorPreference != null)
            mColorPreference.setOnPreferenceChangeListener(this);

        mZoomPreference.setOnPreferenceClickListener(this);
        if (FIXROTATION) {
            mRotationPreference.setOnPreferenceChangeListener(this);
            mFixedRotationPreference.setOnPreferenceClickListener(this);
        }
        mAdvancedSettingsPreference.setOnPreferenceClickListener(this);
        mAiDisplaySettingsPreference.setOnPreferenceClickListener(this);
        mHDR10Preference.setOnPreferenceClickListener(this);
    }

    public void updateRotation() {
        try {
            int rotation = mDisplayManager.getDisplay(mDisplayInfo.getDisplayId()).getRotation();
            switch (rotation) {
                case Surface.ROTATION_0:
                    mRotationPreference.setValue("0");
                    mRotationPreference.setSummary("0");
                    break;
                case Surface.ROTATION_90:
                    mRotationPreference.setValue("90");
                    mRotationPreference.setSummary("90");
                    break;
                case Surface.ROTATION_180:
                    mRotationPreference.setValue("180");
                    mRotationPreference.setSummary("180");
                    break;
                case Surface.ROTATION_270:
                    mRotationPreference.setValue("270");
                    mRotationPreference.setSummary("270");
                    break;
                default:
                    mRotationPreference.setValue("0");
                    mRotationPreference.setSummary("0");
            }
            // wm.freezeRotation(0);
        } catch (Exception e) {
            Log.e(TAG, e.toString());
        }
    }

    public void updateColorValue() {
        if (mDisplayInfo == null || mColorPreference == null)
            return;
        String curColorMode = DrmDisplaySetting.getColorMode(mDisplayInfo);
        Log.i(TAG, "curColorMode:" + curColorMode);
        if (!TextUtils.isEmpty(curColorMode)) {
            List<String> colors = DrmDisplaySetting.getColorModeList(mDisplayInfo);
            mColorPreference.setEntries(colors.toArray(new String[colors.size()]));
            mColorPreference.setEntryValues(colors.toArray(new String[colors.size()]));
            Log.i(TAG, "setValueIndex colors.toString()= " + colors.toString() + ", mColorPreference.getEntryValues().length = " + mColorPreference.getEntryValues().length + ", value = " + mColorPreference.getValue());
            int index = colors.indexOf(curColorMode);
            if (index < 0 || index >= mColorPreference.getEntryValues().length || index > colors.size()) {
                Log.i(TAG, "DeviceFragment - updateColorValue - warning index(" + index + ") < 0 || index > color.lenght(" + colors.size() + ") , set index = 0");
                index = 0;
                DrmDisplaySetting.setColorMode(mDisplayInfo.getDisplayId(), mDisplayInfo.getType(), colors.get(index));
            }
            Log.i(TAG, "updateColorValue setValueIndex index= " + index);
            mColorPreference.setValueIndex(index);
            mColorPreference.setValue(curColorMode);
            mColorPreference.setSummary(curColorMode);
        } else {
            Log.i(TAG, "curColorMode = null");
        }

    }

    /**
     * 还原分辨率值
     */
    public void updateResolutionValue() {
        if (mDisplayInfo == null)
            return;
        String resolutionValue = null;

        resolutionValue = DrmDisplaySetting.getCurDisplayMode(mDisplayInfo);
        /*防止读值不同步导致的UI值与实际设置的值不相符
        1.如DrmDisplaySetting.curSetHdmiMode 已赋值，且tmpSetHdmiMode为空，则从DrmDisplaySetting.curSetHdmiMode取上一次设置的值
        2.如DrmDisplaySetting.curSetHdmiMode 未赋值，且tmpSetHdmiMode为空，则getCurDisplayMode直接获取
        */
        Log.i(TAG, "drm resolutionValue:" + resolutionValue + ", curSetDisplayMode = " + DrmDisplaySetting.curSetDisplayMode + ", tmpSetDisplayMode = " + DrmDisplaySetting.tmpSetDisplayMode);
        if(DrmDisplaySetting.curSetDisplayMode !=null && DrmDisplaySetting.tmpSetDisplayMode == null)
            resolutionValue = DrmDisplaySetting.curSetDisplayMode;

        if (resolutionValue != null)
            mResolutionPreference.setValue(resolutionValue);
        /*show mResolutionPreference current item*/
        List<String> modes = DrmDisplaySetting.getDisplayModes(mDisplayInfo);
        Log.i(TAG, "setValueIndex modes.toString()= " + modes.toString());
        int index = modes.indexOf(resolutionValue);
        if (index < 0) {
            Log.w(TAG, "DeviceFragment - updateResolutionValue - warning index(" + index + ") < 0, set index = 0");
            index = 0;
        }
        Log.i(TAG, "mResolutionPreference setValueIndex index= " + index);
        mResolutionPreference.setValueIndex(index);
        mResolutionPreference.setSummary(mResolutionPreference.getEntry());

    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object obj) {
        Log.i(TAG, "onPreferenceChange:" + obj);
        if (preference == mResolutionPreference) {
            if (!mIsUseDisplayd) {
                int index = mResolutionPreference.findIndexOfValue((String) obj);
                DrmDisplaySetting.setDisplayModeTemp(mDisplayInfo, index);
                showConfirmSetModeDialog();
            }
        } else if (preference == mColorPreference) {
            DrmDisplaySetting.setColorMode(mDisplayInfo.getDisplayId(), mDisplayInfo.getType(), (String) obj);
        } else if (FIXROTATION && preference == mRotationPreference) {
            try {
                int value = Integer.parseInt((String) obj);
                Log.d(TAG,"freezeDisplayRotation~~~value:"+(String) obj);
                if(value == 0)
                    wm.freezeDisplayRotation(mDisplayInfo.getDisplayId(), Surface.ROTATION_0);
                else if(value == 90)
                    wm.freezeDisplayRotation(mDisplayInfo.getDisplayId(), Surface.ROTATION_90);
                else if(value == 180)
                    wm.freezeDisplayRotation(mDisplayInfo.getDisplayId(), Surface.ROTATION_180);
                else if(value == 270)
                    wm.freezeDisplayRotation(mDisplayInfo.getDisplayId(), Surface.ROTATION_270);
                else
                    return true;
                android.os.SystemProperties.set("sys.boot_completed", "1");
            } catch (Exception e) {
                Log.e(TAG, "freezeDisplayRotation error");
            }
        }
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference == mZoomPreference) {
            Intent screenScaleIntent = new Intent(getActivity(), ScreenScaleActivity.class);
            screenScaleIntent.putExtra(ConstData.IntentKey.PLATFORM, mStrPlatform);
            screenScaleIntent.putExtra(ConstData.IntentKey.DISPLAY_INFO, mDisplayInfo);
            startActivity(screenScaleIntent);
        } else if (preference == mResolutionPreference) {
            //updateResolutionValue();
        } else if (preference == mAdvancedSettingsPreference) {
            Intent advancedIntent = new Intent(getActivity(), AdvancedDisplaySettingsActivity.class);
            advancedIntent.putExtra(ConstData.IntentKey.DISPLAY_ID, mDisplayInfo.getDisplayId());
            startActivity(advancedIntent);
        } else if (preference == mAiDisplaySettingsPreference) {
            // Intent aiDisplaySettingsIntent = new Intent(getActivity(), ShowDialogService.class);
            // aiDisplaySettingsIntent.putExtra(ShowDialogService.KEY_DIALOG, ShowDialogService.VALUE_AI_LAB);
            // getActivity().startService(aiDisplaySettingsIntent);
            Intent aiDisplaySettingsIntent = new Intent(getActivity(), AIDisplayActivity.class);
            aiDisplaySettingsIntent.putExtra(KEY_AI_DISPLAY_DIM, true);
            startActivity(aiDisplaySettingsIntent);
        } else if (FIXROTATION && preference == mFixedRotationPreference) {
            boolean checked = mFixedRotationPreference.isChecked();
            int displayId = android.view.Display.DEFAULT_DISPLAY;
            try{
                if(checked){
                    wm.setFixedToUserRotation(mDisplayInfo.getDisplayId(), IWindowManager.FIXED_TO_USER_ROTATION_ENABLED);//enabled
                    int fixedToUserRotationMode = getFixedToUserRotation(mDisplayInfo.getDisplayId());
                    Log.i(TAG, "fixedToUserRotationMode = " + fixedToUserRotationMode);
                    if (fixedToUserRotationMode != IWindowManager.FIXED_TO_USER_ROTATION_ENABLED) {
                        mPreferenceScreen.removePreference(mRotationPreference);
                        mFixedRotationPreference.setChecked(false);
                    } else {
                        mFixedRotationPreference.setChecked(true);
                        mPreferenceScreen.addPreference(mRotationPreference);
                    }
                } else {
                    wm.setFixedToUserRotation(mDisplayInfo.getDisplayId(), IWindowManager.FIXED_TO_USER_ROTATION_DISABLED);//disabled
                    int fixedToUserRotationMode = getFixedToUserRotation(mDisplayInfo.getDisplayId());
                    Log.i(TAG, "fixedToUserRotationMode = " + fixedToUserRotationMode);
                    if (fixedToUserRotationMode != IWindowManager.FIXED_TO_USER_ROTATION_ENABLED) {
                        mPreferenceScreen.removePreference(mRotationPreference);
                        mFixedRotationPreference.setChecked(false);
                    } else {
                        mFixedRotationPreference.setChecked(true);
                        mPreferenceScreen.addPreference(mRotationPreference);
                    }
                }
            } catch (Exception e) {
                Log.e(TAG,"process Runtime error!!");
                e.printStackTrace();
            }
        } else if (preference == mHDR10Preference) {
            Log.d(TAG, "mHDR10Preference onPreferenceClick value = " + mHDR10Preference.isChecked());
            DrmDisplaySetting.setHDR10Enabled(mHDR10Preference.isChecked());
        }
        return true;
    }


    @SuppressLint("NewApi")
    protected void showConfirmSetModeDialog() {
        DialogFragment df = ConfirmSetModeDialogFragment.newInstance(mDisplayInfo, new ConfirmSetModeDialogFragment.OnDialogDismissListener() {
            @Override
            public void onDismiss(boolean isok) {
                Log.i(TAG, "showConfirmSetModeDialog->onDismiss->isok:" + isok);
                rebuildView();
                updateResolutionValue();
                updateColorValue();
            }
        });
        df.show(getFragmentManager(), "ConfirmDialog");
    }

    protected DisplayInfo getDisplayInfo() {
        Intent mIntent = getActivity().getIntent();
        if(mIntent != null) {
            mDisplayInfo = (DisplayInfo) mIntent.getSerializableExtra(ConstData.IntentKey.DISPLAY_INFO);
            Log.d(TAG, "getDisplayInfo mDisplayInfo = " + mDisplayInfo);
        }
        return mDisplayInfo;
    }

    private int getFixedToUserRotation(int displayId) {
        String cmd = "wm fixed-to-user-rotation -d " + displayId;
        Runtime mRuntime = Runtime.getRuntime();
        try {
            //Process中封装了返回的结果和执行错误的结果
            Process mProcess = mRuntime.exec(cmd);
            BufferedReader mReader = new BufferedReader(new InputStreamReader(mProcess.getInputStream()));
            StringBuffer mRespBuff = new StringBuffer();
            char[] buff = new char[1024];
            int ch = 0;
            while ((ch = mReader.read(buff)) != -1) {
                mRespBuff.append(buff, 0, ch);
            }
            mReader.close();
            if ("enabled".equals(mRespBuff.toString().trim())) {
                return IWindowManager.FIXED_TO_USER_ROTATION_ENABLED;
            } else if ("disabled".equals(mRespBuff.toString().trim())) {
                return IWindowManager.FIXED_TO_USER_ROTATION_DISABLED;
            }
        } catch (Exception e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        return IWindowManager.FIXED_TO_USER_ROTATION_DEFAULT;
    }

    private boolean isSupportedHDR10() {
        int isSupported = DrmDisplaySetting.getHdrResolutionSupport(mDisplayInfo.getDisplayId(), DrmDisplaySetting.getCurDisplayMode(mDisplayInfo));
        return isSupported >= 0 && (isSupported & DrmDisplaySetting.HDR10) > 0;
    }

}
