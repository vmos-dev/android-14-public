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

import com.android.DeviceAsWebcam.CameraInfo;

/**
 * A class for providing the switch camera selector list item data info.
 */
public class SelectorListItemData {
    public final boolean isHeader;
    public final int lensFacing;
    public final CameraInfo cameraInfo;
    public boolean isChecked = false;

    private SelectorListItemData(boolean isHeader, CameraInfo cameraInfo) {
        this.isHeader = isHeader;
        this.cameraInfo = cameraInfo;
        lensFacing = cameraInfo.getLensFacing();
    }

    private SelectorListItemData(boolean isHeader, int lensFacing) {
        this.isHeader = isHeader;
        this.lensFacing = lensFacing;
        cameraInfo = null;
    }

    /**
     * Creates a header list item data for the specific lens facing.
     */
    public static SelectorListItemData createHeaderItemData(int lensFacing) {
        return new SelectorListItemData(true, lensFacing);
    }

    /**
     * Creates a camera list item data.
     */
    public static SelectorListItemData createCameraItemData(CameraInfo cameraInfo) {
        return new SelectorListItemData(false, cameraInfo);
    }
}
