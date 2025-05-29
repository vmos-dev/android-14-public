/*
 * Copyright (C) 2023 The Android Open Source Project
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

package com.android.wifitrackerlib;

import static android.net.wifi.ScanResult.WIFI_STANDARD_11N;
import static android.net.wifi.WifiInfo.SECURITY_TYPE_PSK;
import static android.net.wifi.WifiInfo.SECURITY_TYPE_SAE;

import static com.android.wifitrackerlib.WifiEntry.CONNECTED_STATE_CONNECTED;
import static com.android.wifitrackerlib.WifiEntry.CONNECTED_STATE_DISCONNECTED;
import static com.android.wifitrackerlib.WifiEntry.MIN_FREQ_24GHZ;
import static com.android.wifitrackerlib.WifiEntry.WIFI_LEVEL_MAX;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.net.wifi.sharedconnectivity.app.HotspotNetwork;
import android.net.wifi.sharedconnectivity.app.HotspotNetworkConnectionStatus;
import android.net.wifi.sharedconnectivity.app.NetworkProviderInfo;
import android.net.wifi.sharedconnectivity.app.SharedConnectivityManager;
import android.os.Handler;
import android.os.test.TestLooper;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

public class HotspotNetworkEntryTest {
    @Mock private WifiEntry.WifiEntryCallback mMockListener;
    @Mock private WifiEntry.ConnectCallback mMockConnectCallback;
    @Mock private WifiEntry.DisconnectCallback mMockDisconnectCallback;
    @Mock private WifiTrackerInjector mMockInjector;
    @Mock private Context mMockContext;
    @Mock private Resources mMockResources;
    @Mock private WifiManager mMockWifiManager;
    @Mock private SharedConnectivityManager mMockSharedConnectivityManager;
    @Mock private WifiInfo mMockWifiInfo;
    @Mock private Network mMockNetwork;
    @Mock private NetworkCapabilities mMockNetworkCapabilities;

    private TestLooper mTestLooper;
    private Handler mTestHandler;

    private static final HotspotNetwork TEST_HOTSPOT_NETWORK_DATA = new HotspotNetwork.Builder()
            .setDeviceId(1)
            .setNetworkProviderInfo(new NetworkProviderInfo
                    .Builder("My Phone", "Pixel 7")
                    .setDeviceType(NetworkProviderInfo.DEVICE_TYPE_PHONE)
                    .setBatteryPercentage(100)
                    .setConnectionStrength(3)
                    .build())
            .setHostNetworkType(HotspotNetwork.NETWORK_TYPE_CELLULAR)
            .setNetworkName("Google Fi")
            .setHotspotSsid("Instant Hotspot abcde")
            .addHotspotSecurityType(SECURITY_TYPE_PSK)
            .build();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTestLooper = new TestLooper();
        mTestHandler = new Handler(mTestLooper.getLooper());

        when(mMockNetworkCapabilities.getTransportInfo()).thenReturn(mMockWifiInfo);
        when(mMockWifiInfo.isPrimary()).thenReturn(true);
        when(mMockWifiInfo.getSSID()).thenReturn("Instant Hotspot abcde");
        when(mMockWifiInfo.getCurrentSecurityType()).thenReturn(SECURITY_TYPE_PSK);
        when(mMockWifiInfo.getRssi()).thenReturn(TestUtils.GOOD_RSSI);
        when(mMockWifiInfo.getMacAddress()).thenReturn("01:02:03:04:05:06");
        when(mMockWifiInfo.getWifiStandard()).thenReturn(WIFI_STANDARD_11N);
        when(mMockWifiInfo.getFrequency()).thenReturn(MIN_FREQ_24GHZ);

        when(mMockContext.getString(R.string.wifitrackerlib_hotspot_network_connecting))
                .thenReturn("Connecting…");
        when(mMockContext.getString(eq(R.string.wifitrackerlib_hotspot_network_summary),
                anyString(), anyString()))
                .thenAnswer(invocation -> {
                    Object[] args = invocation.getArguments();
                    return args[1] + " from " + args[2];
                });
        when(mMockContext.getString(eq(R.string.wifitrackerlib_hotspot_network_alternate),
                anyString(), anyString()))
                .thenAnswer(invocation -> {
                    Object[] args = invocation.getArguments();
                    return args[1] + " from " + args[2];
                });
        when(mMockContext.getString(R.string.wifitrackerlib_wifi_security_wpa_wpa2))
                .thenReturn("WPA/WPA2-Personal");
        when(mMockContext.getString(R.string.wifitrackerlib_wifi_standard_11n))
                .thenReturn("Wi‑Fi 4");
        when(mMockContext.getResources()).thenReturn(mMockResources);
        when(mMockResources.getString(R.string.wifitrackerlib_wifi_band_24_ghz)).thenReturn(
                "2.4 GHz");
        when(mMockResources.getString(R.string.wifitrackerlib_multiband_separator)).thenReturn(
                ", ");
    }

    @Test
    public void testConnectionInfoMatches_matchesSsidAndSecurity() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        when(mMockWifiInfo.getSSID()).thenReturn("Instant Hotspot fghij");
        when(mMockWifiInfo.getCurrentSecurityType()).thenReturn(SECURITY_TYPE_SAE);

        assertThat(entry.connectionInfoMatches(mMockWifiInfo)).isFalse();

        when(mMockWifiInfo.getSSID()).thenReturn("Instant Hotspot abcde");
        when(mMockWifiInfo.getCurrentSecurityType()).thenReturn(SECURITY_TYPE_PSK);

        assertThat(entry.connectionInfoMatches(mMockWifiInfo)).isTrue();
    }

    @Test
    public void testOnNetworkCapabilitiesChanged_matchingSsidAndSecurity_becomesConnected() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        // Ignore non-matching SSID and security type
        when(mMockWifiInfo.getSSID()).thenReturn("Instant Hotspot fghij");
        when(mMockWifiInfo.getCurrentSecurityType()).thenReturn(SECURITY_TYPE_SAE);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);
        assertThat(entry.getConnectedState()).isEqualTo(CONNECTED_STATE_DISCONNECTED);
        assertThat(entry.canConnect()).isTrue();
        assertThat(entry.canDisconnect()).isFalse();

        // Matching SSID and security type should result in connected
        when(mMockWifiInfo.getSSID()).thenReturn("Instant Hotspot abcde");
        when(mMockWifiInfo.getCurrentSecurityType()).thenReturn(SECURITY_TYPE_PSK);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);
        assertThat(entry.getConnectedState()).isEqualTo(CONNECTED_STATE_CONNECTED);
        assertThat(entry.canConnect()).isFalse();
        assertThat(entry.canDisconnect()).isTrue();
    }

    @Test
    public void testOnNetworkLost_matchingNetwork_becomesDisconnected() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);

        // Non-matching network loss should be ignored
        entry.onNetworkLost(mock(Network.class));
        assertThat(entry.getConnectedState()).isEqualTo(CONNECTED_STATE_CONNECTED);
        assertThat(entry.canConnect()).isFalse();
        assertThat(entry.canDisconnect()).isTrue();

        // Matching network loss should result in disconnected
        entry.onNetworkLost(mMockNetwork);
        assertThat(entry.getConnectedState()).isEqualTo(CONNECTED_STATE_DISCONNECTED);
        assertThat(entry.canConnect()).isTrue();
        assertThat(entry.canDisconnect()).isFalse();
    }

    @Test
    public void testGetTitle_usesDeviceName() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getTitle()).isEqualTo("My Phone");
    }

    @Test
    public void testGetSummary_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getSummary()).isEqualTo("Google Fi from Pixel 7");
    }

    @Test
    public void testGetAlternateSummary_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getAlternateSummary()).isEqualTo("Google Fi from My Phone");
    }

    @Test
    public void testGetSummary_connectionStatusEnabling_returnsConnectingString() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        entry.setListener(mMockListener);
        entry.connect(mMockConnectCallback);
        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_ENABLING_HOTSPOT);
        mTestLooper.dispatchAll();

        verify(mMockConnectCallback, never()).onConnectResult(anyInt());

        assertThat(entry.getSummary()).isEqualTo("Connecting…");
    }

    @Test
    public void testGetSummary_connectionStatusFailure_resetsConnectingString() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        entry.setListener(mMockListener);
        entry.connect(mMockConnectCallback);
        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_ENABLING_HOTSPOT);
        mTestLooper.dispatchAll();
        assertThat(entry.getSummary()).isEqualTo("Connecting…");

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_UNKNOWN_ERROR);
        mTestLooper.dispatchAll();

        assertThat(entry.getSummary()).isNotEqualTo("Connecting…");
    }

    @Test
    public void testGetSummary_connectionSuccess_resetsConnectingString() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        entry.setListener(mMockListener);
        entry.connect(mMockConnectCallback);
        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_ENABLING_HOTSPOT);
        mTestLooper.dispatchAll();

        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);
        entry.onNetworkLost(mMockNetwork);

        assertThat(entry.getSummary()).isNotEqualTo("Connecting…");
    }

    @Test
    public void testGetSsid_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getSsid()).isEqualTo("Instant Hotspot abcde");
    }

    @Test
    public void testGetMacAddress_usesWifiInfo() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);

        assertThat(entry.getMacAddress()).isEqualTo("01:02:03:04:05:06");
    }

    @Test
    public void testGetPrivacy_returnsRandomized() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getPrivacy()).isEqualTo(HotspotNetworkEntry.PRIVACY_RANDOMIZED_MAC);
    }

    @Test
    public void testGetSecurityString_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getSecurityString(false)).isEqualTo("WPA/WPA2-Personal");
    }

    @Test
    public void testGetStandardString_usesWifiInfo() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);

        assertThat(entry.getStandardString()).isEqualTo("Wi‑Fi 4");
    }

    @Test
    public void testGetBandString_usesWifiInfo() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);

        assertThat(entry.getBandString()).isEqualTo("2.4 GHz");
    }

    @Test
    public void testGetUpstreamConnectionStrength_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getUpstreamConnectionStrength()).isEqualTo(3);
    }

    @Test
    public void testGetNetworkType_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getNetworkType()).isEqualTo(HotspotNetwork.NETWORK_TYPE_CELLULAR);
    }

    @Test
    public void testGetDeviceType_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getDeviceType()).isEqualTo(NetworkProviderInfo.DEVICE_TYPE_PHONE);
    }

    @Test
    public void testGetBatteryPercentage_usesHotspotNetworkData() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        assertThat(entry.getBatteryPercentage()).isEqualTo(100);
    }

    @Test
    public void testGetLevel_statusNotConnected_returnsMaxValue() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        when(mMockWifiInfo.getSSID()).thenReturn("Instant Hotspot fghij");
        when(mMockWifiInfo.getCurrentSecurityType()).thenReturn(SECURITY_TYPE_SAE);
        entry.onNetworkCapabilitiesChanged(mMockNetwork, mMockNetworkCapabilities);

        assertThat(entry.getLevel()).isEqualTo(WIFI_LEVEL_MAX);
    }

    @Test
    public void testConnect_serviceCalled() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        entry.connect(null);
        verify(mMockSharedConnectivityManager).connectHotspotNetwork(TEST_HOTSPOT_NETWORK_DATA);
    }

    @Test
    public void testConnect_nullManager_failureCallback() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, /* sharedConnectivityManager= */ null, TEST_HOTSPOT_NETWORK_DATA);

        entry.setListener(mMockListener);
        entry.connect(mMockConnectCallback);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback)
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);
    }

    @Test
    public void testConnect_onConnectionStatusChanged_failureCallback() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        entry.setListener(mMockListener);
        entry.connect(mMockConnectCallback);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, never()).onConnectResult(anyInt());

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_UNKNOWN_ERROR);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(1))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_PROVISIONING_FAILED);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(2))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_TETHERING_TIMEOUT);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(3))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_TETHERING_UNSUPPORTED);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(4))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_NO_CELL_DATA);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(5))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_ENABLING_HOTSPOT_FAILED);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(6))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_ENABLING_HOTSPOT_TIMEOUT);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(7))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);

        entry.onConnectionStatusChanged(
                HotspotNetworkConnectionStatus.CONNECTION_STATUS_CONNECT_TO_HOTSPOT_FAILED);
        mTestLooper.dispatchAll();
        verify(mMockConnectCallback, times(8))
                .onConnectResult(WifiEntry.ConnectCallback.CONNECT_STATUS_FAILURE_UNKNOWN);
    }

    @Test
    public void testDisconnect_serviceCalled() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, mMockSharedConnectivityManager, TEST_HOTSPOT_NETWORK_DATA);

        entry.disconnect(null);
        verify(mMockSharedConnectivityManager).disconnectHotspotNetwork(TEST_HOTSPOT_NETWORK_DATA);
    }

    @Test
    public void testDisconnect_nullManager_failureCallback() {
        final HotspotNetworkEntry entry = new HotspotNetworkEntry(
                mMockInjector, mMockContext, mTestHandler,
                mMockWifiManager, /* sharedConnectivityManager= */ null, TEST_HOTSPOT_NETWORK_DATA);

        entry.setListener(mMockListener);
        entry.disconnect(mMockDisconnectCallback);
        mTestLooper.dispatchAll();
        verify(mMockDisconnectCallback)
                .onDisconnectResult(WifiEntry.DisconnectCallback.DISCONNECT_STATUS_FAILURE_UNKNOWN);
    }
}
