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

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import androidx.leanback.preference.LeanbackPreferenceFragmentCompat;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceScreen;
import android.util.Log;
import android.view.Display;
import android.view.Display.Mode;
import android.view.WindowManager;
import android.os.SystemProperties;

import com.android.tv.settings.R;
import com.android.tv.settings.data.ConstData;
import java.util.Arrays;
import java.util.ArrayList;
import java.util.Optional;

public class DisplayFragment extends LeanbackPreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener {
    private static final String TAG = "DisplayFragment";
    private static final String KEY_UI_RESOLUTIONS = "ui_resolutions";
    private static final String KEY_PFC_UI_RESOLUTIONS = "pfc_ui_resolutions";
    public static final String KEY_DISPLAY_DEVICE_CATEGORY = "display_device_category";
    public static final String HDMI_PLUG_ACTION = "android.intent.action.HDMI_PLUGGED";
    public static final String PROP_UI_RESOLUTIONS = "vendor.hwc.enable_display_configs";
    public static final String PROP_HDMI_TV_EXIST_SIDE_BY_SIDE = "persist.sys.hdmi_tv_exist_side_by_side";
    private PreferenceScreen mPreferenceScreen;
    private static String mStrPlatform;
    private static boolean mIsUseDisplayd;

    private final static int TYPE_EXTERNAL = 2;

    /**
     * 原生标准显示管理接口,用于DRM显示相关
     */
    private DisplayManager mDisplayManager;
    /**
     * 插拔显示设备监听
     */
    private DisplayManager.DisplayListener mDisplayListener = new DisplayManager.DisplayListener() {

        @Override
        public void onDisplayAdded(int displayId) {
        }

        @Override
        public void onDisplayRemoved(int displayId) {

        }

        @Override
        public void onDisplayChanged(int displayId) {
            Log.i(TAG, "onDisplayChanged displayId = " + displayId);
            updateResolutionValue();
        }
    };

    private ListPreference mUiResolutions;
    private PreferenceCategory mPfcUiResolutions;

    /**
     * HDMI热插拔接收器
     */
    private HDMIReceiver mHdmiReceiver;
    private PreferenceCategory mDisplayDeviceCategory;

