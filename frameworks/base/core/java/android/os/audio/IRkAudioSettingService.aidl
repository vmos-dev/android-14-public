/* $_FOR_ROCKCHIP_RBOX_$ */
//$_rbox_$_modify_$_wentao_20120220: Rbox android audio manager class

/*
 * Copyright 2024 Rockchip Electronics Co. LTD
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
package android.os.audio;

/**
 * @hide
 */
interface IRkAudioSettingService {
    int getSelect(int device);
    void setSelect(int device);
    int getMode(int device);
    void setMode(int device, int mode);
    int getFormat(int device, String format);
    void setFormat(int device, int close, String format);
}
