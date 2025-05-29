/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.settings.screenshot;

import android.app.settings.SettingsEnums;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.Gravity;
import android.view.WindowManager;
import android.view.ViewGroup.LayoutParams;
import android.widget.TextView;

import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.SwitchPreference;

import com.android.settings.R;
import com.android.settings.SettingsActivity;
import com.android.settings.dashboard.DashboardFragment;
import com.android.settings.search.BaseSearchIndexProvider;
import com.android.settingslib.search.SearchIndexable;

import java.util.Timer;
import java.util.TimerTask;

@SearchIndexable
public class ScreenshotSettings extends DashboardFragment implements Preference.OnPreferenceClickListener, Preference.OnPreferenceChangeListener {

    private static final String TAG = "ScreenshotSettings";
    private static final String SCREENSHOT_SHOW_KEY = "screenshot_show";
    private static final String SCREENSHOT_DELAY_KEY = "screenshot_delay";
    private static final String DEFAULT_DELAY = "15";
    private int mDelayTime;
    private Context mContext;
    private ListPreference mDelay;
    private SwitchPreference mShow;
    private TextView text;
    private Timer timer;
    private WindowManager mWindowManager;
    private WindowManager.LayoutParams params;

    @Override
    public int getMetricsCategory() {
        return 5;
    }

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        mDelay = (ListPreference) findPreference(SCREENSHOT_DELAY_KEY);
        mDelay.setSummary(DEFAULT_DELAY + getString(R.string.later));
        mDelay.setValue(DEFAULT_DELAY);
        mDelay.setOnPreferenceChangeListener(this);
        mShow = (SwitchPreference) findPreference(SCREENSHOT_SHOW_KEY);
        Resources res = mContext.getResources();
        boolean mHasNavigationBar = res.getBoolean(com.android.internal.R.bool.config_showNavigationBar);
        if (!mHasNavigationBar) {
            getPreferenceScreen().removePreference(mShow);
        }
        boolean isShow = Settings.System.getInt(mContext.getContentResolver(), "screenshot_button_show", 0) == 1;
        mShow.setChecked(isShow);
        mShow.setOnPreferenceChangeListener(this);
        initCountdownView();
    }


    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.screenshot_settings;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    public int getHelpResource() {
        return R.string.help_url_screenshot;
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (SCREENSHOT_SHOW_KEY.equals(preference.getKey())) {
            boolean show = (boolean)newValue;
            Settings.System.putInt(mContext.getContentResolver(), "screenshot_button_show", show ? 1 : 0);
        } else if(SCREENSHOT_DELAY_KEY.equals(preference.getKey())){
            int value = Integer.parseInt((String) newValue);
            mDelay.setSummary((String) newValue + getString(R.string.later));
            startScreenshot(value);
        }
        return true;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        return true;
    }

    /**
     * For Search.
     */
    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(R.xml.screenshot_settings);

    private Handler mHandler = new Handler() {
        public void handleMessage(android.os.Message msg) {
            mDelayTime--;
            Log.d("screenshot", "handleMessage" + "mDelayTime=" + String.valueOf(mDelayTime));
            if (mDelayTime > 1) {
                if (text.getParent() == null) {
                    mWindowManager.addView(text, params);
                } else {
                    text.invalidate();
                }
            } else {
                if (text.getParent() != null) {
                    mWindowManager.removeView(text);
                }
                timer.cancel();
                Intent intent = new Intent();
                intent.setAction("android.intent.action.SCREENSHOT");
                mContext.sendBroadcast(intent);
            }
        };
    };

    public void startScreenshot(int delay) {
        if (timer != null) {
            timer.cancel();
        }
        timer = new Timer();
        mDelayTime = delay;
        timer.schedule(new TimerTask() {

            @Override
            public void run() {
                // TODO Auto-generated method stub
                mHandler.sendEmptyMessage(1);

            }
        }, 1000, 1000);
    }

    private void initCountdownView() {
        text = new CountdownView(mContext);
        text.setLayoutParams(new LayoutParams(40, 40));
        text.setTextSize(40);
        params = new WindowManager.LayoutParams();
        params.type = WindowManager.LayoutParams.TYPE_SYSTEM_ALERT | WindowManager.LayoutParams.TYPE_SYSTEM_OVERLAY;
        params.type = 2006;
        params.flags = WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        params.width = WindowManager.LayoutParams.WRAP_CONTENT;
        params.height = WindowManager.LayoutParams.WRAP_CONTENT;
        params.format = PixelFormat.TRANSLUCENT;
        params.gravity = Gravity.LEFT | Gravity.BOTTOM;
        params.width = 50;
        params.height = 40;
        mWindowManager = (WindowManager) mContext.getSystemService(mContext.WINDOW_SERVICE);
    }

    class CountdownView extends TextView {

        public CountdownView(Context context) {
            super(context);
            // TODO Auto-generated constructor stub
        }

        @Override
        protected void onDraw(Canvas canvas) {
            // TODO Auto-generated method stub
            canvas.drawColor(0, PorterDuff.Mode.CLEAR);
            Paint paint = new Paint();
            paint.setColor(Color.RED);
            paint.setTypeface(Typeface.DEFAULT_BOLD);
            paint.setTextSize(30);
            canvas.drawText(String.valueOf(mDelayTime), 0, 40, paint);
            super.onDraw(canvas);

        }
    }
}
