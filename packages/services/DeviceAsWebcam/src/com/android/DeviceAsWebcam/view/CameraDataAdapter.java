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

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.hardware.camera2.CameraCharacteristics;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.android.DeviceAsWebcam.CameraId;
import com.android.DeviceAsWebcam.R;

import java.util.List;
import java.util.Objects;
import java.util.function.Consumer;

/**
 * A RecyclerView.Adapter for the camera selection setting view
 */
public class CameraDataAdapter extends RecyclerView.Adapter<CameraDataAdapter.ViewHolder> {
    /**
     * Used for a header type list item that can't be selected by the end users.
     */
    private static final int TYPE_HEADER = 0;
    /**
     * Used for a camera item type list item that can be selected by the end users.
     */
    private static final int TYPE_CAMERA_ITEM = 1;
    private final List<SelectorListItemData> mSelectorListItemDataList;
    private final Resources mResources;
    private Consumer<CameraId> mCameraSelectedListener = null;

    /**
     * ViewHolder of the CameraDataAdapter.
     */
    public static class ViewHolder extends RecyclerView.ViewHolder {
        private final View mListItemView;
        private final TextView mTextView;
        private final RadioButton mRadioButton;

        public ViewHolder(View view) {
            super(view);
            mListItemView = view;
            mTextView = view.findViewById(R.id.text_view);
            mRadioButton = view.findViewById(R.id.radio_button);
        }
    }

    /**
     * Initializes the dataset of the Adapter
     */
    public CameraDataAdapter(List<SelectorListItemData> selectorListItemDataList,
            Resources resources) {
        mSelectorListItemDataList = selectorListItemDataList;
        mResources = resources;
    }

    @Override
    public int getItemViewType(int position) {
        return mSelectorListItemDataList.get(position).isHeader ? TYPE_HEADER : TYPE_CAMERA_ITEM;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup viewGroup, int viewType) {
        int layoutId = switch (viewType) {
            case TYPE_HEADER -> R.layout.list_item_header;
            case TYPE_CAMERA_ITEM -> R.layout.list_item_camera;
            default -> 0;
        };

        View view = LayoutInflater.from(viewGroup.getContext()).inflate(layoutId, viewGroup, false);

        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(ViewHolder viewHolder, final int position) {
        SelectorListItemData itemData = mSelectorListItemDataList.get(position);

        if (viewHolder.mTextView == null) {
            return;
        }

        int drawableRsId;
        int textRsId;

        if (itemData.isHeader) {
            drawableRsId = itemData.lensFacing == CameraCharacteristics.LENS_FACING_BACK
                    ? R.drawable.ic_camera_rear : R.drawable.ic_camera_front;
            textRsId = itemData.lensFacing == CameraCharacteristics.LENS_FACING_BACK
                    ? R.string.list_item_text_back_camera : R.string.list_item_text_front_camera;
        } else {
            switch (Objects.requireNonNull(itemData.cameraInfo).getCameraCategory()) {
                case STANDARD -> {
                    drawableRsId = R.drawable.ic_camera_category_standard;
                    textRsId = R.string.list_item_text_standard_camera;
                }
                case WIDE_ANGLE -> {
                    drawableRsId = R.drawable.ic_camera_category_wide_angle;
                    textRsId = R.string.list_item_text_wide_angle_camera;
                }
                case ULTRA_WIDE -> {
                    drawableRsId = R.drawable.ic_camera_category_wide_angle;
                    textRsId = R.string.list_item_text_ultra_wide_camera;
                }
                case TELEPHOTO -> {
                    drawableRsId = R.drawable.ic_camera_category_telephoto;
                    textRsId = R.string.list_item_text_telephoto_camera;
                }
                case OTHER -> {
                    drawableRsId = R.drawable.ic_camera_category_other;
                    textRsId = R.string.list_item_text_other_camera;
                }
                default -> {
                    drawableRsId = R.drawable.ic_camera_category_unknown;
                    textRsId = R.string.list_item_text_unknown_camera;
                }
            }
        }

        updateTextView(viewHolder.mTextView, drawableRsId, textRsId);

        if (itemData.isHeader) {
            return;
        }

        final RadioButton currentRadioButton = viewHolder.mRadioButton;

        if (currentRadioButton != null) {
            currentRadioButton.setChecked(mSelectorListItemDataList.get(position).isChecked);
        }

        viewHolder.mListItemView.setOnClickListener(v -> {
            if (currentRadioButton != null) {
                if (mSelectorListItemDataList.get(position).isChecked) {
                    return;
                }

                currentRadioButton.setChecked(true);
                CameraId currentCameraId = mSelectorListItemDataList.get(
                        position).cameraInfo.getCameraId();

                for (SelectorListItemData listItemData : mSelectorListItemDataList) {
                    if (listItemData.isHeader) {
                        continue;
                    }
                    boolean oldCheckedState = listItemData.isChecked;
                    listItemData.isChecked = Objects.requireNonNull(
                            listItemData.cameraInfo).getCameraId().equals(currentCameraId);
                    if (oldCheckedState != listItemData.isChecked) {
                        CameraDataAdapter.this.notifyItemChanged(
                                mSelectorListItemDataList.indexOf(listItemData));
                    }
                }

                if (mCameraSelectedListener != null) {
                    mCameraSelectedListener.accept(currentCameraId);
                }
            }
        });
    }

    private void updateTextView(TextView textView, int drawableRsId, int textRsId) {
        textView.setText(textRsId);
        Drawable drawable = mResources.getDrawable(drawableRsId, null);
        int size = mResources.getDimensionPixelSize(R.dimen.list_item_icon_size);
        drawable.setBounds(0, 0, size, size);
        textView.setCompoundDrawables(drawable, null, null, null);
    }

    @Override
    public int getItemCount() {
        return mSelectorListItemDataList.size();
    }

    /**
     * Sets a listener to receive the camera selection change events.
     */
    void setOnCameraSelectedListener(Consumer<CameraId> listener) {
        mCameraSelectedListener = listener;
    }
}
