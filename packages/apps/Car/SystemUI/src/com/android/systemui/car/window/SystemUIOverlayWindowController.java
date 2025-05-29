/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.systemui.car.window;

import static android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;
import static android.view.WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Binder;
import android.view.Display;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.util.Log;
import com.android.systemui.CarSystemUIApplication;
import com.android.systemui.R;
import com.android.systemui.car.users.CarSystemUIUserUtil;
import com.android.systemui.dagger.SysUISingleton;
import com.android.systemui.statusbar.policy.ConfigurationController;

import javax.inject.Inject;

/**
 * Controls the expansion state of the primary window which will contain all of the fullscreen sysui
 * behavior. This window still has a collapsed state in order to watch for swipe events to expand
 * this window for the notification panel.
 */
@SysUISingleton
public class SystemUIOverlayWindowController implements
        ConfigurationController.ConfigurationListener {

    /**
     * Touch listener to get touches on the view.
     */
    public interface OnTouchListener {

        /**
         * Called when a touch happens on the view.
         */
        void onTouch(View v, MotionEvent event);
    }

    private final Context mContext;
    private final WindowManager mWindowManager;

    private ViewGroup mBaseLayout;
    private WindowManager.LayoutParams mLp;
    private WindowManager.LayoutParams mLpChanged;
    private boolean mIsAttached = false;
    private boolean mVisible = false;
    private boolean mFocusable = false;
    private boolean mUsingStableInsets = false;

    @Inject
    public SystemUIOverlayWindowController(
            Context context,
            WindowManager windowManager,
            ConfigurationController configurationController) {
        Log.i("SystemUIOverlayWindowController", "inject displayId = " + context.getDisplayId());
        mContext = context;
        mWindowManager = windowManager;

        mLpChanged = new WindowManager.LayoutParams();
        mBaseLayout = (ViewGroup) LayoutInflater.from(context)
                .inflate(R.layout.sysui_overlay_window, /* root= */ null, false);
        configurationController.addCallback(this);
    }

    /**
     * Register to {@link OnTouchListener}
     */
    public void registerOutsideTouchListener(OnTouchListener listener) {
        mBaseLayout.setOnTouchListener(new View.OnTouchListener() {
            @Override
            public boolean onTouch(View v, MotionEvent event) {
                listener.onTouch(v, event);
                return false;
            }
        });
    }

    /** Returns the base view of the primary window. */
    public ViewGroup getBaseLayout() {
        return mBaseLayout;
    }

    /** Returns {@code true} if the window is already attached. */
    public boolean isAttached() {
        return mIsAttached;
    }

    /** Attaches the window to the window manager. */
    public void attach() {
        int applicantDisplayId = ((CarSystemUIApplication) mContext.getApplicationContext()).getMyOccupantZoneDisplayId();
        if (CarSystemUIUserUtil.isSecondaryMUMDSystemUI() && (applicantDisplayId == Display.INVALID_DISPLAY || applicantDisplayId == Display.DEFAULT_DISPLAY)) {
            Log.i("SystemUIOverlayWindowController", "attach failed, secondary MUMD displayId = " + mContext.getDisplayId() + ", applicantDisplayId = " + applicantDisplayId);
            return;
        }
        mContext.updateDisplay(applicantDisplayId);
        if (mIsAttached) {
            return;
        }
        mIsAttached = true;
        // Now that the status bar window encompasses the sliding panel and its
        // translucent backdrop, the entire thing is made TRANSLUCENT and is
        // hardware-accelerated.
        mLp = new WindowManager.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_NOTIFICATION_SHADE,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_TOUCHABLE_WHEN_WAKING
                        | WindowManager.LayoutParams.FLAG_SPLIT_TOUCH
                        | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH
                        | WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS,
                PixelFormat.TRANSLUCENT);
        mLp.token = new Binder();
        mLp.gravity = Gravity.TOP;
        mLp.softInputMode = WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE;
        mLp.setTitle("SystemUIOverlayWindow");
        mLp.packageName = mContext.getPackageName();
        mLp.layoutInDisplayCutoutMode = LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        mLp.insetsFlags.behavior = BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;

        mWindowManager.addView(mBaseLayout, mLp);
        mLpChanged.copyFrom(mLp);
        setWindowVisible(false);
    }

    /** Sets the types of insets to fit. Note: This should be rarely used. */
    public void setFitInsetsTypes(@WindowInsets.Type.InsetsType int types) {
        mLpChanged.setFitInsetsTypes(types);
        mLpChanged.setFitInsetsIgnoringVisibility(mUsingStableInsets);
        updateWindow();
    }

    /** Sets the sides of system bar insets to fit. Note: This should be rarely used. */
    public void setFitInsetsSides(@WindowInsets.Side.InsetsSide int sides) {
        mLpChanged.setFitInsetsSides(sides);
        mLpChanged.setFitInsetsIgnoringVisibility(mUsingStableInsets);
        updateWindow();
    }

    /** Sets the window to the visible state. */
    public void setWindowVisible(boolean visible) {
        mVisible = visible;
        if (visible) {
            mBaseLayout.setVisibility(View.VISIBLE);
        } else {
            mBaseLayout.setVisibility(View.INVISIBLE);
        }
        updateWindow();
    }

    /** Sets the window to be focusable. */
    public void setWindowFocusable(boolean focusable) {
        mFocusable = focusable;
        if (focusable) {
            mLpChanged.flags &= ~WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        } else {
            mLpChanged.flags |= WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        }
        updateWindow();
    }

    /** Sets the window to enable IME. */
    public void setWindowNeedsInput(boolean needsInput) {
        if (needsInput) {
            mLpChanged.flags &= ~WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM;
        } else {
            mLpChanged.flags |= WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM;
        }
        updateWindow();
    }

    /** Returns {@code true} if the window is visible */
    public boolean isWindowVisible() {
        return mVisible;
    }

    public boolean isWindowFocusable() {
        return mFocusable;
    }

    protected void setUsingStableInsets(boolean useStableInsets) {
        mUsingStableInsets = useStableInsets;
    }

    private void updateWindow() {
        if (mLp != null && mLp.copyFrom(mLpChanged) != 0) {
            if (isAttached()) {
                mLp.insetsFlags.behavior = BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE;
                mWindowManager.updateViewLayout(mBaseLayout, mLp);
            }
        }
    }
}
