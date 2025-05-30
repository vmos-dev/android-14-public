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

package com.android.tv.settings.system.development;

import static android.view.CrossWindowBlurListeners.CROSS_WINDOW_BLUR_SUPPORTED;

import static com.android.tv.settings.overlay.FlavorUtils.X_EXPERIENCE_FLAVORS_MASK;

import android.Manifest;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.AppOpsManager;
import android.app.admin.DevicePolicyManager;
import android.app.backup.IBackupManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.database.ContentObserver;
import android.hardware.usb.UsbManager;
import android.media.MediaRecorder.AudioSource;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.NetworkRequest;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Parcel;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.StrictMode;
import android.os.SystemProperties;
import android.os.UserManager;
import android.provider.Settings;
import android.sysprop.AdbProperties;
import android.sysprop.DisplayProperties;
import android.text.TextUtils;
import android.util.Log;
import android.util.Pair;
import android.view.IWindowManager;
import android.view.LayoutInflater;
import android.view.ThreadedRenderer;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.widget.Toast;

import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;
import androidx.preference.PreferenceScreen;
import androidx.preference.SwitchPreference;

import com.android.internal.app.LocalePicker;
import com.android.settingslib.core.ConfirmationDialogController;
import com.android.settingslib.development.DevelopmentSettingsEnabler;
import com.android.settingslib.development.SystemPropPoker;
import com.android.tv.settings.dialog.UsbModeSettings;
import com.android.tv.settings.R;
import com.android.tv.settings.RestrictedPreferenceAdapter;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.overlay.FlavorUtils;
import com.android.tv.settings.system.development.audio.AudioDebug;
import com.android.tv.settings.system.development.audio.AudioMetrics;
import com.android.tv.settings.system.development.audio.AudioReaderException;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Optional;

/**
 * Displays preferences for application developers.
 */
