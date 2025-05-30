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

package com.android.tv.settings.system;

import static com.android.tv.settings.util.InstrumentationUtils.logEntrySelected;
import static com.android.tv.settings.util.InstrumentationUtils.logToggleInteracted;

import android.app.Activity;
import android.app.tvsettings.TvSettingsEnums;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.SystemProperties;
import android.os.UserManager;
import android.provider.Settings;
import android.text.TextUtils;
import android.text.format.DateFormat;

import androidx.annotation.Keep;
import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.SwitchPreference;

import com.android.settingslib.datetime.ZoneGetter;
import com.android.tv.settings.R;
import com.android.tv.settings.RestrictedPreferenceAdapter;
import com.android.tv.settings.SettingsPreferenceFragment;

import java.util.Calendar;
import java.util.Date;
import android.app.time.TimeManager;
import android.app.time.TimeZoneCapabilities;
import android.app.time.TimeZoneCapabilitiesAndConfig;
import static android.app.time.Capabilities.CAPABILITY_POSSESSED;

/**
 * The date and time screen in TV settings.
 */
@Keep
public class DateTimeFragment extends SettingsPreferenceFragment implements
        Preference.OnPreferenceChangeListener {

    private static final String KEY_AUTO_DATE_TIME = "auto_date_time";
    private static final String KEY_SET_DATE = "set_date";
    private static final String KEY_SET_TIME = "set_time";
    private static final String KEY_SET_TIME_ZONE = "set_time_zone";
    private static final String KEY_USE_24_HOUR = "use_24_hour";

    private static final String AUTO_DATE_TIME_NTP = "network";
    private static final String AUTO_DATE_TIME_TS = "transport_stream";
    private static final String AUTO_DATE_TIME_OFF = "off";

    private static final String HOURS_12 = "12";
    private static final String HOURS_24 = "24";

    //    private TvInputManager mTvInputManager;
    private TimeManager mTimeManager;
    private final Calendar mDummyDate = Calendar.getInstance();

    private RestrictedPreferenceAdapter<Preference> mDatePref;
    private RestrictedPreferenceAdapter<Preference> mTimePref;
    private RestrictedPreferenceAdapter<Preference> mTimeZone;
    private Preference mTime24Pref;

    private BroadcastReceiver mIntentReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            final Activity activity = getActivity();
            if (activity != null) {
                updateTimeAndDateDisplay(activity);
            }
        }
    };

    public static DateTimeFragment newInstance() {
        return new DateTimeFragment();
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
//        mTvInputManager =
//                (TvInputManager) getActivity().getSystemService(Context.TV_INPUT_SERVICE);
        super.onCreate(savedInstanceState);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.date_time, null);

        final boolean isRestricted = SecurityFragment.isRestrictedProfileInEffect(getContext());

        Preference datePref = findPreference(KEY_SET_DATE);
        datePref.setVisible(!isRestricted);

        Preference timePref = findPreference(KEY_SET_TIME);
        timePref.setVisible(!isRestricted);

        final boolean tsTimeCapable = SystemProperties.getBoolean("ro.config.ts.date.time", false);
        final ListPreference autoDateTimePref =
                (ListPreference) findPreference(KEY_AUTO_DATE_TIME);
        autoDateTimePref.setValue(getAutoDateTimeState());
        autoDateTimePref.setOnPreferenceChangeListener(this);
        if (tsTimeCapable) {
            autoDateTimePref.setEntries(R.array.auto_date_time_ts_entries);
            autoDateTimePref.setEntryValues(R.array.auto_date_time_ts_entry_values);
        }
        autoDateTimePref.setVisible(!isRestricted);

        Preference timeZonePref = findPreference(KEY_SET_TIME_ZONE);
        timeZonePref.setVisible(!isRestricted);

        mTime24Pref = findPreference(KEY_USE_24_HOUR);
        mTime24Pref.setOnPreferenceChangeListener(this);

        final String userRestriction = UserManager.DISALLOW_CONFIG_DATE_TIME;
        mDatePref = RestrictedPreferenceAdapter.adapt(datePref, userRestriction);
        mTimePref = RestrictedPreferenceAdapter.adapt(timePref, userRestriction);
        mTimeZone = RestrictedPreferenceAdapter.adapt(timeZonePref, userRestriction);
        RestrictedPreferenceAdapter.adapt(autoDateTimePref, userRestriction);
    }

    @Override
    public void onResume() {
        super.onResume();

        ((SwitchPreference)mTime24Pref).setChecked(is24Hour());

        // Register for time ticks and other reasons for time change
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_TIME_TICK);
        filter.addAction(Intent.ACTION_TIME_CHANGED);
        filter.addAction(Intent.ACTION_TIMEZONE_CHANGED);
        getActivity().registerReceiver(mIntentReceiver, filter, null, null);

        updateTimeAndDateDisplay(getActivity());
        updateTimeDateEnable();
    }

    @Override
    public void onPause() {
        super.onPause();
        getActivity().unregisterReceiver(mIntentReceiver);
    }

    private void updateTimeAndDateDisplay(Context context) {
        final Calendar now = Calendar.getInstance();
        mDummyDate.setTimeZone(now.getTimeZone());
        // We use December 31st because it's unambiguous when demonstrating the date format.
        // We use 13:00 so we can demonstrate the 12/24 hour options.
        mDummyDate.set(now.get(Calendar.YEAR), 11, 31, 13, 0, 0);
        Date dummyDate = mDummyDate.getTime();

        mDatePref.updatePreference(pref ->
                pref.setSummary(DateFormat.getLongDateFormat(context).format(now.getTime())));
        mTimePref.updatePreference(pref ->
                pref.setSummary(DateFormat.getTimeFormat(getActivity()).format(now.getTime())));
        mTimeZone.updatePreference(pref ->
                pref.setSummary(ZoneGetter.getTimeZoneOffsetAndName(getActivity(),
                        now.getTimeZone(), now.getTime())));

        mTime24Pref.setSummary(DateFormat.getTimeFormat(getActivity()).format(dummyDate));
    }

    private void updateTimeDateEnable() {
        final boolean enable = TextUtils.equals(getAutoDateTimeState(), AUTO_DATE_TIME_OFF);

        mDatePref.updatePreference(pref -> pref.setEnabled(enable));
        mTimePref.updatePreference(pref -> pref.setEnabled(enable));

        mTimeManager = (TimeManager) getActivity().getSystemService(Context.TIME_MANAGER_SERVICE);
        TimeZoneCapabilitiesAndConfig capabilitiesAndConfig = mTimeManager.getTimeZoneCapabilitiesAndConfig();
        TimeZoneCapabilities capabilities = capabilitiesAndConfig.getCapabilities();
        final boolean setTimeZoneEnable = capabilities.getSetManualTimeZoneCapability() == CAPABILITY_POSSESSED;
        mTimeZone.updatePreference(pref -> pref.setEnabled(setTimeZoneEnable));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (TextUtils.equals(preference.getKey(), KEY_AUTO_DATE_TIME)) {
            String value = (String) newValue;
            if (TextUtils.equals(value, AUTO_DATE_TIME_NTP)) {
                logEntrySelected(TvSettingsEnums.SYSTEM_DATE_TIME_AUTOMATIC_USE_NETWORK_TIME);
                setAutoDateTime(true);
            } else if (TextUtils.equals(value, AUTO_DATE_TIME_TS)) {
                throw new IllegalStateException("TS date is not yet implemented");
//                mTvInputManager.syncTimefromBroadcast(true);
//                setAutoDateTime(false);
            } else if (TextUtils.equals(value, AUTO_DATE_TIME_OFF)) {
                logEntrySelected(TvSettingsEnums.SYSTEM_DATE_TIME_AUTOMATIC_OFF);
                setAutoDateTime(false);
            } else {
                throw new IllegalArgumentException("Unknown auto time value " + value);
            }
            updateTimeDateEnable();
        } else if (TextUtils.equals(preference.getKey(), KEY_USE_24_HOUR)) {
            final boolean use24Hour = (Boolean) newValue;
            logToggleInteracted(TvSettingsEnums.SYSTEM_DATE_TIME_USE_24_HOUR_FORMAT, use24Hour);
            set24Hour(use24Hour);
            timeUpdated(use24Hour);
        }
        return true;
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        switch (preference.getKey()) {
            case KEY_SET_DATE:
                logEntrySelected(TvSettingsEnums.SYSTEM_DATE_TIME_SET_DATE);
                break;
            case KEY_SET_TIME:
                logEntrySelected(TvSettingsEnums.SYSTEM_DATE_TIME_SET_TIME);
                break;
        }
        return super.onPreferenceTreeClick(preference);
    }

    /*  Get & Set values from the system settings  */

    private boolean is24Hour() {
        return DateFormat.is24HourFormat(getActivity());
    }

    private void timeUpdated(boolean use24Hour) {
        Intent timeChanged = new Intent(Intent.ACTION_TIME_CHANGED);
        int timeFormatPreference =
                use24Hour ? Intent.EXTRA_TIME_PREF_VALUE_USE_24_HOUR
                        : Intent.EXTRA_TIME_PREF_VALUE_USE_12_HOUR;
        timeChanged.putExtra(Intent.EXTRA_TIME_PREF_24_HOUR_FORMAT, timeFormatPreference);
        getContext().sendBroadcast(timeChanged);
    }

    private void set24Hour(boolean use24Hour) {
        Settings.System.putString(getContext().getContentResolver(),
                Settings.System.TIME_12_24,
                use24Hour ? HOURS_24 : HOURS_12);
    }

    private void setAutoDateTime(boolean on) {
        Settings.Global.putInt(getContext().getContentResolver(),
                Settings.Global.AUTO_TIME, on ? 1 : 0);
    }

    private String getAutoDateTimeState() {
//        if(mTvInputManager.isUseBroadcastDateTime()) {
//            return AUTO_DATE_TIME_TS;
//        }

        int value = Settings.Global.getInt(getContext().getContentResolver(),
                Settings.Global.AUTO_TIME, 0);
        if(value > 0) {
            return AUTO_DATE_TIME_NTP;
        }

        return AUTO_DATE_TIME_OFF;
    }

    @Override
    protected int getPageId() {
        return TvSettingsEnums.SYSTEM_DATE_TIME;
    }
}
