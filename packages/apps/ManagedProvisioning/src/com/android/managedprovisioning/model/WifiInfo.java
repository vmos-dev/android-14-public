/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.model;

import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_ANONYMOUS_IDENTITY;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_CA_CERTIFICATE;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_DOMAIN;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_EAP_METHOD;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_HIDDEN;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_IDENTITY;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_PAC_URL;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_PASSWORD;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_PHASE2_AUTH;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_PROXY_BYPASS;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_PROXY_HOST;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_PROXY_PORT;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_SECURITY_TYPE;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_SSID;
import static android.app.admin.DevicePolicyManager.EXTRA_PROVISIONING_WIFI_USER_CERTIFICATE;

import android.os.Parcel;
import android.os.Parcelable;
import android.os.PersistableBundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import com.android.internal.annotations.Immutable;
import com.android.managedprovisioning.common.PersistableBundlable;

/**
 * Stores the WiFi configuration which is used in managed provisioning.
 */
@Immutable
public final class WifiInfo extends PersistableBundlable {
    public static final boolean DEFAULT_WIFI_HIDDEN = false;
    public static final int DEFAULT_WIFI_PROXY_PORT = 0;

    public static final Parcelable.Creator<WifiInfo> CREATOR
            = new Parcelable.Creator<WifiInfo>() {
        @Override
        public WifiInfo createFromParcel(Parcel in) {
            return new WifiInfo(in);
        }

        @Override
        public WifiInfo[] newArray(int size) {
            return new WifiInfo[size];
        }
    };

    /** Ssid of the wifi network. */
    public final String ssid;
    /** Wifi network in {@link #ssid} is hidden or not. */
    public final boolean hidden;
    /** Security type of the wifi network in {@link #ssid}. */
    @Nullable
    public final String securityType;
    /** Password of the wifi network in {@link #ssid}. */
    @Nullable
    public final String password;

    /**
     * EAP method of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}.
     */
    @Nullable
    public final String eapMethod;

    /**
     * Phase 2 authentification of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}.
     */
    @Nullable
    public final String phase2Auth;

    /**
     * CA certificate of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}. Format of certificate should be as {@link
     * android.app.admin.DevicePolicyManager#EXTRA_PROVISIONING_WIFI_CA_CERTIFICATE}
     */
    @Nullable
    public final String caCertificate;

    /**
     * User certificate of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}. Format of certificate should be as {@link
     * android.app.admin.DevicePolicyManager#EXTRA_PROVISIONING_WIFI_USER_CERTIFICATE}
     */
    @Nullable
    public final String userCertificate;

    /**
     * Identity of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}.
     */
    @Nullable
    public final String identity;

    /**
     * Anonymous identity of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}.
     */
    @Nullable
    public final String anonymousIdentity;

    /**
     * Domain of the wifi network in {@link #ssid}. This is only used if the
     * {@link #securityType} is {@code EAP}.
     */
    @Nullable
    public final String domain;

    /** Proxy host for the wifi network in {@link #ssid}. */
    @Nullable
    public final String proxyHost;
    /** Proxy port for the wifi network in {@link #ssid}. */
    public final int proxyPort;
    /** The proxy bypass for the wifi network in {@link #ssid}. */
    @Nullable
    public final String proxyBypassHosts;
    /** The proxy bypass list for the wifi network in {@link #ssid}. */
    @Nullable
    public final String pacUrl;

    @Override
    public PersistableBundle toPersistableBundle() {
        final PersistableBundle bundle = new PersistableBundle();
        bundle.putString(EXTRA_PROVISIONING_WIFI_SSID, ssid);
        bundle.putBoolean(EXTRA_PROVISIONING_WIFI_HIDDEN, hidden);
        bundle.putString(EXTRA_PROVISIONING_WIFI_SECURITY_TYPE, securityType);
        bundle.putString(EXTRA_PROVISIONING_WIFI_PASSWORD, password);
        bundle.putString(EXTRA_PROVISIONING_WIFI_EAP_METHOD, eapMethod);
        bundle.putString(EXTRA_PROVISIONING_WIFI_PHASE2_AUTH, phase2Auth);
        bundle.putString(EXTRA_PROVISIONING_WIFI_CA_CERTIFICATE, caCertificate);
        bundle.putString(EXTRA_PROVISIONING_WIFI_USER_CERTIFICATE, userCertificate);
        bundle.putString(EXTRA_PROVISIONING_WIFI_IDENTITY, identity);
        bundle.putString(EXTRA_PROVISIONING_WIFI_ANONYMOUS_IDENTITY, anonymousIdentity);
        bundle.putString(EXTRA_PROVISIONING_WIFI_DOMAIN, domain);
        bundle.putString(EXTRA_PROVISIONING_WIFI_PROXY_HOST, proxyHost);
        bundle.putInt(EXTRA_PROVISIONING_WIFI_PROXY_PORT, proxyPort);
        bundle.putString(EXTRA_PROVISIONING_WIFI_PROXY_BYPASS, proxyBypassHosts);
        bundle.putString(EXTRA_PROVISIONING_WIFI_PAC_URL, pacUrl);
        return bundle;
    }

    /* package */ static WifiInfo fromPersistableBundle(PersistableBundle bundle) {
        return createBuilderFromPersistableBundle(bundle).build();
    }

