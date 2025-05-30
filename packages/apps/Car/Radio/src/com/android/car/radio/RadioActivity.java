/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.radio;

import static com.android.car.ui.core.CarUi.requireToolbar;
import static com.android.car.ui.toolbar.Toolbar.State.HOME;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.view.KeyEvent;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager.widget.ViewPager;

import com.android.car.media.common.source.MediaSource;
import com.android.car.media.common.source.MediaTrampolineHelper;
import com.android.car.radio.bands.ProgramType;
import com.android.car.radio.service.RadioAppService;
import com.android.car.radio.service.RadioAppServiceWrapper;
import com.android.car.radio.util.Log;
import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.TabLayout;
import com.android.car.ui.toolbar.ToolbarController;

import java.util.ArrayList;
import java.util.List;

/**
 * The main activity for the radio app.
 */
public class RadioActivity extends FragmentActivity implements InsetsChangedListener {
    private static final String TAG = "BcRadioApp.activity";

    /**
     * Intent action for notifying that the radio state has changed.
     */
    private static final String ACTION_RADIO_APP_STATE_CHANGE =
            "android.intent.action.RADIO_APP_STATE_CHANGE";

    /**
     * Boolean Intent extra indicating if the radio is the currently in the foreground.
     */
    private static final String EXTRA_RADIO_APP_FOREGROUND =
            "android.intent.action.RADIO_APP_STATE";

    private boolean mIsConnected;
    private RadioController mRadioController;
    private BandController mBandController = new BandController();
    private ToolbarController mToolbar;
    private ViewPager mViewPager;
    private RadioPagerAdapter mRadioPagerAdapter;

    private boolean mUseSourceLogoForAppSelector;
    private MediaTrampolineHelper mMediaTrampoline;

    @Override
    public void onCarUiInsetsChanged(Insets insets) {
        // This InsetsChangedListener is just a marker that we will later handle
        // insets in fragments, since the fragments aren't added immediately.
        // Otherwise CarUi will apply the insets to the content view incorrectly.
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(TAG, "Radio app main activity created");

        setContentView(R.layout.radio_activity);

        mMediaTrampoline = new MediaTrampolineHelper(this);

        mRadioController = new RadioController(this);

        mRadioController.getConnectionState().observe(this, this::onConnectionStateChanged);

        mRadioController.getCurrentProgram().observe(this, info -> {
            ProgramType programType = ProgramType.fromSelector(info.getSelector());
            if (programType != null) {
                mBandController.setType(programType);
                updateMenuItems();
            }
        });

        mRadioPagerAdapter =
                new RadioPagerAdapter(this, getSupportFragmentManager(), mRadioController);
        mViewPager = findViewById(R.id.viewpager);

        mUseSourceLogoForAppSelector =
                getResources().getBoolean(R.bool.use_media_source_logo_for_app_selector);

        mToolbar = requireToolbar(this);
        mToolbar.setState(HOME);
        mToolbar.setTitle(R.string.app_name);
        if (!mUseSourceLogoForAppSelector) {
            mToolbar.setLogo(R.drawable.logo_fm_radio);
        }
        mToolbar.registerOnTabSelectedListener(t -> {
            if (mIsConnected) {
                mViewPager.setCurrentItem(mToolbar.getTabPosition(t));
            }
        });

        mViewPager.addOnPageChangeListener(new ViewPager.SimpleOnPageChangeListener() {
            @Override
            public void onPageSelected(int position) {
                mToolbar.selectTab(position);
            }
        });
    }

    @Override
    protected void onStart() {
        super.onStart();

        mRadioController.start();

        Intent broadcast = new Intent(ACTION_RADIO_APP_STATE_CHANGE);
        broadcast.putExtra(EXTRA_RADIO_APP_FOREGROUND, true);
        sendBroadcast(broadcast);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent); // getIntent() should always return the most recent

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "onNewIntent: " + intent);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        Intent intent = getIntent();
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "onResume intent: " + intent);
        }

        if (intent != null) {
            mMediaTrampoline.setLaunchedMediaSource(RadioAppService.getMediaSourceComp(this));

            // Mark the intent as consumed so that coming back from the media app selector doesn't
            // set the source again.
            setIntent(null);
        }
    }

    @Override
    protected void onStop() {
        super.onStop();

        Intent broadcast = new Intent(ACTION_RADIO_APP_STATE_CHANGE);
        broadcast.putExtra(EXTRA_RADIO_APP_FOREGROUND, false);
        sendBroadcast(broadcast);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        mRadioController.shutdown();

        Log.d(TAG, "Radio app main activity destroyed");
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_MEDIA_STEP_FORWARD:
                mRadioController.step(true);
                return true;
            case KeyEvent.KEYCODE_MEDIA_STEP_BACKWARD:
                mRadioController.step(false);
                return true;
            default:
                return super.onKeyDown(keyCode, event);
        }
    }

    /**
     * Set whether background scanning is supported, to know whether to show the browse tab or not.
     */
    public void setProgramListSupported(boolean supported) {
        if (supported && mRadioPagerAdapter.addBrowseTab()) {
            updateTabs();
        }
    }

    /**
     * Sets supported program types.
     */
    public void setSupportedProgramTypes(@NonNull List<ProgramType> supported) {
        mBandController.setSupportedProgramTypes(supported);
    }

    private void onConnectionStateChanged(@RadioAppServiceWrapper.ConnectionState int state) {
        if (state == RadioAppServiceWrapper.STATE_CONNECTED) {
            mIsConnected = true;
            mViewPager.setAdapter(mRadioPagerAdapter);
        } else {
            mIsConnected = false;
            mViewPager.setAdapter(null);
        }
        Log.i(TAG, "onConnectionStateChanged connected: " + mIsConnected);
        updateMenuItems();
        updateTabs();
    }

    private void updateMenuItems() {
        ProgramType currentBand = mBandController.getCurrentBand();
        MenuItem bandSelectorMenuItem = MenuItem.builder(this)
                .setIcon(currentBand != null ? currentBand.getResourceId() : 0)
                .setOnClickListener(i -> {
                    ProgramType programType = mBandController.switchToNext();
                    mRadioController.switchBand(programType);
                })
                .build();

        Intent appSelectorIntent = MediaSource.getSourceSelectorIntent(this, false);
        MenuItem appSelectorMenuItem = MenuItem.builder(this)
                .setIcon(mUseSourceLogoForAppSelector
                        ? R.drawable.logo_fm_radio : R.drawable.ic_app_switch)
                .setTinted(!mUseSourceLogoForAppSelector)
                .setOnClickListener(m -> startActivity(appSelectorIntent))
                .build();

        ArrayList<MenuItem> menuItems = new ArrayList<>(2);
        if (mIsConnected) {
            menuItems.add(bandSelectorMenuItem);
        }
        menuItems.add(appSelectorMenuItem);
        mToolbar.setMenuItems(menuItems);
    }

    private void updateTabs() {
        mToolbar.clearAllTabs();
        if (mIsConnected) {
            for (int i = 0; i < mRadioPagerAdapter.getCount(); i++) {
                Drawable icon = mRadioPagerAdapter.getPageIcon(i);
                CharSequence title = mRadioPagerAdapter.getPageTitle(i);
                mToolbar.addTab(new TabLayout.Tab(icon, title));
            }
        }
    }
}