public class DevelopmentFragment extends SettingsPreferenceFragment
        implements Preference.OnPreferenceChangeListener,
        EnableDevelopmentDialog.Callback, OemUnlockDialog.Callback, AdbDialog.Callback {
    private static final String TAG = "DevelopmentSettings";

    private static final String ENABLE_DEVELOPER = "development_settings_enable";
    private static final String ENABLE_ADB = "enable_adb";
    private static final String CLEAR_ADB_KEYS = "clear_adb_keys";
    private static final String ENABLE_TERMINAL = "enable_terminal";
    private static final String KEEP_SCREEN_ON = "keep_screen_on";
    private static final String BT_HCI_SNOOP_LOG = "bt_hci_snoop_log";
    private static final String BTSNOOP_LOG_MODE_PROPERTY = "persist.bluetooth.btsnooplogmode";
    private static final String ENABLE_OEM_UNLOCK = "oem_unlock_enable";
    private static final String HDCP_CHECKING_KEY = "hdcp_checking";
    private static final String HDCP_CHECKING_PROPERTY = "persist.sys.hdcp_checking";
    private static final String LOCAL_BACKUP_PASSWORD = "local_backup_password";
    private static final String BUGREPORT = "bugreport";
    private static final String BUGREPORT_IN_POWER_KEY = "bugreport_in_power";
    private static final String RUNNING_APPS = "running_apps";

    private static final String DEBUG_APP_KEY = "debug_app";
    private static final String WAIT_FOR_DEBUGGER_KEY = "wait_for_debugger";
    private static final String MOCK_LOCATION_APP_KEY = "mock_location_app";
    private static final String VERIFY_APPS_OVER_USB_KEY = "verify_apps_over_usb";
    private static final String DEBUG_VIEW_ATTRIBUTES = "debug_view_attributes";
    private static final String FORCE_ALLOW_ON_EXTERNAL_KEY = "force_allow_on_external";
    private static final String STRICT_MODE_KEY = "strict_mode";
    private static final String POINTER_LOCATION_KEY = "pointer_location";
    private static final String SHOW_TOUCHES_KEY = "show_touches";
    private static final String SHOW_SCREEN_UPDATES_KEY = "show_screen_updates";
    private static final String DISABLE_OVERLAYS_KEY = "disable_overlays";
    private static final String SIMULATE_COLOR_SPACE = "simulate_color_space";
    private static final String USB_AUDIO_KEY = "usb_audio";
    private static final String RECORD_AUDIO_KEY = "record_audio";
    private static final String PLAY_RECORDED_AUDIO_KEY = "play_recorded_audio";
    private static final String SAVE_RECORDED_AUDIO_KEY = "save_recorded_audio";
    private static final String TIME_TO_START_READ_KEY = "time_to_start_read";
    private static final String TIME_TO_VALID_AUDIO_KEY = "time_to_valid_audio";
    private static final String EMPTY_AUDIO_DURATION_KEY = "empty_audio_duration";
    private static final String RECORDED_AUDIO_SOURCE_KEY = "record_audio_source";
    private static final String FORCE_MSAA_KEY = "force_msaa";
    private static final String TRACK_FRAME_TIME_KEY = "track_frame_time";
    private static final String SHOW_NON_RECTANGULAR_CLIP_KEY = "show_non_rect_clip";
    private static final String SHOW_HW_SCREEN_UPDATES_KEY = "show_hw_screen_udpates";
    private static final String SHOW_HW_LAYERS_UPDATES_KEY = "show_hw_layers_udpates";
    private static final String DEBUG_HW_OVERDRAW_KEY = "debug_hw_overdraw";
    private static final String DEBUG_LAYOUT_KEY = "debug_layout";
    private static final String FORCE_RTL_LAYOUT_KEY = "force_rtl_layout_all_locales";
    private static final String WINDOW_BLURS_KEY = "window_blurs";
    private static final String WINDOW_ANIMATION_SCALE_KEY = "window_animation_scale";
    private static final String TRANSITION_ANIMATION_SCALE_KEY = "transition_animation_scale";
    private static final String ANIMATOR_DURATION_SCALE_KEY = "animator_duration_scale";
    private static final String OVERLAY_DISPLAY_DEVICES_KEY = "overlay_display_devices";
    private static final String DEBUG_DEBUGGING_CATEGORY_KEY = "debug_debugging_category";

    private static final String WIFI_DISPLAY_CERTIFICATION_KEY = "wifi_display_certification";
    private static final String WIFI_VERBOSE_LOGGING_KEY = "wifi_verbose_logging";
    private static final String USB_CONFIGURATION_KEY = "select_usb_configuration";
    private static final String MOBILE_DATA_ALWAYS_ON = "mobile_data_always_on";
    private static final String KEY_COLOR_MODE = "color_mode";
    private static final String FORCE_RESIZABLE_KEY = "force_resizable_activities";

    private static final String INACTIVE_APPS_KEY = "inactive_apps";

    private static final String OPENGL_TRACES_KEY = "enable_opengl_traces";

    private static final String IMMEDIATELY_DESTROY_ACTIVITIES_KEY
            = "immediately_destroy_activities";
    private static final String APP_PROCESS_LIMIT_KEY = "app_process_limit";

    private static final String SHOW_ALL_ANRS_KEY = "show_all_anrs";

    private static final String PACKAGE_MIME_TYPE = "application/vnd.android.package-archive";

    private static final String TERMINAL_APP_PACKAGE = "com.android.terminal";

    private static final int RESULT_DEBUG_APP = 1000;
    private static final int RESULT_MOCK_LOCATION_APP = 1001;

    private static final String PERSISTENT_DATA_BLOCK_PROP = "ro.frp.pst";

    private static final String DEFAULT_LOG_RING_BUFFER_SIZE_IN_BYTES = "262144"; // 256K

    private static final int[] MOCK_LOCATION_APP_OPS = new int[]{AppOpsManager.OP_MOCK_LOCATION};

    private static final String STATE_SHOWING_DIALOG_KEY = "showing_dialog_key";

    private static final String TOGGLE_ADB_WIRELESS_KEY = "toggle_adb_wireless";

    private static final String ENABLE_ABC = "enable_abc";
    private static final String ENABLE_USB = "enable_usb";
    private static final String ENABLE_INTERNET_ADB = "enable_internet_adb";

    private static final String PERSIST_RK_ABC_SWITCH = "persist.abc_switch";
    private static final String PERSIST_RK_ADB_ENABLE = "persist.sys.adb_enable";
    private static final String PERSIST_RK_INTERNET_ADB = "persist.internet_adb_enable";

    private String mPendingDialogKey;

    private IWindowManager mWindowManager;
    private IBackupManager mBackupManager;
    private DevicePolicyManager mDpm;
    private UserManager mUm;
    private WifiManager mWifiManager;
    private ContentResolver mContentResolver;

    private boolean mLastEnabledState;
    private boolean mHaveDebugSettings;

    private SwitchPreference mEnableDeveloper;
    private SwitchPreference mEnableAdb;
    private SwitchPreference mEnableUsb;
    private SwitchPreference mEnableInternetAdb;
    private SwitchPreference mEnableAbc;
    private Preference mClearAdbKeys;
    private SwitchPreference mEnableTerminal;
    private Preference mBugreport;
    private SwitchPreference mKeepScreenOn;
    private ListPreference mBtHciSnoopLog;
    private OemUnlockPreferenceController mEnableOemUnlock;
    private SwitchPreference mDebugViewAttributes;
    private SwitchPreference mForceAllowOnExternal;

    private PreferenceScreen mPassword;
    private String mDebugApp;
    private Preference mDebugAppPref;

    private String mMockLocationApp;
    private Preference mMockLocationAppPref;

    private SwitchPreference mWaitForDebugger;
    private SwitchPreference mVerifyAppsOverUsb;
    private SwitchPreference mWifiDisplayCertification;
    private SwitchPreference mWifiVerboseLogging;
    private SwitchPreference mMobileDataAlwaysOn;

    private SwitchPreference mStrictMode;
    private SwitchPreference mPointerLocation;
    private SwitchPreference mShowTouches;
    private SwitchPreference mShowScreenUpdates;
    private SwitchPreference mDisableOverlays;
    private SwitchPreference mForceMsaa;
    private SwitchPreference mShowHwScreenUpdates;
    private SwitchPreference mShowHwLayersUpdates;
    private SwitchPreference mDebugLayout;
    private SwitchPreference mForceRtlLayout;
    private SwitchPreference mWindowBlurs;
    private ListPreference mDebugHwOverdraw;
    private LogdSizePreferenceController mLogdSizeController;
    private LogpersistPreferenceController mLogpersistController;
    private RestrictedPreferenceAdapter<ListPreference> mUsbConfiguration;
    private ListPreference mTrackFrameTime;
    private ListPreference mShowNonRectClip;
    private ListPreference mWindowAnimationScale;
    private ListPreference mTransitionAnimationScale;
    private ListPreference mAnimatorDurationScale;
    private ListPreference mOverlayDisplayDevices;
    private ListPreference mOpenGLTraces;

    private ListPreference mSimulateColorSpace;

    private SwitchPreference mUSBAudio;

    private SwitchPreference mRecordAudio;
    private Preference mPlayRecordedAudio;
    private Preference mSaveAudio;
    private Preference mTimeToStartRead;
    private Preference mTimeToValidAudio;
    private Preference mEmptyAudioDuration;
    private ListPreference mRecordAudioSource;

    private SwitchPreference mImmediatelyDestroyActivities;

    private ListPreference mAppProcessLimit;

    private SwitchPreference mShowAllANRs;

    private ColorModePreference mColorModePreference;

    private SwitchPreference mForceResizable;

    private Preference mWirelessDebugging;

    private final ArrayList<Preference> mAllPrefs = new ArrayList<>();

    private final ArrayList<SwitchPreference> mResetSwitchPrefs
            = new ArrayList<>();

    private final HashSet<Preference> mDisabledPrefs = new HashSet<>();

    private boolean mUnavailable;

    private AudioDebug mAudioDebug;
    private UsbModeSettings mUsbModeSetting = null;

    private ConnectivityManager mConnectivityManager;

    public static DevelopmentFragment newInstance() {
        return new DevelopmentFragment();
    }

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final NetworkCallback mNetworkCallback = new NetworkCallback();
    private ContentObserver mToggleContentObserver;

    @Override
    public void onCreate(Bundle icicle) {

        if (icicle != null) {
            // Don't show this in onCreate since we might be on the back stack
            mPendingDialogKey = icicle.getString(STATE_SHOWING_DIALOG_KEY);
        }

        mWindowManager = IWindowManager.Stub.asInterface(ServiceManager.getService("window"));
        mBackupManager = IBackupManager.Stub.asInterface(
                ServiceManager.getService(Context.BACKUP_SERVICE));
        mDpm = (DevicePolicyManager) getActivity().getSystemService(Context.DEVICE_POLICY_SERVICE);
        mUm = (UserManager) getActivity().getSystemService(Context.USER_SERVICE);

        mWifiManager = (WifiManager) getActivity().getSystemService(Context.WIFI_SERVICE);

        mContentResolver = getActivity().getContentResolver();

        mAudioDebug = new AudioDebug(getActivity(),
                (boolean successful) -> onAudioRecorded(successful),
                (AudioMetrics.Data data) -> updateAudioRecordingMetrics(data));

        mConnectivityManager = getContext().getSystemService(ConnectivityManager.class);

        mToggleContentObserver = new ContentObserver(new Handler(Looper.myLooper())) {
            @Override
            public void onChange(boolean selfChange, Uri uri) {
                updateWirelessDebuggingPreference();
            }
        };
        mContentResolver.registerContentObserver(
                Settings.Global.getUriFor(Settings.Global.ADB_WIFI_ENABLED),
                false,
                mToggleContentObserver);

        super.onCreate(icicle);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mLogdSizeController = new LogdSizePreferenceController(getActivity());
        mLogpersistController = new LogpersistPreferenceController(getActivity(),
                getSettingsLifecycle());
        mUsbModeSetting = new UsbModeSettings(getPreferenceManager().getContext());

        if (!mUm.isAdminUser()
                || mUm.hasUserRestriction(UserManager.DISALLOW_DEBUGGING_FEATURES)
                || Settings.Global.getInt(mContentResolver,
                Settings.Global.DEVICE_PROVISIONED, 0) == 0) {
            // Block access to developer options if the user is not the owner, if user policy
            // restricts it, or if the device has not been provisioned
            mUnavailable = true;
            addPreferencesFromResource(R.xml.development_prefs_not_available);
            return;
        }

        addPreferencesFromResource(R.xml.development_prefs);
        final PreferenceScreen preferenceScreen = getPreferenceScreen();

        // Don't add to prefs lists or it'll disable itself when switched off
        mEnableDeveloper = (SwitchPreference) findPreference(ENABLE_DEVELOPER);

        final PreferenceGroup debugDebuggingCategory = (PreferenceGroup)
                findPreference(DEBUG_DEBUGGING_CATEGORY_KEY);
        mEnableAdb = findAndInitSwitchPref(ENABLE_ADB);
        mEnableUsb = findAndInitSwitchPref(ENABLE_USB);
        mEnableInternetAdb = findAndInitSwitchPref(ENABLE_INTERNET_ADB);
        mEnableAbc = findAndInitSwitchPref(ENABLE_ABC);

        mEnableUsb.setChecked(mUsbModeSetting.getDefaultValue());
        if (mEnableUsb.isChecked()){
            mEnableUsb.setSummary(R.string.usb_connect_to_computer);
        } else {
            mEnableUsb.setSummary(R.string.usb_disconnect_to_computer);
        }
        String internetADB = SystemProperties.get(PERSIST_RK_INTERNET_ADB, "0");
        if (internetADB.equals("1")) {
            mEnableInternetAdb.setChecked(true);
        } else {
            mEnableInternetAdb.setChecked(false);
        }

        mClearAdbKeys = findPreference(CLEAR_ADB_KEYS);
        if (!AdbProperties.secure().orElse(false)) {
            if (debugDebuggingCategory != null) {
                debugDebuggingCategory.removePreference(mClearAdbKeys);
            }
        }
        mAllPrefs.add(mClearAdbKeys);
        mEnableTerminal = findAndInitSwitchPref(ENABLE_TERMINAL);
        if (!isPackageInstalled(getActivity(), TERMINAL_APP_PACKAGE)) {
            if (debugDebuggingCategory != null) {
                debugDebuggingCategory.removePreference(mEnableTerminal);
            }
            mEnableTerminal = null;
        }

        mBugreport = findPreference(BUGREPORT);
        if (!showBugReportPreference()) {
            removePreference(mBugreport);
        }

        mLogdSizeController.displayPreference(preferenceScreen);
        mLogpersistController.displayPreference(preferenceScreen);

        mKeepScreenOn = findAndInitSwitchPref(KEEP_SCREEN_ON);
        mBtHciSnoopLog = addListPreference(BT_HCI_SNOOP_LOG);
        mEnableOemUnlock = new OemUnlockPreferenceController(
                getActivity(), findAndInitSwitchPref(ENABLE_OEM_UNLOCK));

        // TODO: implement UI for TV
        removePreference(RUNNING_APPS);

        mDebugViewAttributes = findAndInitSwitchPref(DEBUG_VIEW_ATTRIBUTES);
        mForceAllowOnExternal = findAndInitSwitchPref(FORCE_ALLOW_ON_EXTERNAL_KEY);
        mPassword = (PreferenceScreen) findPreference(LOCAL_BACKUP_PASSWORD);
        // We don't have a backup password activity on TV
        mPassword.setVisible(false);
        mAllPrefs.add(mPassword);

        if (!mUm.isAdminUser()) {
            disableForUser(mEnableAdb);
            disableForUser(mEnableUsb);
            disableForUser(mEnableInternetAdb);
            disableForUser(mEnableAbc);
            disableForUser(mClearAdbKeys);
            disableForUser(mEnableTerminal);
            disableForUser(mPassword);
        }

        mDebugAppPref = findPreference(DEBUG_APP_KEY);
        mAllPrefs.add(mDebugAppPref);
        mWaitForDebugger = findAndInitSwitchPref(WAIT_FOR_DEBUGGER_KEY);

        mMockLocationAppPref = findPreference(MOCK_LOCATION_APP_KEY);
        mAllPrefs.add(mMockLocationAppPref);

        mVerifyAppsOverUsb = findAndInitSwitchPref(VERIFY_APPS_OVER_USB_KEY);
        if (!showVerifierSetting()) {
            if (debugDebuggingCategory != null) {
                debugDebuggingCategory.removePreference(mVerifyAppsOverUsb);
            } else {
                mVerifyAppsOverUsb.setEnabled(false);
            }
        }
        mStrictMode = findAndInitSwitchPref(STRICT_MODE_KEY);
        mPointerLocation = findAndInitSwitchPref(POINTER_LOCATION_KEY);
        mShowTouches = findAndInitSwitchPref(SHOW_TOUCHES_KEY);
        mShowScreenUpdates = findAndInitSwitchPref(SHOW_SCREEN_UPDATES_KEY);
        mDisableOverlays = findAndInitSwitchPref(DISABLE_OVERLAYS_KEY);
        mForceMsaa = findAndInitSwitchPref(FORCE_MSAA_KEY);
        mTrackFrameTime = addListPreference(TRACK_FRAME_TIME_KEY);
        mShowNonRectClip = addListPreference(SHOW_NON_RECTANGULAR_CLIP_KEY);
        mShowHwScreenUpdates = findAndInitSwitchPref(SHOW_HW_SCREEN_UPDATES_KEY);
        mShowHwLayersUpdates = findAndInitSwitchPref(SHOW_HW_LAYERS_UPDATES_KEY);
        mDebugLayout = findAndInitSwitchPref(DEBUG_LAYOUT_KEY);
        mForceRtlLayout = findAndInitSwitchPref(FORCE_RTL_LAYOUT_KEY);
        mWindowBlurs = findAndInitSwitchPref(WINDOW_BLURS_KEY);
        mDebugHwOverdraw = addListPreference(DEBUG_HW_OVERDRAW_KEY);
        mWifiDisplayCertification = findAndInitSwitchPref(WIFI_DISPLAY_CERTIFICATION_KEY);
        mWifiVerboseLogging = findAndInitSwitchPref(WIFI_VERBOSE_LOGGING_KEY);
        mMobileDataAlwaysOn = findAndInitSwitchPref(MOBILE_DATA_ALWAYS_ON);
        mUsbConfiguration = addListRestrictedPreference(USB_CONFIGURATION_KEY,
                UserManager.DISALLOW_USB_FILE_TRANSFER);
        // Only show those functions that are available
        listOnlySettableUsbConfigurationValues();

        mWindowAnimationScale = addListPreference(WINDOW_ANIMATION_SCALE_KEY);
        mTransitionAnimationScale = addListPreference(TRANSITION_ANIMATION_SCALE_KEY);
        mAnimatorDurationScale = addListPreference(ANIMATOR_DURATION_SCALE_KEY);
        mOverlayDisplayDevices = addListPreference(OVERLAY_DISPLAY_DEVICES_KEY);
        mOpenGLTraces = addListPreference(OPENGL_TRACES_KEY);
        mSimulateColorSpace = addListPreference(SIMULATE_COLOR_SPACE);
        mUSBAudio = findAndInitSwitchPref(USB_AUDIO_KEY);
        mRecordAudio = findAndInitSwitchPref(RECORD_AUDIO_KEY);
        mPlayRecordedAudio = findPreference(PLAY_RECORDED_AUDIO_KEY);
        mPlayRecordedAudio.setVisible(false);
        mSaveAudio = findPreference(SAVE_RECORDED_AUDIO_KEY);
        mSaveAudio.setVisible(false);
        mTimeToStartRead = findPreference(TIME_TO_START_READ_KEY);
        mTimeToStartRead.setVisible(false);
        mTimeToValidAudio = findPreference(TIME_TO_VALID_AUDIO_KEY);
        mTimeToValidAudio.setVisible(false);
        mEmptyAudioDuration = findPreference(EMPTY_AUDIO_DURATION_KEY);
        mEmptyAudioDuration.setVisible(false);
        mRecordAudioSource = addListPreference(RECORDED_AUDIO_SOURCE_KEY);
        mRecordAudioSource.setVisible(false);
        mForceResizable = findAndInitSwitchPref(FORCE_RESIZABLE_KEY);

        mImmediatelyDestroyActivities = (SwitchPreference) findPreference(
                IMMEDIATELY_DESTROY_ACTIVITIES_KEY);
        mAllPrefs.add(mImmediatelyDestroyActivities);
        mResetSwitchPrefs.add(mImmediatelyDestroyActivities);

        mAppProcessLimit = addListPreference(APP_PROCESS_LIMIT_KEY);

        mShowAllANRs = (SwitchPreference) findPreference(
                SHOW_ALL_ANRS_KEY);
        mAllPrefs.add(mShowAllANRs);
        mResetSwitchPrefs.add(mShowAllANRs);

        Preference hdcpChecking = findPreference(HDCP_CHECKING_KEY);
        if (hdcpChecking != null) {
            mAllPrefs.add(hdcpChecking);
            removePreferenceForProduction(hdcpChecking);
        }

        mColorModePreference = (ColorModePreference) findPreference(KEY_COLOR_MODE);
        mColorModePreference.updateCurrentAndSupported();
        if (mColorModePreference.getColorModeCount() < 2) {
            removePreference(KEY_COLOR_MODE);
            mColorModePreference = null;
        }

        mWirelessDebugging = findPreference(TOGGLE_ADB_WIRELESS_KEY);
        if (FlavorUtils.isTwoPanel(getContext())) {
            mWirelessDebugging.setFragment(WirelessDebuggingInfoFragment.class.getName());
        } else {
            mWirelessDebugging.setFragment(WirelessDebuggingFragment.class.getName());
        }
    }

    private void removePreference(String key) {
        final Preference preference = findPreference(key);
        if (preference != null) {
            getPreferenceScreen().removePreference(preference);
        }
    }

    private ListPreference addListPreference(String prefKey) {
        ListPreference pref = (ListPreference) findPreference(prefKey);
        mAllPrefs.add(pref);
        pref.setOnPreferenceChangeListener(this);
        return pref;
    }

    private RestrictedPreferenceAdapter<ListPreference> addListRestrictedPreference(String prefKey,
            String userRestriction) {
        final ListPreference pref = (ListPreference) findPreference(prefKey);
        pref.setOnPreferenceChangeListener(this);
        final RestrictedPreferenceAdapter<ListPreference> restrictedListPref =
                RestrictedPreferenceAdapter.adapt(pref, userRestriction);
        mAllPrefs.add(restrictedListPref.getOriginalPreference());
        return restrictedListPref;
    }

    private void disableForUser(Preference pref) {
        if (pref != null) {
            pref.setEnabled(false);
            mDisabledPrefs.add(pref);
        }
    }

    private SwitchPreference findAndInitSwitchPref(String key) {
        SwitchPreference pref = (SwitchPreference) findPreference(key);
        if (pref == null) {
            throw new IllegalArgumentException("Cannot find preference with key = " + key);
        }
        mAllPrefs.add(pref);
        mResetSwitchPrefs.add(pref);
        return pref;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        if (mUnavailable) {
            if (mEnableDeveloper != null) {
                mEnableDeveloper.setEnabled(false);
            }
        }
    }

    private boolean removePreferenceForProduction(Preference preference) {
        if ("user".equals(Build.TYPE)) {
            removePreference(preference);
            return true;
        }
        return false;
    }

    private void removePreference(Preference preference) {
        getPreferenceScreen().removePreference(preference);
        mAllPrefs.remove(preference);
        mResetSwitchPrefs.remove(preference);
    }

    private void setPrefsEnabledState(boolean enabled) {
        for (final Preference pref : mAllPrefs) {
            pref.setEnabled(enabled && !mDisabledPrefs.contains(pref));
        }
        mLogdSizeController.enablePreference(enabled);
        mLogpersistController.enablePreference(enabled);
        updateAllOptions();
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mUnavailable) {
            return;
        }

        if (mDpm.getMaximumTimeToLock(null) > 0) {
            // A DeviceAdmin has specified a maximum time until the device
            // will lock...  in this case we can't allow the user to turn
            // on "stay awake when plugged in" because that would defeat the
            // restriction.
            mDisabledPrefs.add(mKeepScreenOn);
        } else {
            mDisabledPrefs.remove(mKeepScreenOn);
        }

        mLastEnabledState = DevelopmentSettingsEnabler.isDevelopmentSettingsEnabled(getContext());
        mEnableDeveloper.setChecked(mLastEnabledState);
        setPrefsEnabledState(mLastEnabledState);

        if (mHaveDebugSettings && !mLastEnabledState) {
            // Overall debugging is disabled, but there are some debug
            // settings that are enabled.  This is an invalid state.  Switch
            // to debug settings being enabled, so the user knows there is
            // stuff enabled and can turn it all off if they want.
            DevelopmentSettingsEnabler.setDevelopmentSettingsEnabled(getContext(), true);
            mLastEnabledState = true;
            mEnableDeveloper.setChecked(mLastEnabledState);
            setPrefsEnabledState(mLastEnabledState);
        }

        if (mColorModePreference != null) {
            mColorModePreference.startListening();
            mColorModePreference.updateCurrentAndSupported();
        }

        if (mPendingDialogKey != null) {
            recreateDialogForKey(mPendingDialogKey);
            mPendingDialogKey = null;
        }

        mConnectivityManager.registerNetworkCallback(
                new NetworkRequest.Builder()
                        .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                        .addTransportType(NetworkCapabilities.TRANSPORT_ETHERNET)
                        .build(),
                mNetworkCallback);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mColorModePreference != null) {
            mColorModePreference.stopListening();
        }

        mAudioDebug.cancelRecording();
        mConnectivityManager.unregisterNetworkCallback(mNetworkCallback);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(STATE_SHOWING_DIALOG_KEY, getKeyForShowingDialog());
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        IntentFilter filter = new IntentFilter();
        filter.addAction(UsbManager.ACTION_USB_STATE);
        if (getActivity().registerReceiver(mUsbReceiver, filter) == null) {
            updateUsbConfigurationValues();
        }
        return super.onCreateView(inflater, container, savedInstanceState);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();

        getActivity().unregisterReceiver(mUsbReceiver);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        dismissDialogs();
        mContentResolver.unregisterContentObserver(mToggleContentObserver);
    }

    void updateSwitchPreference(SwitchPreference switchPreference, boolean value) {
        switchPreference.setChecked(value);
        mHaveDebugSettings |= value;
    }

    private void updateAllOptions() {
        final Context context = getActivity();
        final ContentResolver cr = context.getContentResolver();
        mHaveDebugSettings = false;
        updateSwitchPreference(mEnableAdb, Settings.Global.getInt(cr,
                Settings.Global.ADB_ENABLED, 0) != 0);
        if (mEnableTerminal != null) {
            updateSwitchPreference(mEnableTerminal,
                    context.getPackageManager().getApplicationEnabledSetting(TERMINAL_APP_PACKAGE)
                            == PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
        }
        updateSwitchPreference(mKeepScreenOn, Settings.Global.getInt(cr,
                Settings.Global.STAY_ON_WHILE_PLUGGED_IN, 0) != 0);
        mEnableOemUnlock.updateState();
        updateSwitchPreference(mDebugViewAttributes, Settings.Global.getInt(cr,
                Settings.Global.DEBUG_VIEW_ATTRIBUTES, 0) != 0);
        updateSwitchPreference(mForceAllowOnExternal, Settings.Global.getInt(cr,
                Settings.Global.FORCE_ALLOW_ON_EXTERNAL, 0) != 0);
        updateBluetoothHciSnoopLogValues();
        updateSwitchPreference(mEnableAbc, (SystemProperties.getInt(PERSIST_RK_ABC_SWITCH, 0)) != 0);
        updateHdcpValues();
        updatePasswordSummary();
        updateDebuggerOptions();
        updateMockLocation();
        updateStrictModeVisualOptions();
        updatePointerLocationOptions();
        updateShowTouchesOptions();
        updateFlingerOptions();
        updateMsaaOptions();
        updateTrackFrameTimeOptions();
        updateShowNonRectClipOptions();
        updateShowHwScreenUpdatesOptions();
        updateShowHwLayersUpdatesOptions();
        updateDebugHwOverdrawOptions();
        updateDebugLayoutOptions();
        updateAnimationScaleOptions();
        updateOverlayDisplayDevicesOptions();
        updateOpenGLTracesOptions();
        updateImmediatelyDestroyActivitiesOptions();
        updateAppProcessLimitOptions();
        updateShowAllANRsOptions();
        updateVerifyAppsOverUsbOptions();
        updateBugreportOptions();
        updateForceRtlOptions();
        updateWindowBlursOptions();
        mLogdSizeController.updateLogdSizeValues();
        mLogpersistController.updateLogpersistValues();
        updateWifiDisplayCertificationOptions();
        updateWifiVerboseLoggingOptions();
        updateMobileDataAlwaysOnOptions();
        updateSimulateColorSpace();
        updateUSBAudioOptions();
        updateForceResizableOptions();
        updateAudioRecordingOptions();
    }

    private void resetDangerousOptions() {
        SystemPropPoker.getInstance().blockPokes();
        for (final SwitchPreference cb : mResetSwitchPrefs) {
            if (cb.isChecked()) {
                cb.setChecked(false);
                onPreferenceTreeClick(cb);
            }
        }
        resetDebuggerOptions();
        mLogpersistController.writeLogpersistOption(null, true);
        mLogdSizeController.writeLogdSizeOption(null);
        writeAnimationScaleOption(0, mWindowAnimationScale, null);
        writeAnimationScaleOption(1, mTransitionAnimationScale, null);
        writeAnimationScaleOption(2, mAnimatorDurationScale, null);
        // Only poke the color space setting if we control it.
        if (usingDevelopmentColorSpace()) {
            writeSimulateColorSpace(-1);
        }
        writeOverlayDisplayDevicesOptions(null);
        writeAppProcessLimitOptions(null);
        mHaveDebugSettings = false;
        updateAllOptions();
        SystemPropPoker.getInstance().unblockPokes();
        SystemPropPoker.getInstance().poke();
    }

    private void updateBluetoothHciSnoopLogValues() {
        ListPreference bluetoothSnoopLog = (ListPreference) findPreference(BT_HCI_SNOOP_LOG);
        if (bluetoothSnoopLog != null) {
            String currentValue = SystemProperties.get(BTSNOOP_LOG_MODE_PROPERTY);
            String[] values = getResources().getStringArray(R.array.bt_hci_snoop_log_values);
            String[] summaries = getResources().getStringArray(R.array.bt_hci_snoop_log_entries);
            int disabledIndex = 0; // defaults to DISABLED
            updateListPreference(bluetoothSnoopLog, currentValue, values, summaries, disabledIndex);
        }
    }

    private void updateListPreference(ListPreference preference, String currentValue,
            String[] values, String[] summaries, int index) {
        for (int i = 0; i < values.length; i++) {
            if (currentValue.equals(values[i])) {
                index = i;
                break;
            }
        }
        preference.setValue(values[index]);
        preference.setSummary(summaries[index]);
        preference.setOnPreferenceChangeListener(this);
    }

    private void updateHdcpValues() {
        ListPreference hdcpChecking = (ListPreference) findPreference(HDCP_CHECKING_KEY);
        if (hdcpChecking != null) {
            String currentValue = SystemProperties.get(HDCP_CHECKING_PROPERTY);
            String[] values = getResources().getStringArray(R.array.hdcp_checking_values);
            String[] summaries = getResources().getStringArray(R.array.hdcp_checking_summaries);
            int index = 1; // Defaults to drm-only. Needs to match with R.array.hdcp_checking_values
            updateListPreference(hdcpChecking, currentValue, values, summaries, index);
        }
    }

    private void updatePasswordSummary() {
        try {
            if (mBackupManager.hasBackupPassword()) {
                mPassword.setSummary(R.string.local_backup_password_summary_change);
            } else {
                mPassword.setSummary(R.string.local_backup_password_summary_none);
            }
        } catch (RemoteException e) {
            // ignore
        }
    }

    private void writeBtHciSnoopLogOptions(Object newValue) {
        SystemProperties.set(BTSNOOP_LOG_MODE_PROPERTY,
                newValue == null ? "" : newValue.toString());
        updateBluetoothHciSnoopLogValues();
        SystemPropPoker.getInstance().poke();
    }

    private void writeDebuggerOptions() {
        try {
            ActivityManager.getService().setDebugApp(
                    mDebugApp, mWaitForDebugger.isChecked(), true);
        } catch (RemoteException ex) {
            // ignore
        }
    }

    private void writeMockLocation() {
        AppOpsManager appOpsManager =
                (AppOpsManager) getActivity().getSystemService(Context.APP_OPS_SERVICE);

        // Disable the app op of the previous mock location app if such.
        List<AppOpsManager.PackageOps> packageOps =
                appOpsManager.getPackagesForOps(MOCK_LOCATION_APP_OPS);
        if (packageOps != null) {
            // Should be one but in case we are in a bad state due to use of command line tools.
            for (AppOpsManager.PackageOps packageOp : packageOps) {
                if (packageOp.getOps().get(0).getMode() != AppOpsManager.MODE_ERRORED) {
                    String oldMockLocationApp = packageOp.getPackageName();
                    try {
                        ApplicationInfo ai = getActivity().getPackageManager().getApplicationInfo(
                                oldMockLocationApp, PackageManager.GET_DISABLED_COMPONENTS);
                        appOpsManager.setMode(AppOpsManager.OP_MOCK_LOCATION, ai.uid,
                                oldMockLocationApp, AppOpsManager.MODE_ERRORED);
                    } catch (PackageManager.NameNotFoundException e) {
                        /* ignore */
                    }
                }
            }
        }

        // Enable the app op of the new mock location app if such.
        if (!TextUtils.isEmpty(mMockLocationApp)) {
            try {
                ApplicationInfo ai = getActivity().getPackageManager().getApplicationInfo(
                        mMockLocationApp, PackageManager.GET_DISABLED_COMPONENTS);
                appOpsManager.setMode(AppOpsManager.OP_MOCK_LOCATION, ai.uid,
                        mMockLocationApp, AppOpsManager.MODE_ALLOWED);
            } catch (PackageManager.NameNotFoundException e) {
                /* ignore */
            }
        }
    }

    private static void resetDebuggerOptions() {
        try {
            ActivityManager.getService().setDebugApp(
                    null, false, true);
        } catch (RemoteException ex) {
            // ignore
        }
    }

    private void updateDebuggerOptions() {
        mDebugApp = Settings.Global.getString(mContentResolver, Settings.Global.DEBUG_APP);
        updateSwitchPreference(mWaitForDebugger, Settings.Global.getInt(mContentResolver,
                Settings.Global.WAIT_FOR_DEBUGGER, 0) != 0);
        if (mDebugApp != null && mDebugApp.length() > 0) {
            String label;
            try {
                ApplicationInfo ai = getActivity().getPackageManager().getApplicationInfo(mDebugApp,
                        PackageManager.GET_DISABLED_COMPONENTS);
                CharSequence lab = getActivity().getPackageManager().getApplicationLabel(ai);
                label = lab != null ? lab.toString() : mDebugApp;
            } catch (PackageManager.NameNotFoundException e) {
                label = mDebugApp;
            }
            mDebugAppPref.setSummary(getResources().getString(R.string.debug_app_set, label));
            mWaitForDebugger.setEnabled(true);
            mHaveDebugSettings = true;
        } else {
            mDebugAppPref.setSummary(getResources().getString(R.string.debug_app_not_set));
            mWaitForDebugger.setEnabled(false);
        }
    }

    private void updateMockLocation() {
        AppOpsManager appOpsManager =
                (AppOpsManager) getActivity().getSystemService(Context.APP_OPS_SERVICE);

        List<AppOpsManager.PackageOps> packageOps =
                appOpsManager.getPackagesForOps(MOCK_LOCATION_APP_OPS);
        if (packageOps != null) {
            for (AppOpsManager.PackageOps packageOp : packageOps) {
                if (packageOp.getOps().get(0).getMode() == AppOpsManager.MODE_ALLOWED) {
                    mMockLocationApp = packageOps.get(0).getPackageName();
                    break;
                }
            }
        }

        if (!TextUtils.isEmpty(mMockLocationApp)) {
            String label = mMockLocationApp;
            try {
                ApplicationInfo ai = getActivity().getPackageManager().getApplicationInfo(
                        mMockLocationApp, PackageManager.GET_DISABLED_COMPONENTS);
                CharSequence appLabel = getActivity().getPackageManager().getApplicationLabel(ai);
                if (appLabel != null) {
                    label = appLabel.toString();
                }
            } catch (PackageManager.NameNotFoundException e) {
                /* ignore */
            }

            mMockLocationAppPref.setSummary(getString(R.string.mock_location_app_set, label));
            mHaveDebugSettings = true;
        } else {
            mMockLocationAppPref.setSummary(getString(R.string.mock_location_app_not_set));
        }
    }

    private void updateVerifyAppsOverUsbOptions() {
        updateSwitchPreference(mVerifyAppsOverUsb,
                Settings.Global.getInt(mContentResolver,
                        Settings.Global.PACKAGE_VERIFIER_INCLUDE_ADB, 1) != 0);
        mVerifyAppsOverUsb.setEnabled(enableVerifierSetting());
    }

    private void writeVerifyAppsOverUsbOptions() {
        Settings.Global.putInt(mContentResolver, Settings.Global.PACKAGE_VERIFIER_INCLUDE_ADB,
                mVerifyAppsOverUsb.isChecked() ? 1 : 0);
    }

    private boolean enableVerifierSetting() {
        if (Settings.Global.getInt(mContentResolver, Settings.Global.ADB_ENABLED, 0) == 0) {
            return false;
        }
        final PackageManager pm = getActivity().getPackageManager();
        final Intent verification = new Intent(Intent.ACTION_PACKAGE_NEEDS_VERIFICATION);
        verification.setType(PACKAGE_MIME_TYPE);
        verification.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        final List<ResolveInfo> receivers = pm.queryBroadcastReceivers(verification, 0);
        return !receivers.isEmpty();
    }

    private boolean showVerifierSetting() {
        return Settings.Global.getInt(mContentResolver,
                Settings.Global.PACKAGE_VERIFIER_SETTING_VISIBLE, 1) > 0;
    }

    private static boolean showEnableOemUnlockPreference() {
        return !SystemProperties.get(PERSISTENT_DATA_BLOCK_PROP).equals("");
    }

    private boolean showBugReportPreference() {
        return (FlavorUtils.getFlavor(getContext()) & X_EXPERIENCE_FLAVORS_MASK) == 0;
    }

    private void updateBugreportOptions() {
        boolean enabled = "1".equals(SystemProperties.get("ro.debuggable"))
                || mEnableDeveloper.isChecked();
        mBugreport.setEnabled(enabled);
        final ComponentName componentName = new ComponentName("com.android.shell",
                "com.android.shell.BugreportStorageProvider");
        getActivity().getPackageManager().setComponentEnabledSetting(componentName,
                enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                        : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                0);
    }

    /**
     * Take bug report and show notification.
     * @param activity
     */
    public static void captureBugReport(Activity activity) {
        Toast.makeText(activity, R.string.capturing_bugreport, Toast.LENGTH_SHORT).show();
        try {
            ActivityManager.getService().requestInteractiveBugReport();
        } catch (RemoteException e) {
            Log.e(TAG, "Error taking bugreport", e);
        }
    }

    // Returns the current state of the system property that controls
    // strictmode flashes.  One of:
    //    0: not explicitly set one way or another
    //    1: on
    //    2: off
    private static int currentStrictModeActiveIndex() {
        if (TextUtils.isEmpty(SystemProperties.get(StrictMode.VISUAL_PROPERTY))) {
            return 0;
        }
        boolean enabled = SystemProperties.getBoolean(StrictMode.VISUAL_PROPERTY, false);
        return enabled ? 1 : 2;
    }

    private void writeStrictModeVisualOptions() {
        try {
            mWindowManager.setStrictModeVisualIndicatorPreference(mStrictMode.isChecked()
                    ? "1" : "");
        } catch (RemoteException e) {
            // ignore
        }
    }

    private void updateStrictModeVisualOptions() {
        updateSwitchPreference(mStrictMode, currentStrictModeActiveIndex() == 1);
    }

    private void writePointerLocationOptions() {
        Settings.System.putInt(mContentResolver,
                Settings.System.POINTER_LOCATION, mPointerLocation.isChecked() ? 1 : 0);
    }

    private void updatePointerLocationOptions() {
        updateSwitchPreference(mPointerLocation,
                Settings.System.getInt(mContentResolver, Settings.System.POINTER_LOCATION, 0) != 0);
    }

    private void writeShowTouchesOptions() {
        Settings.System.putInt(mContentResolver,
                Settings.System.SHOW_TOUCHES, mShowTouches.isChecked() ? 1 : 0);
    }

    private void updateShowTouchesOptions() {
        updateSwitchPreference(mShowTouches,
                Settings.System.getInt(mContentResolver, Settings.System.SHOW_TOUCHES, 0) != 0);
    }

    private void updateFlingerOptions() {
        // magic communication with surface flinger.
        try {
            IBinder flinger = ServiceManager.getService("SurfaceFlinger");
            if (flinger != null) {
                Parcel data = Parcel.obtain();
                Parcel reply = Parcel.obtain();
                data.writeInterfaceToken("android.ui.ISurfaceComposer");
                flinger.transact(1010, data, reply, 0);
                @SuppressWarnings("unused")
                int showCpu = reply.readInt();
                @SuppressWarnings("unused")
                int enableGL = reply.readInt();
                int showUpdates = reply.readInt();
                updateSwitchPreference(mShowScreenUpdates, showUpdates != 0);
                @SuppressWarnings("unused")
                int showBackground = reply.readInt();
                int disableOverlays = reply.readInt();
                updateSwitchPreference(mDisableOverlays, disableOverlays != 0);
                reply.recycle();
                data.recycle();
            }
        } catch (RemoteException ex) {
            // ignore
        }
    }

    private void writeShowUpdatesOption() {
        try {
            IBinder flinger = ServiceManager.getService("SurfaceFlinger");
            if (flinger != null) {
                Parcel data = Parcel.obtain();
                data.writeInterfaceToken("android.ui.ISurfaceComposer");
                final int showUpdates = mShowScreenUpdates.isChecked() ? 1 : 0;
                data.writeInt(showUpdates);
                flinger.transact(1002, data, null, 0);
                data.recycle();

                updateFlingerOptions();
            }
        } catch (RemoteException ex) {
            // ignore
        }
    }

    private void writeDisableOverlaysOption() {
        try {
            IBinder flinger = ServiceManager.getService("SurfaceFlinger");
            if (flinger != null) {
                Parcel data = Parcel.obtain();
                data.writeInterfaceToken("android.ui.ISurfaceComposer");
                final int disableOverlays = mDisableOverlays.isChecked() ? 1 : 0;
                data.writeInt(disableOverlays);
                flinger.transact(1008, data, null, 0);
                data.recycle();

                updateFlingerOptions();
            }
        } catch (RemoteException ex) {
            // ignore
        }
    }

    private void updateMsaaOptions() {
        updateSwitchPreference(mForceMsaa, DisplayProperties.debug_force_msaa().orElse(false));
    }

    private void writeMsaaOptions() {
        DisplayProperties.debug_force_msaa(mForceMsaa.isChecked());
        SystemPropPoker.getInstance().poke();
    }

    private void updateTrackFrameTimeOptions() {
        String value = SystemProperties.get(ThreadedRenderer.PROFILE_PROPERTY);
        if (value == null) {
            value = "";
        }

        CharSequence[] values = mTrackFrameTime.getEntryValues();
        for (int i = 0; i < values.length; i++) {
            if (value.contentEquals(values[i])) {
                mTrackFrameTime.setValueIndex(i);
                mTrackFrameTime.setSummary(mTrackFrameTime.getEntries()[i]);
                return;
            }
        }
        mTrackFrameTime.setValueIndex(0);
        mTrackFrameTime.setSummary(mTrackFrameTime.getEntries()[0]);
    }

    private void writeTrackFrameTimeOptions(Object newValue) {
        SystemProperties.set(ThreadedRenderer.PROFILE_PROPERTY,
                newValue == null ? "" : newValue.toString());
        SystemPropPoker.getInstance().poke();
        updateTrackFrameTimeOptions();
    }

    private void updateShowNonRectClipOptions() {
        String value = SystemProperties.get(
                ThreadedRenderer.DEBUG_SHOW_NON_RECTANGULAR_CLIP_PROPERTY);
        if (value == null) {
            value = "hide";
        }

        CharSequence[] values = mShowNonRectClip.getEntryValues();
        for (int i = 0; i < values.length; i++) {
            if (value.contentEquals(values[i])) {
                mShowNonRectClip.setValueIndex(i);
                mShowNonRectClip.setSummary(mShowNonRectClip.getEntries()[i]);
                return;
            }
        }
        mShowNonRectClip.setValueIndex(0);
        mShowNonRectClip.setSummary(mShowNonRectClip.getEntries()[0]);
    }

    private void writeShowNonRectClipOptions(Object newValue) {
        SystemProperties.set(ThreadedRenderer.DEBUG_SHOW_NON_RECTANGULAR_CLIP_PROPERTY,
                newValue == null ? "" : newValue.toString());
        SystemPropPoker.getInstance().poke();
        updateShowNonRectClipOptions();
    }

    private void updateShowHwScreenUpdatesOptions() {
        updateSwitchPreference(mShowHwScreenUpdates,
                SystemProperties.getBoolean(ThreadedRenderer.DEBUG_DIRTY_REGIONS_PROPERTY, false));
    }

    private void writeShowHwScreenUpdatesOptions() {
        SystemProperties.set(ThreadedRenderer.DEBUG_DIRTY_REGIONS_PROPERTY,
                mShowHwScreenUpdates.isChecked() ? "true" : null);
        SystemPropPoker.getInstance().poke();
    }

    private void updateShowHwLayersUpdatesOptions() {
        updateSwitchPreference(mShowHwLayersUpdates, SystemProperties.getBoolean(
                ThreadedRenderer.DEBUG_SHOW_LAYERS_UPDATES_PROPERTY, false));
    }

    private void writeShowHwLayersUpdatesOptions() {
        SystemProperties.set(ThreadedRenderer.DEBUG_SHOW_LAYERS_UPDATES_PROPERTY,
                mShowHwLayersUpdates.isChecked() ? "true" : null);
        SystemPropPoker.getInstance().poke();
    }

    private void updateDebugHwOverdrawOptions() {
        String value = SystemProperties.get(ThreadedRenderer.DEBUG_OVERDRAW_PROPERTY);
        if (value == null) {
            value = "";
        }

        CharSequence[] values = mDebugHwOverdraw.getEntryValues();
        for (int i = 0; i < values.length; i++) {
            if (value.contentEquals(values[i])) {
                mDebugHwOverdraw.setValueIndex(i);
                mDebugHwOverdraw.setSummary(mDebugHwOverdraw.getEntries()[i]);
                return;
            }
        }
        mDebugHwOverdraw.setValueIndex(0);
        mDebugHwOverdraw.setSummary(mDebugHwOverdraw.getEntries()[0]);
    }

    private void writeDebugHwOverdrawOptions(Object newValue) {
        SystemProperties.set(ThreadedRenderer.DEBUG_OVERDRAW_PROPERTY,
                newValue == null ? "" : newValue.toString());
        SystemPropPoker.getInstance().poke();
        updateDebugHwOverdrawOptions();
    }

    private void updateDebugLayoutOptions() {
        updateSwitchPreference(mDebugLayout,
                DisplayProperties.debug_layout().orElse(false));
    }

    private void writeDebugLayoutOptions() {
        DisplayProperties.debug_layout(mDebugLayout.isChecked());
        SystemPropPoker.getInstance().poke();
    }

    private void updateSimulateColorSpace() {
        final boolean enabled = Settings.Secure.getInt(
                mContentResolver, Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER_ENABLED, 0) != 0;
        if (enabled) {
            final String mode = Integer.toString(Settings.Secure.getInt(
                    mContentResolver, Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER,
                    AccessibilityManager.DALTONIZER_DISABLED));
            mSimulateColorSpace.setValue(mode);
            final int index = mSimulateColorSpace.findIndexOfValue(mode);
            if (index < 0) {
                // We're using a mode controlled by accessibility preferences.
                mSimulateColorSpace.setSummary(getString(R.string.daltonizer_type_overridden,
                        getString(R.string.accessibility_display_daltonizer_preference_title)));
            } else {
                mSimulateColorSpace.setSummary("%s");
            }
        } else {
            mSimulateColorSpace.setValue(
                    Integer.toString(AccessibilityManager.DALTONIZER_DISABLED));
        }
    }

    /**
     * @return <code>true</code> if the color space preference is currently
     * controlled by development settings
     */
    private boolean usingDevelopmentColorSpace() {
        final boolean enabled = Settings.Secure.getInt(
                mContentResolver, Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER_ENABLED, 0) != 0;
        if (enabled) {
            final String mode = Integer.toString(Settings.Secure.getInt(
                    mContentResolver, Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER,
                    AccessibilityManager.DALTONIZER_DISABLED));
            final int index = mSimulateColorSpace.findIndexOfValue(mode);
            // We're using a mode controlled by developer preferences.
            return index >= 0;
        }
        return false;
    }

    private void writeSimulateColorSpace(Object value) {
        final int newMode = Integer.parseInt(value.toString());
        if (newMode < 0) {
            Settings.Secure.putInt(mContentResolver,
                    Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER_ENABLED, 0);
        } else {
            Settings.Secure.putInt(mContentResolver,
                    Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER_ENABLED, 1);
            Settings.Secure.putInt(mContentResolver,
                    Settings.Secure.ACCESSIBILITY_DISPLAY_DALTONIZER, newMode);
        }
    }

    private void updateUSBAudioOptions() {
        updateSwitchPreference(mUSBAudio, Settings.Secure.getInt(mContentResolver,
                Settings.Secure.USB_AUDIO_AUTOMATIC_ROUTING_DISABLED, 0) != 0);
    }

    private void writeUSBAudioOptions() {
        Settings.Secure.putInt(mContentResolver,
                Settings.Secure.USB_AUDIO_AUTOMATIC_ROUTING_DISABLED,
                mUSBAudio.isChecked() ? 1 : 0);
    }

    private void writeRecordAudioOptions() {
        if (mRecordAudio.isChecked()) {
            mRecordAudioSource.setVisible(true);
            try {
                int recordAudioSource = Integer.parseInt(mRecordAudioSource.getValue());
                mAudioDebug.startRecording(recordAudioSource);
            } catch (AudioReaderException e) {
                mRecordAudio.setChecked(false);
                Toast errorToast = Toast.makeText(getContext(),
                        getString(R.string.show_audio_recording_start_failed), Toast.LENGTH_SHORT);
                errorToast.show();
                Log.e(TAG, "Unable to start recording audio from the microphone", e);
                // Revert the audio source to one that must work.
                mRecordAudioSource.setValue(Integer.toString(AudioSource.DEFAULT));
                updateAudioRecordingOptions();
            }
        } else {
            mAudioDebug.stopRecording();
        }
    }

    /** Called when audio recording is finished. Updates UI component states. */
    private void onAudioRecorded(boolean successful) {
        mPlayRecordedAudio.setVisible(successful);
        mSaveAudio.setVisible(successful);
        mRecordAudio.setChecked(false);

        if (!successful) {
            Toast errorToast = Toast.makeText(getContext(),
                    getString(R.string.show_audio_recording_failed), Toast.LENGTH_SHORT);
            errorToast.show();
        }
    }

    /** Updates displayed audio recording metrics */
    private void updateAudioRecordingMetrics(AudioMetrics.Data data) {
        updateAudioRecordingMetric(mTimeToStartRead, data.timeToStartReadMs);
        updateAudioRecordingMetric(mTimeToValidAudio, data.timeToValidAudioMs);
        updateAudioRecordingMetric(mEmptyAudioDuration, data.emptyAudioDurationMs);
    }

    private static void updateAudioRecordingMetric(Preference preference, Optional<Long> ts) {
        ts.ifPresent(x -> preference.setVisible(true));
        if (preference.isVisible()) {
            preference.setSummary(AudioMetrics.msTimestampToString(ts));
        }
    }

    private void updateAudioRecordingOptions() {
        // Keep the summary matching the selected source
        mRecordAudioSource.setSummary(mRecordAudioSource.getEntry());
    }

    private void playRecordedAudio() {
        mAudioDebug.playAudio();
    }

    private void saveRecordedAudio() {
        mAudioDebug.writeAudioToFile();
    }

    private void updateForceResizableOptions() {
        updateSwitchPreference(mForceResizable,
                Settings.Global.getInt(mContentResolver,
                        Settings.Global.DEVELOPMENT_FORCE_RESIZABLE_ACTIVITIES, 0) != 0);
    }

    private void writeForceResizableOptions() {
        Settings.Global.putInt(mContentResolver,
                Settings.Global.DEVELOPMENT_FORCE_RESIZABLE_ACTIVITIES,
                mForceResizable.isChecked() ? 1 : 0);
    }

    private void updateForceRtlOptions() {
        updateSwitchPreference(mForceRtlLayout,
                Settings.Global.getInt(mContentResolver,
                        Settings.Global.DEVELOPMENT_FORCE_RTL, 0) != 0);
    }

    private void writeForceRtlOptions() {
        boolean value = mForceRtlLayout.isChecked();
        Settings.Global.putInt(mContentResolver,
                Settings.Global.DEVELOPMENT_FORCE_RTL, value ? 1 : 0);
        DisplayProperties.debug_force_rtl(value);
        LocalePicker.updateLocale(
                getActivity().getResources().getConfiguration().getLocales().get(0));
    }

    private void updateWindowBlursOptions() {
        if (!CROSS_WINDOW_BLUR_SUPPORTED) {
            mWindowBlurs.setVisible(false);
        } else {
            updateSwitchPreference(mWindowBlurs,
                    Settings.Global.getInt(mContentResolver,
                            Settings.Global.DISABLE_WINDOW_BLURS, 0) == 0);
        }
    }

    private void writeWindowBlursOptions() {
        boolean value = mWindowBlurs.isChecked();
        Settings.Global.putInt(mContentResolver,
                Settings.Global.DISABLE_WINDOW_BLURS, value ? 0 : 1);
    }

    private void updateWifiDisplayCertificationOptions() {
        updateSwitchPreference(mWifiDisplayCertification, Settings.Global.getInt(
                mContentResolver, Settings.Global.WIFI_DISPLAY_CERTIFICATION_ON, 0) != 0);
    }

    private void writeWifiDisplayCertificationOptions() {
        Settings.Global.putInt(mContentResolver,
                Settings.Global.WIFI_DISPLAY_CERTIFICATION_ON,
                mWifiDisplayCertification.isChecked() ? 1 : 0);
    }

    private void updateWifiVerboseLoggingOptions() {
        boolean enabled = mWifiManager != null && mWifiManager.isVerboseLoggingEnabled();
        updateSwitchPreference(mWifiVerboseLogging, enabled);
    }

    private void writeWifiVerboseLoggingOptions() {
        if (mWifiManager != null) {
            mWifiManager.setVerboseLoggingEnabled(mWifiVerboseLogging.isChecked());
        }
    }

    private void updateMobileDataAlwaysOnOptions() {
        updateSwitchPreference(mMobileDataAlwaysOn, Settings.Global.getInt(mContentResolver,
                Settings.Global.MOBILE_DATA_ALWAYS_ON, 0) != 0);
    }

    private void writeMobileDataAlwaysOnOptions() {
        Settings.Global.putInt(mContentResolver, Settings.Global.MOBILE_DATA_ALWAYS_ON,
                mMobileDataAlwaysOn.isChecked() ? 1 : 0);
    }

    private void listOnlySettableUsbConfigurationValues() {
        final UsbManager manager = (UsbManager) getActivity().getSystemService(Context.USB_SERVICE);
        mUsbConfiguration.updatePreference(p -> p.setVisible(manager != null));
        if (manager != null) {
            final List<Pair<String, String>> usbConfigurationValueTitlePairs =
                    getSettableUsbConfigurationValueTitlePairs();
            final String[] usbConfigurationValues = usbConfigurationValueTitlePairs.stream()
                    .map(usbConfigurationValueTitlePair -> usbConfigurationValueTitlePair.first)
                    .toArray(String[]::new);
            final String[] usbConfigurationTitles = usbConfigurationValueTitlePairs.stream()
                    .map(usbConfigurationValueTitlePair -> usbConfigurationValueTitlePair.second)
                    .toArray(String[]::new);
            mUsbConfiguration.updatePreference(listPreference -> {
                listPreference.setEntryValues(usbConfigurationValues);
                listPreference.setEntries(usbConfigurationTitles);
            });
        }
    }

    private List<Pair<String, String>> getSettableUsbConfigurationValueTitlePairs() {
        final String[] values = getResources().getStringArray(R.array.usb_configuration_values);
        final String[] titles = getResources().getStringArray(R.array.usb_configuration_titles);
        final List<Pair<String, String>> settableUsbConfigurationValueTitlePairs =
                new ArrayList<>();
        for (int i = 0; i < values.length; i++) {
            if (UsbManager.areSettableFunctions(UsbManager.usbFunctionsFromString(values[i]))) {
                settableUsbConfigurationValueTitlePairs.add(Pair.create(values[i], titles[i]));
            }
        }
        return settableUsbConfigurationValueTitlePairs;
    }

    private void updateUsbConfigurationValues() {
        final UsbManager manager = (UsbManager) getActivity().getSystemService(Context.USB_SERVICE);
        if (mUsbConfiguration == null) {
            return;
        }
        mUsbConfiguration.updatePreference(p -> p.setVisible(manager != null));
        if (manager != null) {
            final List<Pair<String, String>> usbConfigurationValueTitlePairs =
                    getSettableUsbConfigurationValueTitlePairs();
            int index = 0;
            long functions = manager.getCurrentFunctions();
            for (int i = 0; i < usbConfigurationValueTitlePairs.size(); i++) {
                if ((functions & UsbManager.usbFunctionsFromString(
                        usbConfigurationValueTitlePairs.get(i).first)) != 0) {
                    index = i;
                    break;
                }
            }
            final int updateIndex = index;
            mUsbConfiguration.updatePreference(listPreference -> {
                listPreference.setValue(usbConfigurationValueTitlePairs.get(updateIndex).first);
                listPreference.setSummary(usbConfigurationValueTitlePairs.get(updateIndex).second);
                listPreference.setOnPreferenceChangeListener(this);
            });
        }
    }

    private void writeUsbConfigurationOption(Object newValue) {
        UsbManager manager = (UsbManager) getActivity().getSystemService(Context.USB_SERVICE);
        String function = newValue.toString();
        manager.setCurrentFunctions(UsbManager.usbFunctionsFromString(function));
    }

    private void writeImmediatelyDestroyActivitiesOptions() {
        try {
            ActivityManager.getService().setAlwaysFinish(
                    mImmediatelyDestroyActivities.isChecked());
        } catch (RemoteException ex) {
            // ignore
        }
    }

    private void updateImmediatelyDestroyActivitiesOptions() {
        updateSwitchPreference(mImmediatelyDestroyActivities, Settings.Global.getInt(
                mContentResolver, Settings.Global.ALWAYS_FINISH_ACTIVITIES, 0) != 0);
    }

    private void updateAnimationScaleValue(int which, ListPreference pref) {
        try {
            float scale = mWindowManager.getAnimationScale(which);
            if (scale != 1) {
                mHaveDebugSettings = true;
            }
            CharSequence[] values = pref.getEntryValues();
            for (int i = 0; i < values.length; i++) {
                float val = Float.parseFloat(values[i].toString());
                if (scale <= val) {
                    pref.setValueIndex(i);
                    pref.setSummary(pref.getEntries()[i]);
                    return;
                }
            }
            pref.setValueIndex(values.length - 1);
            pref.setSummary(pref.getEntries()[0]);
        } catch (RemoteException e) {
            // ignore
        }
    }

    private void updateAnimationScaleOptions() {
        updateAnimationScaleValue(0, mWindowAnimationScale);
        updateAnimationScaleValue(1, mTransitionAnimationScale);
        updateAnimationScaleValue(2, mAnimatorDurationScale);
    }

    private void writeAnimationScaleOption(int which, ListPreference pref, Object newValue) {
        try {
            float scale = newValue != null ? Float.parseFloat(newValue.toString()) : 1;
            mWindowManager.setAnimationScale(which, scale);
            updateAnimationScaleValue(which, pref);
        } catch (RemoteException e) {
            // ignore
        }
    }

    private void updateOverlayDisplayDevicesOptions() {
        String value = Settings.Global.getString(mContentResolver,
                Settings.Global.OVERLAY_DISPLAY_DEVICES);
        if (value == null) {
            value = "";
        }

        CharSequence[] values = mOverlayDisplayDevices.getEntryValues();
        for (int i = 0; i < values.length; i++) {
            if (value.contentEquals(values[i])) {
                mOverlayDisplayDevices.setValueIndex(i);
                mOverlayDisplayDevices.setSummary(mOverlayDisplayDevices.getEntries()[i]);
                return;
            }
        }
        mOverlayDisplayDevices.setValueIndex(0);
        mOverlayDisplayDevices.setSummary(mOverlayDisplayDevices.getEntries()[0]);
    }

    private void writeOverlayDisplayDevicesOptions(Object newValue) {
        Settings.Global.putString(mContentResolver, Settings.Global.OVERLAY_DISPLAY_DEVICES,
                (String) newValue);
        updateOverlayDisplayDevicesOptions();
    }

    private void updateOpenGLTracesOptions() {
        String value = DisplayProperties.debug_opengl_trace().orElse("");

        CharSequence[] values = mOpenGLTraces.getEntryValues();
        for (int i = 0; i < values.length; i++) {
            if (value.contentEquals(values[i])) {
                mOpenGLTraces.setValueIndex(i);
                mOpenGLTraces.setSummary(mOpenGLTraces.getEntries()[i]);
                return;
            }
        }
        mOpenGLTraces.setValueIndex(0);
        mOpenGLTraces.setSummary(mOpenGLTraces.getEntries()[0]);
    }

    private void writeOpenGLTracesOptions(Object newValue) {
        DisplayProperties.debug_opengl_trace(newValue == null ? "" : newValue.toString());
        SystemPropPoker.getInstance().poke();
        updateOpenGLTracesOptions();
    }

    private void updateAppProcessLimitOptions() {
        try {
            int limit = ActivityManager.getService().getProcessLimit();
            CharSequence[] values = mAppProcessLimit.getEntryValues();
            for (int i = 0; i < values.length; i++) {
                int val = Integer.parseInt(values[i].toString());
                if (val >= limit) {
                    if (i != 0) {
                        mHaveDebugSettings = true;
                    }
                    mAppProcessLimit.setValueIndex(i);
                    mAppProcessLimit.setSummary(mAppProcessLimit.getEntries()[i]);
                    return;
                }
            }
            mAppProcessLimit.setValueIndex(0);
            mAppProcessLimit.setSummary(mAppProcessLimit.getEntries()[0]);
        } catch (RemoteException e) {
            // ignore
        }
    }

    private void writeAppProcessLimitOptions(Object newValue) {
        try {
            int limit = newValue != null ? Integer.parseInt(newValue.toString()) : -1;
            ActivityManager.getService().setProcessLimit(limit);
            updateAppProcessLimitOptions();
        } catch (RemoteException e) {
            // ignore
        }
    }

    private void writeShowAllANRsOptions() {
        Settings.Secure.putInt(mContentResolver, Settings.Secure.ANR_SHOW_BACKGROUND,
                mShowAllANRs.isChecked() ? 1 : 0);
    }

    private void updateShowAllANRsOptions() {
        updateSwitchPreference(mShowAllANRs, Settings.Secure.getInt(
                mContentResolver, Settings.Secure.ANR_SHOW_BACKGROUND, 0) != 0);
    }

    @Override
    public void onOemUnlockConfirm() {
        mEnableOemUnlock.onOemUnlockConfirm();
        updateAllOptions();
    }

    @Override
    public void onEnableDevelopmentConfirm() {
        mEnableDeveloper.setChecked(true);
        DevelopmentSettingsEnabler.setDevelopmentSettingsEnabled(getContext(), true);
        mLastEnabledState = true;
        setPrefsEnabledState(true);
    }

    @Override
    public void onEnableAdbConfirm() {
        Settings.Global.putInt(mContentResolver, Settings.Global.ADB_ENABLED, 1);
        mEnableAdb.setChecked(true);
        updateVerifyAppsOverUsbOptions();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == RESULT_DEBUG_APP) {
            if (resultCode == Activity.RESULT_OK) {
                mDebugApp = data.getAction();
                writeDebuggerOptions();
                updateDebuggerOptions();
            }
        } else if (requestCode == RESULT_MOCK_LOCATION_APP) {
            if (resultCode == Activity.RESULT_OK) {
                mMockLocationApp = data.getAction();
                writeMockLocation();
                updateMockLocation();
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data);
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (ActivityManager.isUserAMonkey()) {
            return false;
        }

        if (preference == mEnableDeveloper) {
            if (mEnableDeveloper.isChecked()) {
                // Pass to super to launch the dialog, then uncheck until the dialog
                // result comes back
                super.onPreferenceTreeClick(preference);
                mEnableDeveloper.setChecked(false);
            } else {
                resetDangerousOptions();
                DevelopmentSettingsEnabler.setDevelopmentSettingsEnabled(getContext(), false);
                mLastEnabledState = false;
                setPrefsEnabledState(false);
            }
        } else if (preference == mBugreport) {
            captureBugReport(this.getActivity());
        } else if (preference == mEnableAdb) {
            if (mEnableAdb.isChecked()) {
                // Pass to super to launch the dialog, then uncheck until the dialog
                // result comes back
                super.onPreferenceTreeClick(preference);
                mEnableAdb.setChecked(false);
            } else {
                Settings.Global.putInt(mContentResolver, Settings.Global.ADB_ENABLED, 0);
                mVerifyAppsOverUsb.setEnabled(false);
                mVerifyAppsOverUsb.setChecked(false);
            }
        } else if (preference == mEnableUsb) {
            if (mEnableUsb.isChecked()){
                mUsbModeSetting.onUsbModeClick(UsbModeSettings.SLAVE_MODE);
                mEnableUsb.setSummary(R.string.usb_connect_to_computer);
            } else {
                mUsbModeSetting.onUsbModeClick(UsbModeSettings.HOST_MODE);
                mEnableUsb.setSummary(R.string.usb_disconnect_to_computer);
            }
        } else if (preference == mEnableInternetAdb) {
            if (mEnableInternetAdb.isChecked()) {
                SystemProperties.set(PERSIST_RK_INTERNET_ADB, "1");
            } else {
                SystemProperties.set(PERSIST_RK_INTERNET_ADB, "0");
            }
        } else if (preference == mEnableAbc) {
            if (SystemProperties.getInt(PERSIST_RK_ABC_SWITCH, 0) == 1) {
                Log.d(TAG, "set modify abc property to persist 0");
                SystemProperties.set(PERSIST_RK_ABC_SWITCH, "0");
            } else {
                Log.d(TAG, "set modify abc property to persist 1");
                SystemProperties.set(PERSIST_RK_ABC_SWITCH, "1");
                File dirLogs = new File("data/vendor/logs");
                Log.i("ROCKCHIP", "file exists = " + dirLogs.exists());
                if (dirLogs.exists()) {
                    long dirSize = getDirSize(dirLogs);
                    dirSize = dirSize / 1024 / 1024;
                    Log.i("ROCKCHIP", "dirLogs is exists size = " + dirSize);
                    String tipHead = getResources().getString(R.string.abc_tip_head);
                    String tipEnd = getResources().getString(R.string.abc_tip_end);
                    Toast.makeText(getActivity(), tipHead + dirSize + tipEnd, Toast.LENGTH_LONG).show();
                }
            }
        } else if (preference == mEnableTerminal) {
            final PackageManager pm = getActivity().getPackageManager();
            pm.setApplicationEnabledSetting(TERMINAL_APP_PACKAGE,
                    mEnableTerminal.isChecked() ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                            : PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, 0);
        } else if (preference == mKeepScreenOn) {
            Settings.Global.putInt(mContentResolver, Settings.Global.STAY_ON_WHILE_PLUGGED_IN,
                    mKeepScreenOn.isChecked() ?
                            (BatteryManager.BATTERY_PLUGGED_AC | BatteryManager.BATTERY_PLUGGED_USB)
                            : 0);
        } else if (preference == mEnableOemUnlock.getPreference()) {
            if (!mEnableOemUnlock.onPreferenceClick()) {
                // Pass to super to launch the confirmation dialog.
                super.onPreferenceTreeClick(preference);
            }
        } else if (preference == mMockLocationAppPref) {
            Intent intent = new Intent(getActivity(), AppPicker.class);
            intent.putExtra(AppPicker.EXTRA_REQUESTIING_PERMISSION,
                    Manifest.permission.ACCESS_MOCK_LOCATION);
            startActivityForResult(intent, RESULT_MOCK_LOCATION_APP);
        } else if (preference == mDebugViewAttributes) {
            Settings.Global.putInt(mContentResolver, Settings.Global.DEBUG_VIEW_ATTRIBUTES,
                    mDebugViewAttributes.isChecked() ? 1 : 0);
        } else if (preference == mForceAllowOnExternal) {
            Settings.Global.putInt(mContentResolver, Settings.Global.FORCE_ALLOW_ON_EXTERNAL,
                    mForceAllowOnExternal.isChecked() ? 1 : 0);
        } else if (preference == mDebugAppPref) {
            Intent intent = new Intent(getActivity(), AppPicker.class);
            intent.putExtra(AppPicker.EXTRA_DEBUGGABLE, true);
            startActivityForResult(intent, RESULT_DEBUG_APP);
        } else if (preference == mWaitForDebugger) {
            writeDebuggerOptions();
        } else if (preference == mVerifyAppsOverUsb) {
            writeVerifyAppsOverUsbOptions();
        } else if (preference == mStrictMode) {
            writeStrictModeVisualOptions();
        } else if (preference == mPointerLocation) {
            writePointerLocationOptions();
        } else if (preference == mShowTouches) {
            writeShowTouchesOptions();
        } else if (preference == mShowScreenUpdates) {
            writeShowUpdatesOption();
        } else if (preference == mDisableOverlays) {
            writeDisableOverlaysOption();
        } else if (preference == mImmediatelyDestroyActivities) {
            writeImmediatelyDestroyActivitiesOptions();
        } else if (preference == mShowAllANRs) {
            writeShowAllANRsOptions();
        } else if (preference == mForceMsaa) {
            writeMsaaOptions();
        } else if (preference == mShowHwScreenUpdates) {
            writeShowHwScreenUpdatesOptions();
        } else if (preference == mShowHwLayersUpdates) {
            writeShowHwLayersUpdatesOptions();
        } else if (preference == mDebugLayout) {
            writeDebugLayoutOptions();
        } else if (preference == mForceRtlLayout) {
            writeForceRtlOptions();
        } else if (preference == mWindowBlurs) {
            writeWindowBlursOptions();
        } else if (preference == mWifiDisplayCertification) {
            writeWifiDisplayCertificationOptions();
        } else if (preference == mWifiVerboseLogging) {
            writeWifiVerboseLoggingOptions();
        } else if (preference == mMobileDataAlwaysOn) {
            writeMobileDataAlwaysOnOptions();
        } else if (preference == mUSBAudio) {
            writeUSBAudioOptions();
        } else if (preference == mForceResizable) {
            writeForceResizableOptions();
        } else if (preference == mRecordAudio) {
            writeRecordAudioOptions();
        } else if (preference == mSaveAudio) {
            saveRecordedAudio();
        } else if (preference == mPlayRecordedAudio) {
            playRecordedAudio();
        } else {
            return super.onPreferenceTreeClick(preference);
        }

        return false;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (HDCP_CHECKING_KEY.equals(preference.getKey())) {
            SystemProperties.set(HDCP_CHECKING_PROPERTY, newValue.toString());
            updateHdcpValues();
            SystemPropPoker.getInstance().poke();
            return true;
        } else if (preference == mUsbConfiguration.getOriginalPreference()) {
            writeUsbConfigurationOption(newValue);
            return true;
        } else if (preference == mWindowAnimationScale) {
            writeAnimationScaleOption(0, mWindowAnimationScale, newValue);
            return true;
        } else if (preference == mTransitionAnimationScale) {
            writeAnimationScaleOption(1, mTransitionAnimationScale, newValue);
            return true;
        } else if (preference == mAnimatorDurationScale) {
            writeAnimationScaleOption(2, mAnimatorDurationScale, newValue);
            return true;
        } else if (preference == mOverlayDisplayDevices) {
            writeOverlayDisplayDevicesOptions(newValue);
            return true;
        } else if (preference == mOpenGLTraces) {
            writeOpenGLTracesOptions(newValue);
            return true;
        } else if (preference == mTrackFrameTime) {
            writeTrackFrameTimeOptions(newValue);
            return true;
        } else if (preference == mDebugHwOverdraw) {
            writeDebugHwOverdrawOptions(newValue);
            return true;
        } else if (preference == mShowNonRectClip) {
            writeShowNonRectClipOptions(newValue);
            return true;
        } else if (preference == mAppProcessLimit) {
            writeAppProcessLimitOptions(newValue);
            return true;
        } else if (preference == mSimulateColorSpace) {
            writeSimulateColorSpace(newValue);
            return true;
        } else if (preference == mBtHciSnoopLog) {
            writeBtHciSnoopLogOptions(newValue);
            return true;
        } else if (preference == mRecordAudioSource) {
            /* Just keep the value in the preference, but accept the change */
            return true;
        }
        return false;
    }

    /**
     * Iterates through preference controllers that show confirmation dialogs and returns the
     * preference key for the first currently showing dialog. Ideally there should only ever be one.
     *
     * @return Preference key, or null if no dialog is showing
     */
    private String getKeyForShowingDialog() {
        // TODO: iterate through a fragment-wide list of PreferenceControllers and just pick out the
        // ConfirmationDialogController objects
        final List<ConfirmationDialogController> dialogControllers = new ArrayList<>(2);
        dialogControllers.add(mLogpersistController);
        for (ConfirmationDialogController dialogController : dialogControllers) {
            if (dialogController.isConfirmationDialogShowing()) {
                return dialogController.getPreferenceKey();
            }
        }
        return null;
    }

    /**
     * Re-show the dialog we lost previously
     *
     * @param preferenceKey Key for the preference the dialog is for
     */
    private void recreateDialogForKey(String preferenceKey) {
        // TODO: iterate through a fragment-wide list of PreferenceControllers and just pick out the
        // ConfirmationDialogController objects
        final List<ConfirmationDialogController> dialogControllers = new ArrayList<>(2);
        dialogControllers.add(mLogpersistController);
        for (ConfirmationDialogController dialogController : dialogControllers) {
            if (TextUtils.equals(preferenceKey, dialogController.getPreferenceKey())) {
                dialogController.showConfirmationDialog(findPreference(preferenceKey));
            }
        }
    }

    private void dismissDialogs() {
        mLogpersistController.dismissConfirmationDialog();
    }

    private final BroadcastReceiver mUsbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            updateUsbConfigurationValues();
        }
    };

    private static boolean isPackageInstalled(Context context, String packageName) {
        try {
            return context.getPackageManager().getPackageInfo(packageName, 0) != null;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    private void updateWirelessDebuggingPreference() {
        if (mWirelessDebugging == null) {
            return;
        }

        if (!isNetworkConnected()) {
            if (FlavorUtils.isTwoPanel(getContext())) {
                mWirelessDebugging.setFragment(WirelessDebuggingInfoFragment.class.getName());
            }
            mWirelessDebugging.setSummary(R.string.connectivity_summary_no_network_connected);
        } else {
            if (FlavorUtils.isTwoPanel(getContext())) {
                mWirelessDebugging.setFragment(WirelessDebuggingFragment.class.getName());
            }
            boolean enabled = Settings.Global.getInt(mContentResolver,
                    Settings.Global.ADB_WIFI_ENABLED, 1) != 0;
            if (enabled) {
                mWirelessDebugging.setSummary(R.string.enabled);
            } else {
                mWirelessDebugging.setSummary(R.string.disabled);
            }
        }
    }

    private boolean isNetworkConnected() {
        NetworkInfo activeNetworkInfo = mConnectivityManager.getActiveNetworkInfo();
        return activeNetworkInfo != null && activeNetworkInfo.isConnected();
    }

    private class NetworkCallback extends ConnectivityManager.NetworkCallback {
        @Override
        public void onAvailable(Network network) {
            super.onAvailable(network);
            mHandler.post(() -> updateWirelessDebuggingPreference());
        }

        @Override
        public void onLost(Network network) {
            super.onLost(network);
            mHandler.post(() -> updateWirelessDebuggingPreference());
        }
    }

    private void goToLoader() {
        try{
            java.lang.Runtime.getRuntime().exec("reboot loader");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private long getDirSize(File dirOrFile) {
        long dirSize = 0;
        if (dirOrFile.exists()) {
            if (dirOrFile.isDirectory()) {
                File[] files = dirOrFile.listFiles();
                for (File file : files) {
                    dirSize += getDirSize(file);
                }
            } else {
                dirSize += dirOrFile.length();
            }
        }
        return dirSize;
    }
}
