/*
 * Copyright (C) 2009 The Android Open Source Project
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
package com.android.settings.ethernet;

import com.android.settings.R;
import com.android.settings.SettingsPreferenceFragment;

import android.app.Activity;
import android.app.Dialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.res.Resources;
import android.net.ConnectivityManager;
import android.net.InetAddresses;
import android.net.NetworkInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemProperties;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.Log;

import androidx.preference.SwitchPreference;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.preference.ListPreference;

import java.util.HashMap;
import java.util.regex.Pattern;
import java.lang.Integer;
import java.net.InetAddress;
import java.net.Inet4Address;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.List;

import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.Preference.OnPreferenceClickListener;

/*for 5.0*/
import android.net.EthernetManager;
import android.net.IpConfiguration;
import android.net.IpConfiguration.IpAssignment;
import android.net.IpConfiguration.ProxySettings;
import android.net.StaticIpConfiguration;
import android.net.LinkAddress;
import android.widget.Toast;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.settings.ethernet.EthernetStaticIpDialog;
import com.android.settings.utils.ReflectUtils;

public class EthernetSettings extends SettingsPreferenceFragment implements
        Preference.OnPreferenceChangeListener, EthernetStaticIpDialog.OnStaticIpDialogClickListener {
    private static final String TAG = "EthernetSettings";

    public enum ETHERNET_STATE {
        ETHER_STATE_DISCONNECTED,
        ETHER_STATE_CONNECTING,
        ETHER_STATE_CONNECTED
    }

    private static final String KEY_NET_INTERFACE = "ethernet_net_interface";
    private static final String KEY_ETH_IP_ADDRESS = "ethernet_ip_addr";
    private static final String KEY_ETH_HW_ADDRESS = "ethernet_hw_addr";
    private static final String KEY_ETH_NET_MASK = "ethernet_netmask";
    private static final String KEY_ETH_GATEWAY = "ethernet_gateway";
    private static final String KEY_ETH_DNS1 = "ethernet_dns1";
    private static final String KEY_ETH_DNS2 = "ethernet_dns2";
    private static final String KEY_ETH_MODE = "ethernet_mode_select";

    private final static String nullIpInfo = "0.0.0.0";

    private final static String ASSIGN_ETH = "eth0";//eth0 eth1
    private final static String PREFIX_SPLIT = "_";

    private final IntentFilter mIntentFilter;
    EthernetManager mEthManager;
    Context mContext;
    private EthernetStaticIpDialog mDialog;
    private long mChangeTime;

    private static final int MSG_GET_ETHERNET_STATE = 0;
    private HashMap<String, EthInfo> mEthInfoList = new HashMap<String, EthInfo>();
    private String[] mEthInfoKeyList = null;

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            if (MSG_GET_ETHERNET_STATE == msg.what) {
                handleEtherStateChange(ETHERNET_STATE.values()[msg.arg1],
                        String.valueOf(msg.obj));
            }
        }
    };

    @Override
    public int getMetricsCategory() {
        return MetricsEvent.WIFI_TETHER_SETTINGS;
    }

    @Override
    public int getDialogMetricsCategory(int dialogId) {
        if (dialogId > mEthInfoKeyList.length - 1) {
            return 0;
        }
        return MetricsEvent.WIFI_TETHER_SETTINGS;
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            log("Action " + action);
            if (ConnectivityManager.CONNECTIVITY_ACTION.equals(action)) {
                NetworkInfo info = intent.getParcelableExtra(ConnectivityManager.EXTRA_NETWORK_INFO);
                Log.v(TAG, "===" + info.toString());
                if (null != info && ConnectivityManager.TYPE_ETHERNET == info.getType()) {
                    long currentTime = System.currentTimeMillis();
                    int delayTime = 0;
                    if (currentTime - mChangeTime < 1000) {
                        delayTime = 2000;
                    }
                    String currentIfaceName = null;
                    if (mEthInfoList.size() > 1) {
                        handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTED, currentIfaceName, delayTime);
                    } else {
                        if (NetworkInfo.State.CONNECTED == info.getState()) {
                            handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTED, currentIfaceName, delayTime);
                        } else if (NetworkInfo.State.DISCONNECTED == info.getState()) {
                            handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_DISCONNECTED, currentIfaceName, delayTime);
                        }
                    }
                }
            }
        }
    };

    public EthernetSettings() {
        mIntentFilter = new IntentFilter();
        mIntentFilter.addAction(ConnectivityManager.CONNECTIVITY_ACTION);
    }

    private void handleEtherStateChange(ETHERNET_STATE etherState, String ifaceName, long delayMillis) {
        mHandler.removeMessages(MSG_GET_ETHERNET_STATE);
        if (delayMillis > 0) {
            Message msg = new Message();
            msg.what = MSG_GET_ETHERNET_STATE;
            msg.arg1 = etherState.ordinal();
            msg.obj = ifaceName;
            mHandler.sendMessageDelayed(msg, delayMillis);
        } else {
            handleEtherStateChange(etherState, ifaceName);
        }
    }

    private void handleEtherStateChange(ETHERNET_STATE EtherState, String ifaceName) {
        log("curEtherState" + EtherState);
        EthInfo ethInfo = new EthInfo();
        switch (EtherState) {
            case ETHER_STATE_DISCONNECTED://拔出就全部置0
                ethInfo.setIpAddress(nullIpInfo);
                ethInfo.setNetmask(nullIpInfo);
                ethInfo.setGateway(nullIpInfo);
                ethInfo.setDns1(nullIpInfo);
                ethInfo.setDns2(nullIpInfo);
                Iterator<String> iterator = mEthInfoList.keySet().iterator();
                if (null == ifaceName) {
                    while (iterator.hasNext()) {
                        String ifaceNameKey = iterator.next();
                        mEthInfoList.put(ifaceNameKey, ethInfo);
                    }
                } else {
                    mEthInfoList.put(ifaceName, ethInfo);
                }
                break;
            case ETHER_STATE_CONNECTING:
                String mStatusString = this.getResources().getString(R.string.ethernet_info_getting);
                ethInfo.setIpAddress(mStatusString);
                ethInfo.setNetmask(mStatusString);
                ethInfo.setGateway(mStatusString);
                ethInfo.setDns1(mStatusString);
                ethInfo.setDns2(mStatusString);
                if (null != ifaceName) {
                    mEthInfoList.put(ifaceName, ethInfo);
                }
                break;
            case ETHER_STATE_CONNECTED:
                getEthInfo();
                break;
        }

        refreshUI();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.ethernet_settings);
        mContext = this.getActivity().getApplicationContext();
        mEthManager = (EthernetManager) getSystemService(Context.ETHERNET_SERVICE);

        if (mEthManager == null) {
            Log.e(TAG, "get ethernet manager failed");
            Toast.makeText(mContext, R.string.disabled_ethernet, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        String[] ifaces = null;
        Object result = ReflectUtils.invokeMethodNoParameter(mEthManager, "getAvailableInterfaces");
        if (result != null) {
            ifaces = (String[]) result;
        } else {
            Log.e(TAG, "ReflectUtils EthManager getNetmask result == null ! ");
            return;
        }
        for (String iface : ifaces) {
            if (TextUtils.isEmpty(ASSIGN_ETH)) {
                mEthInfoList.put(iface, null);
            } else if (ASSIGN_ETH.equals(iface)) {
                mEthInfoList.put(iface, null);
                break;
            }
        }

        if (mEthInfoList.size() == 0) {
            Log.e(TAG, "get ethernet ifaceName failed");
            Toast.makeText(mContext, R.string.disabled_ethernet, Toast.LENGTH_SHORT).show();
            finish();
        } else {
            mEthInfoKeyList = new String[mEthInfoList.size()];
            Iterator<String> iterator = mEthInfoList.keySet().iterator();
            int index = 0;
            while (iterator.hasNext()) {
                mEthInfoKeyList[index++] = iterator.next();
            }
            initView();
        }
    }

    private void initView() {
        PreferenceScreen preferenceScreen = getPreferenceScreen();
        Iterator<String> iterator = mEthInfoList.keySet().iterator();
        while (iterator.hasNext()) {
            String key = iterator.next();
            String prefix = key + PREFIX_SPLIT;
            log("prefix = " + prefix);
            // Create Network interface Preference
            PreferenceCategory ethernetNetInterface = new PreferenceCategory(mContext);
            ethernetNetInterface.setKey(prefix + KEY_NET_INTERFACE);
            //ethernetNetInterface.setSummary(R.string.device_info_default);
            //ethernetNetInterface.setTitle(R.string.ethernet_net_interface);
            ethernetNetInterface.setTitle(key);

            // Create Ethernet IP Address Preference
            Preference ethernetIpAddrPref = new Preference(mContext);
            ethernetIpAddrPref.setKey(prefix + KEY_ETH_IP_ADDRESS);
            ethernetIpAddrPref.setSummary(R.string.device_info_default);
            ethernetIpAddrPref.setTitle(R.string.ethernet_ip_addr);

            // Create Ethernet Netmask Preference
            Preference ethernetNetmaskPref = new Preference(mContext);
            ethernetNetmaskPref.setKey(prefix + KEY_ETH_NET_MASK);
            ethernetNetmaskPref.setSummary(R.string.device_info_default);
            ethernetNetmaskPref.setTitle(R.string.ethernet_netmask);

            // Create Ethernet Gateway Preference
            Preference ethernetGatewayPref = new Preference(mContext);
            ethernetGatewayPref.setKey(prefix + KEY_ETH_GATEWAY);
            ethernetGatewayPref.setSummary(R.string.device_info_default);
            ethernetGatewayPref.setTitle(R.string.ethernet_gateway);

            // Create Ethernet DNS1 Preference
            Preference ethernetDns1Pref = new Preference(mContext);
            ethernetDns1Pref.setKey(prefix + KEY_ETH_DNS1);
            log("hjf  ethernetDns1Pref.setKey = " + prefix + KEY_ETH_DNS1);
            ethernetDns1Pref.setSummary(R.string.device_info_default);
            ethernetDns1Pref.setTitle(R.string.ethernet_dns1);

            // Create Ethernet DNS2 Preference
            Preference ethernetDns2Pref = new Preference(mContext);
            ethernetDns2Pref.setKey(prefix + KEY_ETH_DNS2);
            ethernetDns2Pref.setSummary(R.string.device_info_default);
            ethernetDns2Pref.setTitle(R.string.ethernet_dns2);

            ListPreference ethernetModePref = new ListPreference(mContext);
            ethernetModePref.setKey(prefix + KEY_ETH_MODE);
            ethernetModePref.setEntries(R.array.ethernet_mode_location);
            ethernetModePref.setEntryValues(R.array.ethernet_mode_values);
            ethernetModePref.setPersistent(true);
            ethernetModePref.setTitle(R.string.ethernet_mode_title);
            ethernetModePref.setOnPreferenceChangeListener(this);

            preferenceScreen.addPreference(ethernetNetInterface);
            ethernetNetInterface.addPreference(ethernetIpAddrPref);
            ethernetNetInterface.addPreference(ethernetNetmaskPref);
            ethernetNetInterface.addPreference(ethernetGatewayPref);
            ethernetNetInterface.addPreference(ethernetDns1Pref);
            ethernetNetInterface.addPreference(ethernetDns2Pref);
            ethernetNetInterface.addPreference(ethernetModePref);
        }
    }

    private Inet4Address getIPv4Address(String text) {
        try {
            return (Inet4Address) InetAddresses.parseNumericAddress(text);
        } catch (IllegalArgumentException | ClassCastException e) {
            return null;
        }
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mEthInfoList.size() == 0) {
            return;
        }
        refreshUI();
        log("resume");
        mContext.registerReceiver(mReceiver, mIntentFilter);
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mEthInfoList.size() == 0) {
            return;
        }
        mContext.unregisterReceiver(mReceiver);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mHandler.removeMessages(MSG_GET_ETHERNET_STATE);
        log("destory");
    }

    @Override
    public void onStop() {
        super.onStop();
        log("stop");
    }

    private void setStringSummary(String preference, String value) {
        try {
            findPreference(preference).setSummary(value);
        } catch (RuntimeException e) {
            findPreference(preference).setSummary("");
            log("can't find " + preference);
        }
    }

    private void refreshUI() {
        Iterator<String> iterator = mEthInfoList.keySet().iterator();
        while (iterator.hasNext()) {
            String key = iterator.next();
            String prefix = key + PREFIX_SPLIT;
            EthInfo info = mEthInfoList.get(key);
            log("info = " + info);
            if (info != null) {
                //setStringSummary(prefix + KEY_NET_INTERFACE, key);
                setStringSummary(prefix + KEY_ETH_IP_ADDRESS, info.getIpAddress());
                setStringSummary(prefix + KEY_ETH_NET_MASK, info.getNetmask());
                setStringSummary(prefix + KEY_ETH_GATEWAY, info.getGateway());
                log("hjf  info.getGateway() = " + info.getGateway());
                setStringSummary(prefix + KEY_ETH_DNS1, info.getDns1());
                log("hjf  prefix + KEY_ETH_DNS1 = " + prefix + KEY_ETH_DNS1);
                log("hjf  info.getDns1() = " + info.getDns1());
                setStringSummary(prefix + KEY_ETH_DNS2, info.getDns2());
                updateCheckbox(key, (ListPreference) findPreference(prefix + KEY_ETH_MODE));
            } else {
                setStringSummary(prefix + KEY_ETH_IP_ADDRESS, nullIpInfo);
                setStringSummary(prefix + KEY_ETH_NET_MASK, nullIpInfo);
                setStringSummary(prefix + KEY_ETH_GATEWAY, nullIpInfo);
                setStringSummary(prefix + KEY_ETH_DNS1, nullIpInfo);
                setStringSummary(prefix + KEY_ETH_DNS2, nullIpInfo);
            }
        }
    }

    private void updateCheckbox(String ifaceName, ListPreference preference) {
        if (mEthManager == null) {
            preference.setSummary("null");
        } else {
            IpAssignment mode = null;
            IpConfiguration ipConfiguration = Reflect_getConfiguration(ifaceName);
            log("updateCheckbox ipConfiguration = " + ipConfiguration);
            if ((ipConfiguration != null) && (!ipConfiguration.equals(""))) {
                mode = ipConfiguration.getIpAssignment();
            }
            log("mode = " + mode);

            if (mode == IpAssignment.DHCP || mode == IpAssignment.UNASSIGNED) {
                preference.setValue("DHCP");
                preference.setSummary(R.string.usedhcp);
            } else {
                preference.setValue("StaticIP");
                preference.setSummary(R.string.usestatic);
            }
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        log("onPreferenceChange key=" + key + ", value=" + newValue);
        String ifaceName = null;
        int index = 0;
        for (int i = 0; i < mEthInfoKeyList.length; i++) {
            if (key.startsWith(mEthInfoKeyList[i])) {
                ifaceName = mEthInfoKeyList[i];
                index = i;
                break;
            }
        }
        if (null == ifaceName) {
            return true;
        }
        String value = (String) newValue;
        if (value.equals("DHCP")) {
            mChangeTime = System.currentTimeMillis();
            handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTING, ifaceName);
            IpConfiguration ipConfiguration = new IpConfiguration();
            ipConfiguration.setIpAssignment(IpAssignment.DHCP);
            ipConfiguration.setProxySettings(ProxySettings.NONE);
            Reflect_setConfiguration(ifaceName, ipConfiguration);
            log(ifaceName + " switch to dhcp");
        } else if (value.equals("StaticIP")) {
            log(ifaceName + " static editor");
            showDialog(index);
        }
        return true;
    }

    //将子网掩码转换成ip子网掩码形式，比如输入32输出为255.255.255.255
    public String interMask2String(int prefixLength) {
        String netMask = null;
        int inetMask = prefixLength;

        int part = inetMask / 8;
        int remainder = inetMask % 8;
        int sum = 0;

        for (int i = 8; i > 8 - remainder; i--) {
            sum = sum + (int) Math.pow(2, i - 1);
        }

        if (part == 0) {
            netMask = sum + ".0.0.0";
        } else if (part == 1) {
            netMask = "255." + sum + ".0.0";
        } else if (part == 2) {
            netMask = "255.255." + sum + ".0";
        } else if (part == 3) {
            netMask = "255.255.255." + sum;
        } else if (part == 4) {
            netMask = "255.255.255.255";
        }

        return netMask;
    }

    /*
     * convert subMask string to prefix length
     */
    private int maskStr2InetMask(String maskStr) {
        StringBuffer sb;
        String str;
        int inetmask = 0;
        int count = 0;
        /*
         * check the subMask format
         */
        Pattern pattern = Pattern.compile("(^((\\d|[01]?\\d\\d|2[0-4]\\d|25[0-5])\\.){3}(\\d|[01]?\\d\\d|2[0-4]\\d|25[0-5])$)|^(\\d|[1-2]\\d|3[0-2])$");
        if (pattern.matcher(maskStr).matches() == false) {
            Log.e(TAG, "subMask is error");
            return 0;
        }

        String[] ipSegment = maskStr.split("\\.");
        for (int n = 0; n < ipSegment.length; n++) {
            sb = new StringBuffer(Integer.toBinaryString(Integer.parseInt(ipSegment[n])));
            str = sb.reverse().toString();
            count = 0;
            for (int i = 0; i < str.length(); i++) {
                i = str.indexOf("1", i);
                if (i == -1)
                    break;
                count++;
            }
            inetmask += count;
        }
        return inetmask;
    }

    private IpConfiguration setStaticIpConfiguration(EthInfo info) {

        Inet4Address inetAddr = getIPv4Address(info.getIpAddress());
        int prefixLength = maskStr2InetMask(info.getNetmask());
        InetAddress gatewayAddr = getIPv4Address(info.getGateway());
        InetAddress dnsAddr = getIPv4Address(info.getDns1());

        if (null == inetAddr || inetAddr.getAddress().toString().isEmpty()
                || prefixLength == 0
                || gatewayAddr.toString().isEmpty()
                || dnsAddr.toString().isEmpty()) {
            log("ip,mask or dnsAddr is wrong");
            return null;
        }

        String dnsStr2 = info.getDns2();
        ArrayList<InetAddress> dnsAddrs = new ArrayList<InetAddress>();
        dnsAddrs.add(dnsAddr);
        if (!dnsStr2.isEmpty()) {
            dnsAddrs.add(getIPv4Address(dnsStr2));
        }

        StaticIpConfiguration staticIpConfiguration = new StaticIpConfiguration.Builder()
                .setIpAddress(new LinkAddress(inetAddr, prefixLength))
                .setGateway(gatewayAddr)
                .setDnsServers(dnsAddrs)
                .build();
        log("staticIpConfiguration = " + staticIpConfiguration);
        IpConfiguration ipConfiguration = new IpConfiguration();
        ipConfiguration.setIpAssignment(IpAssignment.STATIC);
        ipConfiguration.setProxySettings(ProxySettings.NONE);
        ipConfiguration.setStaticIpConfiguration(staticIpConfiguration);
        return ipConfiguration;
    }

    public EthInfo getEthInfoFromDhcp(String ifaceName) {
        EthInfo ethInfo = new EthInfo();
        String tempIpInfo = null;
        String methodName;
        Class<?>[] paramTypes = new Class<?>[]{String.class};
        Object[] values = new Object[]{ifaceName};
        Object result;

        methodName = "getIpAddress";
        result = ReflectUtils.invokeMethod(mEthManager, methodName, paramTypes, values);
        if (result != null) {
            tempIpInfo = (String) result;
        } else {
            Log.e(TAG, "ReflectUtils EthManager getNetmask result == null ! ");
        }
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            ethInfo.setIpAddress(tempIpInfo);
        } else {
            ethInfo.setIpAddress(nullIpInfo);
        }
        log("IpAddress = " + tempIpInfo);

        methodName = "getNetmask";
        result = ReflectUtils.invokeMethod(mEthManager, methodName, paramTypes, values);
        if (result != null) {
            tempIpInfo = (String) result;
        } else {
            Log.e(TAG, "ReflectUtils EthManager getNetmask result == null ! ");
        }
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            ethInfo.setNetmask(tempIpInfo);
        } else {
            ethInfo.setNetmask(nullIpInfo);
        }
        log("Netmask = " + tempIpInfo);

        methodName = "getGateway";
        result = ReflectUtils.invokeMethod(mEthManager, methodName, paramTypes, values);
        if (result != null) {
            tempIpInfo = (String) result;
        } else {
            Log.e(TAG, "ReflectUtils EthManager getNetmask result == null ! ");
        }
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            ethInfo.setGateway(tempIpInfo);
        } else {
            ethInfo.setGateway(nullIpInfo);
        }
        log("Gateway = " + tempIpInfo);

        methodName = "getDns";
        result = ReflectUtils.invokeMethod(mEthManager, methodName, paramTypes, values);
        if (result != null) {
            tempIpInfo = (String) result;
        } else {
            Log.e(TAG, "ReflectUtils EthManager getNetmask result == null ! ");
        }
        if ((tempIpInfo != null) && (!tempIpInfo.equals(""))) {
            String data[] = tempIpInfo.split(",");
            ethInfo.setDns1(data[0]);
            if (data.length <= 1) {
                ethInfo.setDns2(nullIpInfo);
            } else {
                ethInfo.setDns2(data[1]);
                log("dns1 = " + data[0]);
                log("dns2 = " + data[1]);
            }
        } else {
            ethInfo.setDns1(nullIpInfo);
        }
        return ethInfo;
    }

    public EthInfo getEthInfoFromStaticIp(String ifaceName) {
        EthInfo ethInfo = new EthInfo();
        StaticIpConfiguration staticIpConfiguration = null;
        IpConfiguration ipConfiguration = Reflect_getConfiguration(ifaceName);
        if (ipConfiguration != null) {
            staticIpConfiguration = ipConfiguration.getStaticIpConfiguration();
        }

        if (staticIpConfiguration == null) {
            return null;
        }
        LinkAddress ipAddress = staticIpConfiguration.getIpAddress();
        InetAddress gateway = staticIpConfiguration.getGateway();
        List<InetAddress> dnsServers = staticIpConfiguration.getDnsServers();

        if (ipAddress != null) {
            ethInfo.setIpAddress(ipAddress.getAddress().getHostAddress());
            ethInfo.setNetmask(interMask2String(ipAddress.getPrefixLength()));
        }
        if (gateway != null) {
            ethInfo.setGateway(gateway.getHostAddress());
        }
        ethInfo.setDns1(dnsServers.get(0).getHostAddress());

        if (dnsServers.size() > 1) { /* 只保留两个*/
            ethInfo.setDns2(dnsServers.get(1).getHostAddress());
        }
        return ethInfo;
    }

    public void getEthInfo() {
        Iterator<String> iterator = mEthInfoList.keySet().iterator();
        while (iterator.hasNext()) {
            String ifaceName = iterator.next();
            EthInfo ethInfo = new EthInfo();
            IpAssignment mode = null;
            IpConfiguration ipConfiguration = Reflect_getConfiguration(ifaceName);
            if ((ipConfiguration != null) && (!ipConfiguration.equals(""))) {
                mode = ipConfiguration.getIpAssignment();
            }
            log("ifaceName = " + ifaceName + " ; mode = " + mode);
            if (mode == IpAssignment.DHCP || mode == IpAssignment.UNASSIGNED) {
                ethInfo = getEthInfoFromDhcp(ifaceName);
            } else if (mode == IpAssignment.STATIC) {
                ethInfo = getEthInfoFromStaticIp(ifaceName);
            }
            log("ifaceName = " + ifaceName + " , ethInfo = " + ethInfo);
            mEthInfoList.put(ifaceName, ethInfo);
        }
    }

    private void log(String s) {
        Log.d(TAG, s);
    }

    @Override
    public void onStaticIpDialogClick(String ifaceName) {
        String prefix = ifaceName + PREFIX_SPLIT;
        EthInfo ethInfo = mDialog.saveIpSettingInfo(); //从Dialog获取静态数据
        mEthInfoList.put(ifaceName, ethInfo);
        IpConfiguration ipConfiguration = new IpConfiguration();
        ipConfiguration = setStaticIpConfiguration(ethInfo);
        if (ipConfiguration != null) {
            mChangeTime = System.currentTimeMillis();
            handleEtherStateChange(ETHERNET_STATE.ETHER_STATE_CONNECTING, ifaceName);
            log("hjf ifaceName = " + ifaceName + " : " + ipConfiguration);
            Reflect_setConfiguration(ifaceName, ipConfiguration);
        } else {
            Log.e(TAG, "ipConfiguration == null");
        }
        updateCheckbox(ifaceName, (ListPreference) findPreference(prefix + KEY_ETH_MODE));
    }

    @Override
    public Dialog onCreateDialog(int dialogId) {
        log("onCreateDialog " + dialogId);
        if (dialogId > mEthInfoKeyList.length - 1) {
            return super.onCreateDialog(dialogId);
        }
        mDialog = new EthernetStaticIpDialog(getActivity(), true, this, mEthInfoKeyList[dialogId]);
        return mDialog;
    }

    private void Reflect_setConfiguration(String ifaceName, IpConfiguration ipConfiguration) {
        String methodName = "setConfiguration";
        Class<?>[] paramTypes = new Class<?>[]{String.class, IpConfiguration.class};
        Object[] values = new Object[]{ifaceName, ipConfiguration};
        try {
            // 使用反射调用invokeMethod函数来调用setConfiguration方法
            ReflectUtils.invokeMethod(mEthManager, methodName, paramTypes, values);
            // 反射调用成功，现在setConfiguration方法已被调用
        } catch (Exception e) {
            // 反射调用失败，处理错误情况
            e.printStackTrace();
            Log.e(TAG, "setConfiguration error!");
        }
    }

    private IpConfiguration Reflect_getConfiguration(String ifaceName) {
        IpConfiguration tempipconfig = null;
        String methodName = "getConfiguration";
        Class<?>[] paramTypes = new Class<?>[]{String.class};
        Object[] values = new Object[]{ifaceName};
        Object result = ReflectUtils.invokeMethod(mEthManager, methodName, paramTypes, values);
        if (result != null) {
            tempipconfig = (IpConfiguration) result;
        } else {
            Log.e(TAG, "ReflectUtils EthManager getConfiguration result == null ! ");
        }
        return tempipconfig;
    }

    public static boolean isAvailable() {
        return "true".equals(SystemProperties.get("ro.rk.ethernet_settings"));
    }
}