    private static Builder createBuilderFromPersistableBundle(PersistableBundle bundle) {
        Builder builder = new Builder();
        builder.setSsid(bundle.getString(EXTRA_PROVISIONING_WIFI_SSID));
        builder.setHidden(bundle.getBoolean(EXTRA_PROVISIONING_WIFI_HIDDEN));
        builder.setSecurityType(bundle.getString(EXTRA_PROVISIONING_WIFI_SECURITY_TYPE));
        builder.setPassword(bundle.getString(EXTRA_PROVISIONING_WIFI_PASSWORD));
        builder.setEapMethod(bundle.getString(EXTRA_PROVISIONING_WIFI_EAP_METHOD));
        builder.setPhase2Auth(bundle.getString(EXTRA_PROVISIONING_WIFI_PHASE2_AUTH));
        builder.setCaCertificate(bundle.getString(EXTRA_PROVISIONING_WIFI_CA_CERTIFICATE));
        builder.setUserCertificate(bundle.getString(EXTRA_PROVISIONING_WIFI_USER_CERTIFICATE));
        builder.setIdentity(bundle.getString(EXTRA_PROVISIONING_WIFI_IDENTITY));
        builder.setAnonymousIdentity(bundle.getString(EXTRA_PROVISIONING_WIFI_ANONYMOUS_IDENTITY));
        builder.setDomain(bundle.getString(EXTRA_PROVISIONING_WIFI_DOMAIN));
        builder.setProxyHost(bundle.getString(EXTRA_PROVISIONING_WIFI_PROXY_HOST));
        builder.setProxyPort(bundle.getInt(EXTRA_PROVISIONING_WIFI_PROXY_PORT));
        builder.setProxyBypassHosts(bundle.getString(EXTRA_PROVISIONING_WIFI_PROXY_BYPASS));
        builder.setPacUrl(bundle.getString(EXTRA_PROVISIONING_WIFI_PAC_URL));
        return builder;
    }

    private WifiInfo(Builder builder) {
        ssid = builder.mSsid;
        hidden = builder.mHidden;
        securityType = builder.mSecurityType;
        password = builder.mPassword;
        eapMethod = builder.eapMethod;
        phase2Auth = builder.phase2Auth;
        caCertificate = builder.caCertificate;
        userCertificate = builder.userCertificate;
        identity = builder.identity;
        anonymousIdentity = builder.anonymousIdentity;
        domain = builder.domain;
        proxyHost = builder.mProxyHost;
        proxyPort = builder.mProxyPort;
        proxyBypassHosts = builder.mProxyBypassHosts;
        pacUrl = builder.mPacUrl;

        validateFields();
    }

    private WifiInfo(Parcel in) {
        this(createBuilderFromPersistableBundle(
                PersistableBundlable.getPersistableBundleFromParcel(in)));
    }

    private void validateFields() {
        if (TextUtils.isEmpty(ssid)) {
            throw new IllegalArgumentException("Ssid must not be empty!");
        }
    }

    public final static class Builder {
        private String mSsid;
        private boolean mHidden = DEFAULT_WIFI_HIDDEN;
        private String mSecurityType;
        private String mPassword;
        private String eapMethod;
        private String phase2Auth;
        private String caCertificate;
        private String userCertificate;
        private String identity;
        private String anonymousIdentity;
        private String domain;
        private String mProxyHost;
        private int mProxyPort = DEFAULT_WIFI_PROXY_PORT;
        private String mProxyBypassHosts;
        private String mPacUrl;

        /**
         * Set the SSID of the network.
         *
         * Note: This must be in the same format as {@link android.net.wifi.WifiConfiguration#SSID},
         *       and must be wrapped in double quotes or else it will be interpreted as hexadecimal.
         */
        public Builder setSsid(String ssid) {
            mSsid = ssid;
            return this;
        }

        public Builder setHidden(boolean hidden) {
            mHidden = hidden;
            return this;
        }

        public Builder setSecurityType(String securityType) {
            mSecurityType = securityType;
            return this;
        }

        public Builder setPassword(String password) {
            mPassword = password;
            return this;
        }

        public Builder setEapMethod(String eapMethod) {
            this.eapMethod = eapMethod;
            return this;
        }

        public Builder setPhase2Auth(String phase2Auth) {
            this.phase2Auth = phase2Auth;
            return this;
        }

        public Builder setCaCertificate(String caCertificate) {
            this.caCertificate = caCertificate;
            return this;
        }

        public Builder setUserCertificate(String userCertificate) {
            this.userCertificate = userCertificate;
            return this;
        }

        public Builder setIdentity(String identity) {
            this.identity = identity;
            return this;
        }

        public Builder setAnonymousIdentity(String anonymousIdentity) {
            this.anonymousIdentity = anonymousIdentity;
            return this;
        }

        public Builder setDomain(String domain) {
            this.domain = domain;
            return this;
        }

        public Builder setProxyHost(String proxyHost) {
            mProxyHost = proxyHost;
            return this;
        }

        public Builder setProxyPort(int proxyPort) {
            mProxyPort = proxyPort;
            return this;
        }

        public Builder setProxyBypassHosts(String proxyBypassHosts) {
            mProxyBypassHosts = proxyBypassHosts;
            return this;
        }

        public Builder setPacUrl(String pacUrl) {
            mPacUrl = pacUrl;
            return this;
        }

        public WifiInfo build() {
            return new WifiInfo(this);
        }

        public static Builder builder() {
            return new Builder();
        }
    }
}
