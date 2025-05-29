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

package com.android.DeviceAsWebcam.view;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.animation.AccelerateDecelerateInterpolator;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.android.DeviceAsWebcam.CameraId;
import com.android.DeviceAsWebcam.R;

import java.util.List;
import java.util.function.Consumer;

/**
 * A class to provide a switch-camera selector view for end users to select a preferred camera.
 */
public class SwitchCameraSelectorView extends FrameLayout {
    private List<SelectorListItemData> mSelectorListItemDataList;
    private View mRootView;
    private View mRecyclerViewContainerView;
    private RecyclerView mRecyclerView;
    private CameraDataAdapter mCameraDataAdapter;
    private RecyclerView.LayoutManager mLayoutManager;
    private Consumer<Integer> mVisibilityChangedListener = null;

    public SwitchCameraSelectorView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the switch camera selector view.
     */
    public void init(LayoutInflater layoutInflater, List<SelectorListItemData> itemDataList) {
        removeAllViews();
        addView(layoutInflater.inflate(R.layout.switch_camera_selector, null));
        mSelectorListItemDataList = itemDataList;

        mRootView = findViewById(R.id.switch_camera_selector_root);
        mRecyclerViewContainerView = findViewById(R.id.camera_id_list_container);
        mRecyclerView = findViewById(R.id.camera_id_list_view);
        findViewById(R.id.list_view_top_item).setEnabled(false);
        findViewById(R.id.list_view_bottom_item).setEnabled(false);

        mCameraDataAdapter = new CameraDataAdapter(mSelectorListItemDataList, getResources());
        mLayoutManager = new LinearLayoutManager(getContext());
        mRecyclerView.setLayoutManager(mLayoutManager);
        mRecyclerView.setAdapter(mCameraDataAdapter);

        mRootView.setOnClickListener(v -> hide());

        // Hides the camera selector in the beginning
        hide();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // Dynamically calculates the suitable height for the dialog view because the dialog view
        // is rotated from a portrait orientation layout to landscape.
        if (mRecyclerViewContainerView != null) {
            int listItemHeight = getResources().getDimensionPixelSize(
                    R.dimen.list_item_height);
            int topBottomHeight = getResources().getDimensionPixelSize(
                    R.dimen.list_item_top_bottom_height);
            int dialogHeight =
                    listItemHeight * mSelectorListItemDataList.size() + topBottomHeight * 2;
            int margin = getResources().getDimensionPixelSize(
                    R.dimen.camera_selector_top_bottom_margin);
            int expectedMargin = (getHeight() - dialogHeight) / 2;
            int minMargin = (getHeight() - getWidth()) / 2 + margin;
            int finalMargin = Math.max(expectedMargin, minMargin);
            ConstraintLayout.LayoutParams lp =
                    (ConstraintLayout.LayoutParams) mRecyclerViewContainerView.getLayoutParams();
            lp.topMargin = finalMargin;
            lp.bottomMargin = finalMargin;
            mRecyclerViewContainerView.setLayoutParams(lp);
        }
    }

    /**
     * Rotates the switch camera selector dialog.
     */
    public void setRotation(int rotation) {
        ObjectAnimator anim = ObjectAnimator.ofFloat(mRecyclerViewContainerView,
                        /*propertyName=*/"rotation", rotation)
                .setDuration(300);
        anim.setInterpolator(new AccelerateDecelerateInterpolator());
        anim.start();
    }

    /**
     * Sets a listener to receive the camera selection change events.
     */
    public void setOnCameraSelectedListener(Consumer<CameraId> listener) {
        mCameraDataAdapter.setOnCameraSelectedListener(listener);
    }

    /**
     * Sets a listener to receive the visibility change events.
     */
    public void setOnVisibilityChangedListener(@Nullable Consumer<Integer> listener) {
        mVisibilityChangedListener = listener;
    }

    /**
     * Shows the switch camera selector view.
     */
    public void show() {
        mRootView.setAlpha(0f);
        mRootView.setVisibility(VISIBLE);
        if (mVisibilityChangedListener != null) {
            mVisibilityChangedListener.accept(VISIBLE);
        }

        mRootView.animate()
                .alpha(1f)
                .setDuration(300)
                .setListener(null);
    }

    /**
     * Hides the switch camera selector view.
     */
    public void hide() {
        mRootView.setVisibility(GONE);
        if (mVisibilityChangedListener != null) {
            mVisibilityChangedListener.accept(GONE);
        }
    }

    /**
     * Updates the selected item.
     */
    public void updateSelectedItem(CameraId cameraId) {
        for (SelectorListItemData itemData : mSelectorListItemDataList) {
            if (itemData.isHeader) {
                continue;
            }
            boolean oldCheckedState = itemData.isChecked;
            itemData.isChecked = itemData.cameraInfo.getCameraId().equals(cameraId);
            if (oldCheckedState != itemData.isChecked) {
                mCameraDataAdapter.notifyItemChanged(
                        mSelectorListItemDataList.indexOf(itemData));
            }
            if (itemData.isChecked) {
                mRecyclerView.scrollToPosition(mSelectorListItemDataList.indexOf(itemData));
            }
        }
    }
}
