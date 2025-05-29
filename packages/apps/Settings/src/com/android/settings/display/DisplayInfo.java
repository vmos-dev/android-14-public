/*
 * Copyright 2023 Rockchip Electronics S.LSI Co. LTD
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

package com.android.settings.display;

import android.util.Log;

import java.io.Serializable;
import java.util.Arrays;

public class DisplayInfo implements Serializable {
    private int displayId;
    private int displayNo;
    private int status;
    private int type;
    private String description;
    private String[] modes;
    private String[] orginModes;
    private String currentResolution;
    private String lastResolution = "";//""Auto";

    public int getDisplayId() {
        return displayId;
    }

    public void setDisplayId(int displayId) {
        this.displayId = displayId;
    }
    public int getDisplayNo() {
        return displayNo;
    }

    public void setDisplayNo(int display) {
        this.displayNo = display;
    }

    public int getStatus() {
        return status;
    }

    public void setStatus(int status) {
        this.status = status;
    }

    public int getType() {
        return type;
    }

    public void setType(int type) {
        this.type = type;
    }

    public String getDescription() {
        return description;
    }

    public void setDescription(String description) {
        this.description = description;
    }

    public String[] getModes() {
        return modes;
    }

    public void setModes(String[] modes) {
        this.modes = modes;
    }

    public String[] getOrginModes() {
        return orginModes;
    }

    public void setOrginModes(String[] orginModes) {
        this.orginModes = orginModes;
    }

    public String getCurrentResolution() {
        return currentResolution;
    }

    public void setCurrentResolution(String currentResolution) {
        this.currentResolution = currentResolution;
    }

    public String getLastResolution() {
        return lastResolution;
    }

    public void setLastResolution(String lastResolution) {
        Log.i("HdmiSettings", "setLastResolution " + lastResolution);
        this.lastResolution = lastResolution;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder();
        builder.append("displayId:").append(displayId).append("  ")
                .append("type:").append(type).append("  ")
                .append("description:").append(description).append("  ")
                .append("modes:").append(Arrays.toString(modes));
        return builder.toString();
    }
}
