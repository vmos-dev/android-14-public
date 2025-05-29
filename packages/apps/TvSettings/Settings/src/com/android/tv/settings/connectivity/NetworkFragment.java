/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.settings.connectivity;

import static com.android.tv.settings.overlay.FlavorUtils.FLAVOR_CLASSIC;
import static com.android.tv.settings.overlay.FlavorUtils.FLAVOR_TWO_PANEL;
import static com.android.tv.settings.overlay.FlavorUtils.FLAVOR_VENDOR;
import static com.android.tv.settings.overlay.FlavorUtils.FLAVOR_X;
import static com.android.tv.settings.util.InstrumentationUtils.logEntrySelected;
import static com.android.tv.settings.util.InstrumentationUtils.logToggleInteracted;

import android.app.AppOpsManager;
import android.app.tvsettings.TvSettingsEnums;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.ConnectivityManager.NetworkCallback;
import android.net.ConnectivityManager;
import android.net.VpnManager;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.os.SystemProperties;
import android.os.UserManager;
import android.provider.Settings;
import android.security.Credentials;
import android.security.LegacyVpnProfileStore;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Log;

import androidx.annotation.UiThread;
import androidx.annotation.WorkerThread;
import androidx.annotation.Keep;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceManager;
import androidx.preference.TwoStatePreference;

import com.android.settingslib.RestrictedLockUtils;
import com.android.settingslib.RestrictedPreference;
import com.android.settingslib.wifi.AccessPoint;
import com.android.internal.net.LegacyVpnInfo;
import com.android.internal.net.VpnConfig;
import com.android.internal.net.VpnProfile;
import com.android.internal.util.ArrayUtils;
import com.android.tv.settings.MainFragment;
import com.android.tv.settings.data.ConstData;
import com.android.tv.settings.R;
import com.android.tv.settings.RestrictedPreferenceAdapter;
import com.android.tv.settings.SettingsPreferenceFragment;
import com.android.tv.settings.overlay.FlavorUtils;
import com.android.tv.settings.util.SliceUtils;
import com.android.tv.settings.vpn.*;
import com.android.tv.settings.vpn.GearPreference.OnGearClickListener;
import com.android.tv.settings.widget.CustomContentDescriptionSwitchPreference;
import com.android.tv.settings.widget.TvAccessPointPreference;
import com.android.tv.twopanelsettings.slices.SlicePreference;
import com.google.android.collect.Lists;

import java.util.Collection;
import java.util.HashSet;
import java.util.Map;
import java.util.List;
import java.util.Set;
import java.util.ArrayList;
import java.util.Collections;

/**
 * Fragment for controlling network connectivity
 */