    public static DisplayFragment newInstance() {
        return new DisplayFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mIsUseDisplayd = false;//SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        if (!mIsUseDisplayd) {
            setPreferencesFromResource(R.xml.display_drm, null);
        }
        initData();
        rebuildView();
    }


    @Override
    public void onStart() {
        Log.i(TAG, "onStart");
        super.onStart();
    }

    @Override
    public void onResume() {
        Log.i(TAG, "onResume");
        super.onResume();
        updateResolutionValue();
        registerDisplayListener();
        registerHDMIReceiver();
    }

    @Override
    public void onPause() {
        Log.i(TAG, "onPause");
        super.onPause();
        unRegiserDisplayListener();
        unRegisterHDMIReceiver();
    }

    private void initData() {
        mDisplayManager = (DisplayManager) getActivity().getSystemService(Context.DISPLAY_SERVICE);
        mPreferenceScreen = getPreferenceScreen();
        mPfcUiResolutions = findPreference(KEY_PFC_UI_RESOLUTIONS);
        mDisplayDeviceCategory = (PreferenceCategory) findPreference(KEY_DISPLAY_DEVICE_CATEGORY);
        mDisplayManager = (DisplayManager) getActivity().getSystemService(Context.DISPLAY_SERVICE);
        mUiResolutions = (ListPreference) findPreference(KEY_UI_RESOLUTIONS);
        if (!SystemProperties.get(PROP_UI_RESOLUTIONS, "false").equals("true")) {
            getPreferenceScreen().removePreference(mPfcUiResolutions);
            getPreferenceScreen().removePreference(mUiResolutions);
        }
        mHdmiReceiver = new HDMIReceiver();
        Log.i(TAG, "screenTitle:" + mPreferenceScreen.getTitle());
        mUiResolutions.setOnPreferenceChangeListener(this);
    }

    public void updateResolutionValue() {
        String resolutionValue = null;
        resolutionValue = DrmDisplaySetting.getCurrentAndroidMode(mDisplayManager);
        Log.i(TAG, "resolutionValue = " + resolutionValue);
        List<String> modes = Arrays.asList(DrmDisplaySetting.getAndroidModes(mDisplayManager));
        Log.i(TAG, "setValueIndex modes.toString()= " + modes.toString());
        int index = modes.indexOf(resolutionValue);
        if (index < 0) {
            Log.w(TAG, "DeviceFragment - updateResolutionValue - warning index(" + index + ") < 0, set index = 0");
            index = 0;
        }
        Log.i(TAG, "mUiResolutions setValueIndex index= " + index);
        mUiResolutions.setValueIndex(index);
    }

    /**
     * 注册显示监听
     */
    private void registerDisplayListener() {
        mDisplayManager.registerDisplayListener(mDisplayListener, null);
    }

    /**
     * 取消显示监听
     */
    private void unRegiserDisplayListener() {
        mDisplayManager.unregisterDisplayListener(mDisplayListener);
    }

    /**
     * 注册HDMI接收器
     */
    private void registerHDMIReceiver() {
        IntentFilter filter = new IntentFilter(HDMI_PLUG_ACTION);
        getActivity().registerReceiver(mHdmiReceiver, filter);
    }


    /**
     * 取消注册HDMI接收器
     */
    private void unRegisterHDMIReceiver() {
        getActivity().unregisterReceiver(mHdmiReceiver);
    }

    /**
     * 重新构造页面
     */
    private void rebuildView() {
        mDisplayDeviceCategory.removeAll();
        List<DisplayInfo> displayInfos = getDisplayInfos();
        Log.i(TAG, "rebuildView->displayInfos:" + displayInfos);
        if (displayInfos.size() <= 0) {
            try {
                Log.i(TAG, "sleep for waiting native status update");
                Thread.sleep(1500);
                displayInfos = getDisplayInfos();
                Log.i(TAG, "after sleep rebuildView->displayInfos:" + displayInfos);
            } catch (InterruptedException e) {
            }
        }
        if (displayInfos.size() > 0) {
            for (DisplayInfo displayInfo : displayInfos) {
                mDisplayDeviceCategory.addPreference(createDisplayDevicePreference(displayInfo));
            }
        }
        for (String modeStr : DrmDisplaySetting.getAndroidModes(mDisplayManager)) {
            Log.i("ROCKCHIP", "convert mode = " + modeStr);
        }
        for (String indexStr : DrmDisplaySetting.getAndroidModesIndex(mDisplayManager)) {
            Log.i("ROCKCHIP", "convert mode index = " + indexStr);
        }
        mUiResolutions.setEntries(DrmDisplaySetting.getAndroidModes(mDisplayManager));
        mUiResolutions.setEntryValues(DrmDisplaySetting.getAndroidModesIndex(mDisplayManager));
        Display[] displays = mDisplayManager.getDisplays();
        for (Display display : displays) {
            Log.i("ROCKCHIP", "display = " + display.toString());
            Mode[] modes = display.getSupportedModes();
            for (Mode mode : modes) {
                Log.i("ROCKCHIP", "source mode = " + mode.toString());
            }
        }
    }

    /*
    * 获取显示设备信息
    */
    private Preference createDisplayDevicePreference(DisplayInfo mDisplayInfo) {
        Preference mDisplayPreference = new Preference(getPreferenceManager().getContext());
        mDisplayPreference.setKey(mDisplayInfo.getDescription());
        mDisplayPreference.setTitle(mDisplayInfo.getDescription());
        mDisplayPreference.setFragment(DeviceFragment.class.getName());
        mDisplayPreference.setOnPreferenceClickListener(this);
        mDisplayPreference.setEnabled(true);
        mDisplayPreference.setSelectable(true);
        return mDisplayPreference;
    }

    /**
     * 获取所有外接显示设备信息
     *
     * @param
     * @return
     */
    private List<DisplayInfo> getDisplayInfos() {
        List<DisplayInfo> displayInfos = new ArrayList<DisplayInfo>();
        mIsUseDisplayd = false;//SystemProperties.getBoolean("ro.rk.displayd.enable", true);
        Display[] displays = mDisplayManager.getDisplays();

        //filter DisplayInfo for rk3328
        List<DisplayInfo> tempDisplayInfo = DrmDisplaySetting.getDisplayInfoList();
        switch(SystemProperties.get("ro.board.platform", "")) {
            case "rk3328":
            case "rk3528":
                String isHdmiTvExistSideBySide = SystemProperties.get(PROP_HDMI_TV_EXIST_SIDE_BY_SIDE, "");
                if (isHdmiTvExistSideBySide != null && isHdmiTvExistSideBySide.equals("true")) {
                    Log.i(TAG, "HDMI & TV is exist side by side");
                } else {
                    Log.i(TAG, "HDMI & TV is not exist side by side");
                    Optional<DisplayInfo> hdmiDisplayInfo = tempDisplayInfo.stream()
                            .filter(display -> display.getType() == DrmDisplaySetting.DRM_MODE_CONNECTOR_HDMIA)
                            .findFirst();
                    Optional<DisplayInfo> tvDisplayInfo = tempDisplayInfo.stream()
                            .filter(display -> display.getType() == DrmDisplaySetting.DRM_MODE_CONNECTOR_TV)
                            .findFirst();
                    if (hdmiDisplayInfo.isPresent() && tvDisplayInfo.isPresent()) {
                        tempDisplayInfo.remove(tvDisplayInfo.get());
                    }
                }
                break;
            default:
                break;
        }

        if (!mIsUseDisplayd) {
            displayInfos.addAll(tempDisplayInfo);
        }
        return displayInfos;
    }


    /**
     * 反射调用相关方法
     *
     * @param object
     * @param methodName
     * @param parameterTypes
     * @param args
     * @return
     */
    private Object invokeMethod(Object object, String methodName, Class<?>[] parameterTypes, Object[] args) {
        Object result = null;
        try {
            Method method = object.getClass().getDeclaredMethod(methodName, parameterTypes);
            method.setAccessible(true);
            result = method.invoke(object, args);
        } catch (Exception e) {
            Log.i(TAG, "invokeMethod->exception:" + e);
        }
        return result;
    }


    /**
     * 转换显示接口
     */
    private void changeDisplayInterface(boolean isHDMIConnect) {
/*        mDisplayOutputManager = null;
        try {
            mDisplayOutputManager = new DisplayOutputManager();
        } catch (Exception e) {
            Log.i(TAG, "new DisplayOutputManger exception:" + e);
        }
        if (!isHDMIConnect) {
            mDisplayOutputManager.setInterface(mDisplayOutputManager.MAIN_DISPLAY, 1, true);
        }*/
    }


    /**
     * 显示设备插拔监听器
     *
     * @author GaoFei
     */
    class DisplayListener implements DisplayManager.DisplayListener {

        @Override
        public void onDisplayAdded(int displayId) {
            Log.i(TAG, "DisplayListener->onDisplayAdded");
            rebuildView();
        }

        @Override
        public void onDisplayRemoved(int displayId) {
            Log.i(TAG, "DisplayListener->onDisplayRemoved");
            rebuildView();
        }

        @Override
        public void onDisplayChanged(int displayId) {
            Log.i(TAG, "DisplayListener->onDisplayChanged");

        }

    }


    /**
     * HDMI 热插拔事件
     *
     * @author GaoFei
     */
    class HDMIReceiver extends BroadcastReceiver {

        @Override
        public void onReceive(Context context, Intent intent) {
            boolean state = intent.getBooleanExtra("state", true);
            //changeDisplayInterface(state);
            Log.i(TAG, "DisplayFragment.java HDMIReceiver->onReceive");
            DrmDisplaySetting.updateDisplayInfos();
            rebuildView();
        }

    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object obj) {
        if (preference == mUiResolutions) {
            int index = 0;
            try {
                index = Integer.parseInt((String) obj);
            } catch (Exception e) {
                //TODO: handle exception
                e.printStackTrace();
            }
            Log.i("ROCKCHIP", "setIndex = " + index);
            Display mDisplay = mDisplayManager.getDisplays()[0];
            if (mDisplay.getDisplayId() == 0) {
                android.os.SystemProperties.set("sys.display-0.mode", String.valueOf(index));
                WindowManager.LayoutParams layoutParams = getActivity().getWindow().getAttributes();
                layoutParams.preferredDisplayModeId = index;
                getActivity().getWindow().setAttributes(layoutParams);
            } else {
                /* if (android.os.SystemProperties.get("ro.board.platform", "").equals("rk356x")) {
                    if (mDisplay.getType() == TYPE_EXTERNAL) {
                        int mPhysicalDisplayId = Integer.valueOf(mDisplay.getUniqueId().split(":")[1]);
                        Log.i(TAG, "uniqueId = " + mDisplay.getUniqueId());
                        if (mPhysicalDisplayId == 1) {
                            android.os.SystemProperties.set("sys.display-1.mode", String.valueOf(index));
                        }
                        if (mPhysicalDisplayId == 2) {
                            android.os.SystemProperties.set("sys.display-2.mode", String.valueOf(index));
                        }
                    }
                } else {
                    if (mDisplay.getType() == TYPE_EXTERNAL) {
                        android.os.SystemProperties.set("sys.display-1.mode", String.valueOf(index));
                    }
                } */
            }
        }
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        Log.d(TAG, "onPreferenceClick preference = " + preference);
        List<DisplayInfo> mDisplayInfos = getDisplayInfos();
        for (DisplayInfo displayInfo : mDisplayInfos) {
            if (displayInfo.getDescription().equals(preference.getKey())) {
                Intent intent = new Intent();
                intent.putExtra(ConstData.IntentKey.DISPLAY_INFO, displayInfo);
                getActivity().setIntent(intent);
                return false;
            }
        }
        return true;
    }
}
