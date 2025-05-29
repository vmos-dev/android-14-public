/* $_FOR_ROCKCHIP_RBOX_$ */
//$_rbox_$_modify_$_zhengyang_20120220: AIDL file for rbox android display management service.

/* //device/java/android/android/os/IDisplayDeviceManagementService.aidl
**
** Copyright 2007, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

package android.os;

/**
 * @hide
 */
interface IRkDisplayDeviceManagementService
{
    /**
     ** GENERAL
     **/

     /**
     * Returns a list of currently known display interfaces
     */
    String[] listInterfaces(int display);

     /**
     * Returns current drm connectors
     */
    int getDisplayNumbers();

    /**
     * Returns the current enabled display interfaces
     */
    String getCurrentInterface(int display);

    /**
     * Returns a list of supported display modes
     */
    String[] getModelist(int display, String iface);

    /**
     * Returns current display mode
     */
    String getMode(int display, String iface);

    /**
     * Returns current display connection state
     */
    int getDpyConnState(int display);

    /**
     * Set display mode
     */
    void setMode(int display, String iface, String mode);

    /**
     * Set color mode format-depth
     */
    void setColorMode(int display, String iface, String format);

    /**
     * Returns a list of supported corlor modes
     */
    String[] getSupportCorlorList(int display, String iface);

    /**
     * Returns current color mode
     */
    String getCurColorMode(int display, String iface);

    /**
     * Set hdr mode
     */
    void setHdrMode(int display, String iface, int hdrMode);

    /**
     * Set display screen scale value
     */
    void setScreenScale(int display, int direction, int value);

    /**
     * Switch framebuffer
     */
    void setDisplaySize(int display, int width, int height);

    /**
     * Get Supported 3D Modes
     */
    int get3DModes(int display, String iface);

    /**
     * Get Current 3D Mode
     */
    int getCur3DMode(int display, String iface);

    /**
     * Set 3D Mode
     */
    void set3DMode(int display, String iface, int mode);
    /**
     * saveConfig
     */
    int saveConfig();

    /**
     * updateDisplayInfos
     */
    void updateDisplayInfos();

    /**
     * Set brightness
     */
    void setBrightness(int display, int brightness);
    /**
     * Set contrast
     */
    void setContrast(int display, int contrast);
    /**
     *Set saturation
     */
    void setSaturation(int display, int saturation);
    /**
      *Set Hue
    */
    void setHue(int display, int degree);
    /**
     *get Bcsh
    */
    int[] getBcsh(int display);
    /**
     *get Bcsh
    */
    int[] getOverscan(int display);
    /**
     *setGamma
    */
    int setGamma(int dpy,int size,in int[] red, in int[] green,in int[] blue);
    /**
     *set3DLut
    */
    int set3DLut(int dpy,int size,in int[] red, in int[] green,in int[] blue);
    /**
     *getConnectorInfo
    */
    String[] getConnectorInfo();
    /**
     *updateDispHeader
    */
    int updateDispHeader();

    /**
     * 返回该分辨率支持的各种模式，如HDR10、杜比视界等，通过位运算获取是否支持
     * resolution: String  分辨率
     * return: int 是否支持的状态相加
    */
    int getResolutionSupported(int display, String resolution);

    /**
     * 当前杜比视界状态
     * return: boolean true为打开杜比视界，false为关闭
    */
    boolean isDolbyVisionStatus();

    /**
     * 当前HDR10状态
     * return: boolean true为打开，false为关闭
    */
    boolean isHDR10Status();

    /**
     * 设置杜比视界状态
     * enabled: boolean 设置的状态
     * return: boolean 是否成功
    */
    boolean setDolbyVisionEnabled(boolean enabled);

    /**
     * 设置HDR10状态
     * enabled: boolean 设置的状态
     * return: boolean 是否成功
    */
    boolean setHDR10Enabled(boolean enabled);

    /**
     * 当前AI画质功能状态
     * return: boolean true为打开，false为关闭
    */
    boolean isAiImageQuality();

    /**
     * 设置AI画质状态
     * enabled: boolean 设置的状态
     * return: boolean 是否成功
    */
    boolean setAiImageQuality(boolean enabled);

    /**
     * 当前AI画质实验室功能状态
     * return: boolean true为打开，false为关闭
    */
    boolean isAiImageQualityLabMode();

    /**
     * 设置AI画质实验室功能状态
     * enabled: boolean 设置的状态
     * return: boolean 是否成功
    */
    boolean setAiImageQualityLabMode(boolean enabled);
}