@Keep
public class NetworkFragment extends SettingsPreferenceFragment implements
        ConnectivityListener.Listener, ConnectivityListener.WifiNetworkListener,
        AccessPoint.AccessPointListener, Preference.OnPreferenceClickListener, Handler.Callback {
    private static final String TAG = "NetworkFragment";

    private static final String KEY_WIFI_ENABLE = "wifi_enable";
    private static final String KEY_WIFI_LIST = "wifi_list";
    private static final String KEY_WIFI_COLLAPSE = "wifi_collapse";
    private static final String KEY_WIFI_OTHER = "wifi_other";
    private static final String KEY_WIFI_ADD = "wifi_add";
    private static final String KEY_WIFI_ADD_EASYCONNECT = "wifi_add_easyconnect";
    private static final String KEY_WIFI_ALWAYS_SCAN = "wifi_always_scan";
    private static final String KEY_ETHERNET = "ethernet";
    private static final String KEY_ETHERNET_STATUS = "ethernet_status";
    private static final String KEY_ETHERNET_PROXY = "ethernet_proxy";
    private static final String KEY_ETHERNET_DHCP = "ethernet_dhcp";
    private static final String KEY_DATA_SAVER_SLICE = "data_saver_slice";
    private static final String KEY_DATA_ALERT_SLICE = "data_alert_slice";
    private static final String KEY_NETWORK_DIAGNOSTICS = "network_diagnostics";

    private static final String ACTION_DATA_ALERT_SETTINGS = "android.settings.DATA_ALERT_SETTINGS";
    private static final String KEY_HOTSPOT = "hotspot";
    private static final String KEY_VPN = "avaliable_vpns";
    private static final String KEY_EDIT_VPN = "edit_vpn";

    private static final int INITIAL_UPDATE_DELAY = 500;

    private static final String NETWORK_DIAGNOSTICS_ACTION =
            "com.android.tv.settings.network.NETWORK_DIAGNOSTICS";

    private ConnectivityListener mConnectivityListener;
    private WifiManager mWifiManager;
    private ConnectivityManager mConnectivityManager;
    private VpnManager mVpnManager;
    private TvAccessPointPreference.UserBadgeCache mUserBadgeCache;

    private TwoStatePreference mEnableWifiPref;
    private CollapsibleCategory mWifiNetworksCategory;
    private Preference mCollapsePref;
    private RestrictedPreference mAddPref;
    private RestrictedPreference mAddEasyConnectPref;
    private TwoStatePreference mAlwaysScan;
    private PreferenceCategory mEthernetCategory;
    private PreferenceCategory mVpnCategory;
    private Preference mEthernetStatusPref;
    private Preference mEthernetProxyPref;
    private Preference mEthernetDhcpPref;
    private PreferenceCategory mWifiOther;
    private Preference mHotsPot;

    private final Handler mHandler = new Handler();
    private long mNoWifiUpdateBeforeMillis;
    private Preference mVpnCreatePref;
    private LegacyVpnInfo mConnectedLegacyVpn;
    private Handler mUpdater;
    private static final int RESCAN_MESSAGE = 0;
    private static final int RESCAN_INTERVAL_MS = 1000;
    private Map<String, LegacyVpnPreference> mLegacyVpnPreferences = new ArrayMap<>();
    private Runnable mInitialUpdateWifiListRunnable = new Runnable() {
        @Override
        public void run() {
            mNoWifiUpdateBeforeMillis = 0;
            updateWifiList();
        }
    };
    private boolean mIsWifiHardwarePresent;

    public static NetworkFragment newInstance() {
        return new NetworkFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mIsWifiHardwarePresent = getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_WIFI);
        mConnectivityListener = new ConnectivityListener(
                getContext(), this, getSettingsLifecycle());
        mWifiManager = getContext().getSystemService(WifiManager.class);
        mConnectivityManager = getContext().getSystemService(ConnectivityManager.class);
        mUserBadgeCache =
                new TvAccessPointPreference.UserBadgeCache(getContext().getPackageManager());
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onStart() {
        super.onStart();
        mConnectivityListener.setWifiListener(this);
        mNoWifiUpdateBeforeMillis = SystemClock.elapsedRealtime() + INITIAL_UPDATE_DELAY;
        updateWifiList();
    }

    @Override
    public void onResume() {
        super.onResume();
        // There doesn't seem to be an API to listen to everything this could cover, so
        // tickle it here and hope for the best.
        updateConnectivity();
        if (mUpdater == null) {
            mUpdater = new Handler(this);
        }
        mUpdater.sendEmptyMessage(RESCAN_MESSAGE);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mUpdater != null) {
            mUpdater.removeCallbacksAndMessages(null);
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        mConnectivityListener.stop();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mConnectivityListener != null) {
            mConnectivityListener.destroy();
        }
    }

    private int getPreferenceScreenResId() {
        switch (FlavorUtils.getFlavor(getContext())) {
            case FLAVOR_CLASSIC:
            case FLAVOR_TWO_PANEL:
                return R.xml.network;
            case FLAVOR_X:
            case FLAVOR_VENDOR:
                return R.xml.network_x;
            default:
                return R.xml.network;
        }
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getPreferenceManager().setPreferenceComparisonCallback(
                new PreferenceManager.SimplePreferenceComparisonCallback());
        setPreferencesFromResource(getPreferenceScreenResId(), null);

        mEnableWifiPref = (TwoStatePreference) findPreference(KEY_WIFI_ENABLE);
        mWifiNetworksCategory = (CollapsibleCategory) findPreference(KEY_WIFI_LIST);
        mCollapsePref = findPreference(KEY_WIFI_COLLAPSE);
        mAddPref = (RestrictedPreference) findPreference(KEY_WIFI_ADD);
        mAddEasyConnectPref = (RestrictedPreference) findPreference(KEY_WIFI_ADD_EASYCONNECT);
        mAlwaysScan = (TwoStatePreference) findPreference(KEY_WIFI_ALWAYS_SCAN);
        mWifiOther = (PreferenceCategory) findPreference(KEY_WIFI_OTHER);
        mHotsPot = findPreference(KEY_HOTSPOT);

        mEthernetCategory = (PreferenceCategory) findPreference(KEY_ETHERNET);
        mEthernetStatusPref = findPreference(KEY_ETHERNET_STATUS);
        mEthernetProxyPref = findPreference(KEY_ETHERNET_PROXY);
        mEthernetDhcpPref = findPreference(KEY_ETHERNET_DHCP);

        if (!mIsWifiHardwarePresent) {
            mEnableWifiPref.setVisible(false);
        }

        updateVisibilityForDataSaver();
        Preference dataSaverSlicePref = findPreference(KEY_DATA_SAVER_SLICE);
        Preference dataAlertSlicePref = findPreference(KEY_DATA_ALERT_SLICE);
        Intent i = getActivity().getIntent();
        if (i != null && i.getAction() != null) {
            if (i.getAction().equals(Settings.ACTION_DATA_SAVER_SETTINGS)
                    && dataSaverSlicePref.isVisible()) {
                mHandler.post(() -> scrollToPreference(dataSaverSlicePref));
            } else if (i.getAction().equals(ACTION_DATA_ALERT_SETTINGS)
                    && dataAlertSlicePref.isVisible()) {
                mHandler.post(() -> scrollToPreference(dataAlertSlicePref));
            }
        }
        mVpnCategory = (PreferenceCategory) findPreference(KEY_VPN);
        getPreferenceScreen().removePreference(mVpnCategory);
        getPreferenceScreen().removePreference((PreferenceCategory) findPreference("vpn"));
        if (!SystemProperties.get("ro.target.product","box").equals("box")) {
            getPreferenceScreen().removePreference(mHotsPot);
        }

        Preference networkDiagnosticsPref = findPreference(KEY_NETWORK_DIAGNOSTICS);
        Intent networkDiagnosticsIntent = makeNetworkDiagnosticsIntent();
        if (networkDiagnosticsIntent != null) {
            networkDiagnosticsPref.setVisible(true);
            networkDiagnosticsPref.setIntent(networkDiagnosticsIntent);
        } else {
            networkDiagnosticsPref.setVisible(false);
        }

        final UserManager userManager = UserManager.get(getContext());
        if (userManager.hasUserRestriction(UserManager.DISALLOW_CONFIG_WIFI)) {
            mAddPref.setFragment(null);
            mAddEasyConnectPref.setFragment(null);

            if (!mAddPref.isDisabledByAdmin()) {
                mAddPref.setEnabled(false);
            }
            if (!mAddEasyConnectPref.isDisabledByAdmin()) {
                mAddEasyConnectPref.setEnabled(false);
            }
        }
    }

    private void updateVisibilityForDataSaver() {
        Preference dataSaverSlicePref = findPreference(KEY_DATA_SAVER_SLICE);
        Preference dataAlertSlicePref = findPreference(KEY_DATA_ALERT_SLICE);
        boolean isDataSaverVisible = isConnected() && SliceUtils.isSliceProviderValid(
                getContext(), ((SlicePreference) dataSaverSlicePref).getUri());
        boolean isDataAlertVisible = isConnected() && SliceUtils.isSliceProviderValid(
                getContext(), ((SlicePreference) dataAlertSlicePref).getUri());
        dataSaverSlicePref.setVisible(isDataSaverVisible);
        dataAlertSlicePref.setVisible(isDataAlertVisible);
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (preference.getKey() == null) {
            return super.onPreferenceTreeClick(preference);
        }
        switch (preference.getKey()) {
            case KEY_WIFI_ENABLE:
                mConnectivityListener.setWifiEnabled(mEnableWifiPref.isChecked());
                logToggleInteracted(
                        TvSettingsEnums.NETWORK_WIFI_ON_OFF, mEnableWifiPref.isChecked());
                return true;
            case KEY_WIFI_COLLAPSE:
                final boolean collapse = !mWifiNetworksCategory.isCollapsed();
                mCollapsePref.setTitle(collapse
                        ? R.string.wifi_setting_see_all : R.string.wifi_setting_see_fewer);
                mWifiNetworksCategory.setCollapsed(collapse);
                logEntrySelected(
                        collapse
                                ? TvSettingsEnums.NETWORK_SEE_FEWER
                                : TvSettingsEnums.NETWORK_SEE_ALL);
                return true;
            case KEY_WIFI_ALWAYS_SCAN:
                Settings.Global.putInt(getActivity().getContentResolver(),
                        Settings.Global.WIFI_SCAN_ALWAYS_AVAILABLE,
                        mAlwaysScan.isChecked() ? 1 : 0);
                logToggleInteracted(
                        TvSettingsEnums.NETWORK_ALWAYS_SCANNING_NETWORKS, mAlwaysScan.isChecked());
                return true;
            case KEY_ETHERNET_STATUS:
                return true;
            case KEY_WIFI_ADD:
                logEntrySelected(TvSettingsEnums.NETWORK_ADD_NEW_NETWORK);
                break;
            case KEY_WIFI_ADD_EASYCONNECT:
                startActivity(AddWifiNetworkActivity.createEasyConnectIntent(getContext()));
                break;
        }
        return super.onPreferenceTreeClick(preference);
    }

    private boolean isConnected() {
        NetworkInfo activeNetworkInfo = mConnectivityManager.getActiveNetworkInfo();
        return activeNetworkInfo != null && activeNetworkInfo.isConnected()
                && ConnectivityManager.TYPE_ETHERNET != activeNetworkInfo.getType();
    }

    private void updateConnectivity() {
        if (!isAdded()) {
            return;
        }

        final boolean wifiEnabled = mIsWifiHardwarePresent
                && mConnectivityListener.isWifiEnabledOrEnabling();
        mEnableWifiPref.setChecked(wifiEnabled);

        mWifiNetworksCategory.setVisible(wifiEnabled);
        mCollapsePref.setVisible(wifiEnabled && mWifiNetworksCategory.shouldShowCollapsePref());
        mAddPref.setVisible(wifiEnabled);
        if (mAddEasyConnectPref != null) {
            mAddEasyConnectPref.setVisible(isEasyConnectEnabled());
        }

        if (!wifiEnabled) {
            updateWifiList();
        }

        int scanAlwaysAvailable = 0;
        try {
            scanAlwaysAvailable = Settings.Global.getInt(getContext().getContentResolver(),
                    Settings.Global.WIFI_SCAN_ALWAYS_AVAILABLE);
        } catch (Settings.SettingNotFoundException e) {
            // Ignore
        }
        mAlwaysScan.setChecked(scanAlwaysAvailable == 1);
        if (mAlwaysScan instanceof CustomContentDescriptionSwitchPreference) {
            ((CustomContentDescriptionSwitchPreference) mAlwaysScan).setContentDescription(
                    getResources()
                            .getString(
                                    R.string.wifi_setting_always_scan_content_description));
        }

        final boolean ethernetAvailable = mConnectivityListener.isEthernetAvailable();
        mEthernetCategory.setVisible(ethernetAvailable);
        mEthernetStatusPref.setVisible(ethernetAvailable);
        mEthernetProxyPref.setVisible(ethernetAvailable);
        if (ethernetAvailable) {
            mEthernetProxyPref.setIntent(EditProxySettingsActivity.createEthernetIntent(
                    getContext(),
                    mConnectivityListener.getEthernetInterfaceName(),
                    mConnectivityListener.getEthernetIpConfiguration()));
        }
        mEthernetProxyPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_ETHERNET_PROXY_SETTINGS);
                    return false;
                });

        mEthernetDhcpPref.setVisible(ethernetAvailable);
        if (ethernetAvailable) {
            mEthernetDhcpPref.setIntent(EditIpSettingsActivity.createEthernetIntent(getContext(),
                    mConnectivityListener.getEthernetInterfaceName(),
                    mConnectivityListener.getEthernetIpConfiguration()));
        }
        mEthernetDhcpPref.setOnPreferenceClickListener(
                preference -> {
                    logEntrySelected(TvSettingsEnums.NETWORK_ETHERNET_IP_SETTINGS);
                    return false;
                });

        if (ethernetAvailable) {
            final boolean ethernetConnected =
                    mConnectivityListener.isEthernetConnected();
            mEthernetStatusPref.setTitle(ethernetConnected
                    ? R.string.connected : R.string.not_connected);
            mEthernetStatusPref.setSummary(mConnectivityListener.getEthernetIpAddress());
        }

        updateVisibilityForDataSaver();
    }

    private void updateVPNList() {
        getActivity().runOnUiThread(new Runnable() {
            public void run() {

                List<VpnProfile> vpnProfiles = loadVpnProfiles();
                String lockdownVpnKey = VpnUtils.getLockdownVpn();
                final Set<Preference> updates = new ArraySet<>();
                Map<String, LegacyVpnInfo> connectedLegacyVpns = getConnectedLegacyVpns();
                for (VpnProfile profile : vpnProfiles) {
                    LegacyVpnPreference p = findOrCreatePreference(profile);
                    if (connectedLegacyVpns.containsKey(profile.key)) {
                        p.setState(connectedLegacyVpns.get(profile.key).state);
                    } else {
                        p.setState(LegacyVpnPreference.STATE_NONE);
                    }
                    p.setAlwaysOn(lockdownVpnKey != null && lockdownVpnKey.equals(profile.key));
                    updates.add(p);
                }
                mLegacyVpnPreferences.values().retainAll(updates);
                for (int i = mVpnCategory.getPreferenceCount() - 1; i >= 0; i--) {
                    Preference p = mVpnCategory.getPreference(i);
                    if (updates.contains(p)) {
                        updates.remove(p);
                    } /*else if(!"vpn_create".equals(p.getKey())){
                         mVpnCategory.removePreference(p);
                     }*/
                }
                // Show any new preferences on the screen
                for (Preference pref : updates) {
                    mVpnCategory.addPreference(pref);
                }
 /*             Preference vpnCreatePref = new Preference(getPreferenceManager().getContext());
                vpnCreatePref.setTitle(R.string.create_vpn);
                Intent createVpnIntent = new Intent();
                createVpnIntent.setClassName(getActivity().getPackageName(), VpnCreateActivity.class.getName());
                VpnProfile createProfile = new VpnProfile(Long.toHexString(System.currentTimeMillis()));
                createVpnIntent.putExtra(ConstData.IntentKey.VPN_PROFILE, createProfile);
                createVpnIntent.putExtra(ConstData.IntentKey.VPN_EXIST, false);
                createVpnIntent.putExtra(ConstData.IntentKey.VPN_EDITING, true);
                vpnCreatePref.setIntent(createVpnIntent);
                mVpnCategory.addPreference(vpnCreatePref);*/
            }

            ;
        });

    }

    private void updateWifiList() {
        if (!isAdded()) {
            return;
        }

        if (!mIsWifiHardwarePresent || !mConnectivityListener.isWifiEnabledOrEnabling()) {
            mWifiNetworksCategory.removeAll();
            mNoWifiUpdateBeforeMillis = 0;
            return;
        }

        final long now = SystemClock.elapsedRealtime();
        if (mNoWifiUpdateBeforeMillis > now) {
            mHandler.removeCallbacks(mInitialUpdateWifiListRunnable);
            mHandler.postDelayed(mInitialUpdateWifiListRunnable,
                    mNoWifiUpdateBeforeMillis - now);
            return;
        }

        final int existingCount = mWifiNetworksCategory.getRealPreferenceCount();
        final Set<Preference> toRemove = new HashSet<>(existingCount);
        for (int i = 0; i < existingCount; i++) {
            toRemove.add(mWifiNetworksCategory.getPreference(i));
        }

        final Context themedContext = getPreferenceManager().getContext();
        final Collection<AccessPoint> accessPoints = mConnectivityListener.getAvailableNetworks();
        int index = 0;

        for (final AccessPoint accessPoint : accessPoints) {
            accessPoint.setListener(this);
            RestrictedPreferenceAdapter<TvAccessPointPreference> restrictedPref =
                    (RestrictedPreferenceAdapter<TvAccessPointPreference>) accessPoint.getTag();
            Preference pref;
            if (restrictedPref == null) {
                pref = new TvAccessPointPreference(accessPoint, themedContext, mUserBadgeCache,
                        false);
                restrictedPref = new RestrictedPreferenceAdapter(themedContext, pref,
                        UserManager.DISALLOW_CONFIG_WIFI);
                accessPoint.setTag(restrictedPref);
            } else {
                toRemove.remove(restrictedPref.getPreference());
                pref = restrictedPref.getOriginalPreference();
            }
            if (accessPoint.isActive() && !isCaptivePortal(accessPoint)) {
                pref.setFragment(WifiDetailsFragment.class.getName());
                // No need to track entry selection as new page will be focused
                pref.setOnPreferenceClickListener(preference -> false);
                WifiDetailsFragment.prepareArgs(pref.getExtras(), accessPoint);
                pref.setIntent(null);
            } else {
                pref.setFragment(null);
                pref.setIntent(WifiConnectionActivity.createIntent(getContext(), accessPoint));
                pref.setOnPreferenceClickListener(
                        preference -> {
                            logEntrySelected(TvSettingsEnums.NETWORK_NOT_CONNECTED_AP);
                            return false;
                        });
            }
            pref.setVisible(!restrictedPref.isRestricted() || accessPoint.isSaved());
            pref.setOrder(index++);
            restrictedPref.updatePreference();

            // Double-adding is harmless
            mWifiNetworksCategory.addPreference(restrictedPref.getPreference());
        }

        for (final Preference preference : toRemove) {
            mWifiNetworksCategory.removePreference(preference);
        }

        mCollapsePref.setVisible(mWifiNetworksCategory.shouldShowCollapsePref());
    }

    private boolean isCaptivePortal(AccessPoint accessPoint) {
        if (accessPoint.getDetailedState() != NetworkInfo.DetailedState.CONNECTED) {
            return false;
        }
        NetworkCapabilities nc = mConnectivityManager.getNetworkCapabilities(
                mWifiManager.getCurrentNetwork());
        return nc != null && nc.hasCapability(NetworkCapabilities.NET_CAPABILITY_CAPTIVE_PORTAL);
    }

    private Intent makeNetworkDiagnosticsIntent() {
        Intent intent = new Intent();
        intent.setAction(NETWORK_DIAGNOSTICS_ACTION);

        ResolveInfo resolveInfo = MainFragment.systemIntentIsHandled(getContext(), intent);
        if (resolveInfo == null || resolveInfo.activityInfo == null) {
            return null;
        }

        intent.setPackage(resolveInfo.activityInfo.packageName);

        return intent;
    }

    @Override
    public void onConnectivityChange() {
        updateConnectivity();
    }

    @Override
    public void onWifiListChanged() {
        updateWifiList();
    }

    @Override
    public void onAccessPointChanged(AccessPoint accessPoint) {
        RestrictedPreferenceAdapter<TvAccessPointPreference> restrictedPref =
                (RestrictedPreferenceAdapter<TvAccessPointPreference>) accessPoint.getTag();
        restrictedPref.updatePreference(pref -> pref.refresh());
    }

    @Override
    public void onLevelChanged(AccessPoint accessPoint) {
        RestrictedPreferenceAdapter<TvAccessPointPreference> restrictedPref =
                (RestrictedPreferenceAdapter<TvAccessPointPreference>) accessPoint.getTag();
        restrictedPref.updatePreference(pref -> pref.onLevelChanged());
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.NETWORK;
    }

    private boolean isEasyConnectEnabled() {
        final boolean wifiEnabled = mIsWifiHardwarePresent
                && mConnectivityListener.isWifiEnabledOrEnabling();

        if (!wifiEnabled || !mWifiManager.isEasyConnectSupported()) {
            return false;
        }

        return getContext().getResources().getBoolean(R.bool.config_easyconnect_enabled);
    }

    static List<VpnProfile> loadVpnProfiles() {
        final ArrayList<VpnProfile> result = Lists.newArrayList();

        for (String key : LegacyVpnProfileStore.list(Credentials.VPN)) {
            final VpnProfile profile = VpnProfile.decode(key, LegacyVpnProfileStore.get(Credentials.VPN + key));
            if (profile != null) {
                result.add(profile);
            }
        }
        return result;
    }

    @WorkerThread
    private Map<String, LegacyVpnInfo> getConnectedLegacyVpns() {
        mConnectedLegacyVpn = mVpnManager.getLegacyVpnInfo(UserHandle.myUserId());
        if (mConnectedLegacyVpn != null) {
            return Collections.singletonMap(mConnectedLegacyVpn.key, mConnectedLegacyVpn);
        }
        return Collections.emptyMap();
    }

    @UiThread
    private LegacyVpnPreference findOrCreatePreference(VpnProfile profile) {
        LegacyVpnPreference pref = mLegacyVpnPreferences.get(profile.key);
        if (pref == null) {
            pref = new LegacyVpnPreference(getPreferenceManager().getContext());
            //pref.setOnGearClickListener(mGearListener);
            pref.setOnPreferenceClickListener(this);
            mLegacyVpnPreferences.put(profile.key, pref);
        }
        // This may change as the profile can update and keep the same key.
        pref.setProfile(profile);
        return pref;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference instanceof LegacyVpnPreference) {
            LegacyVpnPreference pref = (LegacyVpnPreference) preference;
            VpnProfile profile = pref.getProfile();
            if (mConnectedLegacyVpn != null && profile.key.equals(mConnectedLegacyVpn.key) &&
                    mConnectedLegacyVpn.state == LegacyVpnInfo.STATE_CONNECTED) {
                try {
                    mConnectedLegacyVpn.intent.send();
                    return true;
                } catch (Exception e) {
                    Log.w(TAG, "Starting config intent failed", e);
                }
            }
            Intent prefIntent = new Intent();
            prefIntent.setClass(getContext(), VpnCreateActivity.class);
            prefIntent.putExtra(ConstData.IntentKey.VPN_PROFILE, profile);
            prefIntent.putExtra(ConstData.IntentKey.VPN_EDITING, false);
            prefIntent.putExtra(ConstData.IntentKey.VPN_EXIST, true);
            startActivity(prefIntent);
            //ConfigDialogFragment.show(this, profile, false /* editing */, true /* exists */);
            return true;
        } /*else if (preference instanceof AppPreference) {
            AppPreference pref = (AppPreference) preference;
            boolean connected = (pref.getState() == AppPreference.STATE_CONNECTED);

            if (!connected) {
                try {
                    UserHandle user = UserHandle.of(pref.getUserId());
                    Context userContext = getActivity().createPackageContextAsUser(
                            getActivity().getPackageName(), 0  flags , user);
                    PackageManager pm = userContext.getPackageManager();
                    Intent appIntent = pm.getLaunchIntentForPackage(pref.getPackageName());
                    if (appIntent != null) {
                        userContext.startActivityAsUser(appIntent, user);
                        return true;
                    }
                } catch (PackageManager.NameNotFoundException nnfe) {
                    Log.w(LOG_TAG, "VPN provider does not exist: " + pref.getPackageName(), nnfe);
                }
            }

            // Already connected or no launch intent available - show an info dialog
            PackageInfo pkgInfo = pref.getPackageInfo();
            AppDialogFragment.show(this, pkgInfo, pref.getLabel(), false  editing , connected);
            return true;
        }else{
            createVPN();
        }*/
        return false;
    }

    @Override
    public boolean handleMessage(Message message) {
        mUpdater.removeMessages(RESCAN_MESSAGE);
        mUpdater.sendEmptyMessageDelayed(RESCAN_MESSAGE, RESCAN_INTERVAL_MS);
        return true;
    }
}
