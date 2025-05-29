package com.android.tv.settings.display;

import android.R.integer;
import android.app.Activity;
import android.graphics.drawable.GradientDrawable.Orientation;
import android.hardware.display.DisplayManager;
import android.os.SystemProperties;
import android.util.Log;
import android.view.Display;
import android.view.Display.Mode;
import android.view.View;
import android.view.Window;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import com.android.tv.settings.util.ReflectUtils;

/**
 * Drm Display Setting.
 */

public class DrmDisplaySetting {

    private final static boolean DEBUG = true;

    private final static String TAG = "DrmDisplaySetting";

    private final static String SUB_TAG = "DrmDisplaySetting";

    public static final int DRM_MODE_CONNECTOR_Unknown = 0;
    public static final int DRM_MODE_CONNECTOR_VGA = 1;
    public static final int DRM_MODE_CONNECTOR_DVII = 2;
    public static final int DRM_MODE_CONNECTOR_DVID = 3;
    public static final int DRM_MODE_CONNECTOR_DVIA = 4;
    public static final int DRM_MODE_CONNECTOR_Composite = 5;
    public static final int DRM_MODE_CONNECTOR_SVIDEO = 6;
    public static final int DRM_MODE_CONNECTOR_LVDS = 7;
    public static final int DRM_MODE_CONNECTOR_Component = 8;
    public static final int DRM_MODE_CONNECTOR_9PinDIN = 9;
    public static final int DRM_MODE_CONNECTOR_DisplayPort = 10;
    public static final int DRM_MODE_CONNECTOR_HDMIA = 11;
    public static final int DRM_MODE_CONNECTOR_HDMIB = 12;
    public static final int DRM_MODE_CONNECTOR_TV = 13;
    public static final int DRM_MODE_CONNECTOR_eDP = 14;
    public static final int DRM_MODE_CONNECTOR_VIRTUAL = 15;
    public static final int DRM_MODE_CONNECTOR_DSI = 16;

    public static final String DISPLAY_TYPE_UNKNOWN = "UNKNOW";// Unknown
    public static final String DISPLAY_TYPE_VGA = "VGA";
    public static final String DISPLAY_TYPE_DVII = "DVII";
    public static final String DISPLAY_TYPE_DVID = "DVID";
    public static final String DISPLAY_TYPE_DVIA = "DVIA";
    public static final String DISPLAY_TYPE_Composite = "Composite";
    public static final String DISPLAY_TYPE_SVideo = "SVideo";
    public static final String DISPLAY_TYPE_LVDS = "LVDS";
    public static final String DISPLAY_TYPE_Component = "Component";
    public static final String DISPLAY_TYPE_9PinDIN = "9PinDIN";
    public static final String DISPLAY_TYPE_DP = "DP";
    public static final String DISPLAY_TYPE_HDMIA = "HDMIA";
    public static final String DISPLAY_TYPE_HDMIB = "HDMIB";
    public static final String DISPLAY_TYPE_TV = "TV";
    public static final String DISPLAY_TYPE_EDP = "EDP";
    public static final String DISPLAY_TYPE_VIRTUAL = "VIRTUAL";
    public static final String DISPLAY_TYPE_DSI = "DSI";
    public static Map<String, String> CONNECTOR_DISPLAY_NAME = new HashMap<String, String>() {
        {
            put("0", DISPLAY_TYPE_UNKNOWN);
            put("1", DISPLAY_TYPE_VGA);
            put("2", DISPLAY_TYPE_DVII);
            put("3", DISPLAY_TYPE_DVID);
            put("4", DISPLAY_TYPE_DVIA);
            put("5", DISPLAY_TYPE_Composite);
            put("6", DISPLAY_TYPE_SVideo);
            put("7", DISPLAY_TYPE_LVDS);
            put("8", DISPLAY_TYPE_Component);
            put("9", DISPLAY_TYPE_9PinDIN);
            put("10", DISPLAY_TYPE_DP);
            put("11", DISPLAY_TYPE_HDMIA);
            put("12", DISPLAY_TYPE_HDMIB);
            put("13", DISPLAY_TYPE_TV);
            put("14", DISPLAY_TYPE_EDP);
            put("15", DISPLAY_TYPE_VIRTUAL);
            put("16", DISPLAY_TYPE_DSI);
        }
    };

    public final static int DRM_MODE_CONNECTED = 1;
    public final static int DRM_MODE_DISCONNECTED = 2;
    public final static int DRM_MODE_UNKNOWNCONNECTION = 3;

    public final static int SDR = 0;
    public final static int HDR10 = 1;
    public final static int DOLBY_VISION = 2;
    public final static int HLG = 4;

    private static Object rkDisplayOutputManager = null;


    private static void logd(String text) {
        Log.d(TAG, SUB_TAG + " - " + text);
    }

    private static boolean initRkDisplayOutputManager() {
        if (rkDisplayOutputManager == null) {
            try {
                rkDisplayOutputManager = Class.forName("android.os.RkDisplayOutputManager").newInstance();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return rkDisplayOutputManager != null;
    }

    public static String[] getConnectorInfo() {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        return (String[]) ReflectUtils.invokeMethodNoParameter(rkDisplayOutputManager, "getConnectorInfo");
    }

    public static int getDisplayNumber() {
        if (!initRkDisplayOutputManager()) {
            return 0;
        }
        return (int) ReflectUtils.invokeMethodNoParameter(rkDisplayOutputManager, "getDisplayNumber");
    }

    public static List<DisplayInfo> getDisplayInfoList() {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        List<DisplayInfo> displayInfos = new ArrayList<DisplayInfo>();

        // 使用drm方式获取显示列表(hw_output通过drm接口获取)
        int mDisplayNumber = getDisplayNumber();
        logd("getDisplayInfoList->mDisplayNumber:" + mDisplayNumber);
        String[] mConnectorInfos = getConnectorInfo();
        for (String mConnectInfo : mConnectorInfos) {
            logd("getDisplayInfoList->mConnectInfo:" + mConnectInfo);
        }

        for(int i = 0; i < mDisplayNumber; i++) {
            String typeName = "";
            String id = "";
            String type = "";
            int state = DRM_MODE_UNKNOWNCONNECTION;
            if(null != mConnectorInfos && mConnectorInfos.length == mDisplayNumber) {
                String[] rets = mConnectorInfos[i].split(",");
                if(rets != null && rets.length > 2) {
                    type = rets[0].replaceAll("type:", "");
                    typeName = CONNECTOR_DISPLAY_NAME.get(type);
                    id = rets[1].replaceAll("id:", "");
                    try {
                        state = Integer.parseInt(rets[2].replaceAll("state:", ""));
                    } catch (Exception e) {
                        //TODO: handle exception
                        e.printStackTrace();
                    }
                    logd("getDisplayInfoList->typeName:" + typeName + " id:" + id + " state:" + state);
                    if(state != DRM_MODE_CONNECTED
                        || DISPLAY_TYPE_VIRTUAL.equals(typeName)
                        || DISPLAY_TYPE_DSI.equals(typeName)) {
                        continue;
                    }
                }
            }
            DisplayInfo mDisplayInfo = new DisplayInfo();
            mDisplayInfo.setDisplayId(i);
            int mCurrentType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{ int.class }, new Object[]{ mDisplayInfo.getDisplayId() });
            typeName = (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "typetoface", new Class[]{int.class}, new Object[]{mCurrentType});
            mDisplayInfo.setType(mCurrentType);
            if("0".equals(id) || "".equals(id)) {
                mDisplayInfo.setDescription(typeName);
            } else {
                mDisplayInfo.setDescription(typeName + "-" + id);
            }

            String[] originModes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getModeList", new Class[] { int.class, int.class }, new Object[] { mDisplayInfo.getDisplayId(), mDisplayInfo.getType() });
            originModes = filterOrginModes(originModes);
            mDisplayInfo.setOrginModes(originModes);
            mDisplayInfo.setModes(getFilterModeList(originModes));
            String[] colors = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getSupportCorlorList",
                    new Class[] { int.class, int.class }, new Object[] { mDisplayInfo.getDisplayId(),
                            mDisplayInfo.getType() });
            mDisplayInfo.setColors(colors);
            logd("getDisplayInfoList->mDisplayInfo:" + mDisplayInfo.toString());
            displayInfos.add(mDisplayInfo);
        }

        return displayInfos;
    }

    public static List<String> getDisplayModes(DisplayInfo di) {
        List<String> res = new ArrayList<String>();
        if (di != null) {
            String[] modes = di.getOrginModes();
            if (modes != null && modes.length != 0) {
                res = Arrays.asList(modes);
            }
        }
        return res;
    }

    public static String getCurDisplayMode(DisplayInfo di) {
        if (!initRkDisplayOutputManager()) {
            Log.d(TAG, "Display Manager is null!!");
            return null;
        }
        String curMode = null;
        if (di != null) {
            int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager,
                "getCurrentInterface", new Class[] { int.class }, new Object[] { di.getDisplayId() });
            curMode = (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentMode",
                    new Class[] { int.class, int.class }, new Object[] { di.getDisplayId(), currMainType });
        }
        logd("getCurDisplayMode - curMode - " + curMode);
        return curMode;
    }

    public static List<String> getColorModeList(DisplayInfo mDisplay) {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        List<String> res = new ArrayList<String>();
        int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface",
                new Class[] { int.class }, new Object[] { mDisplay.getDisplayId() });
        String[] modes = (String[]) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getSupportCorlorList",
                new Class[] { int.class, int.class }, new Object[] { mDisplay.getDisplayId(), currMainType });
        if (modes != null && modes.length != 0) {
            res = Arrays.asList(modes);
        }
        return res;
    }
    public static void setColorMode(int displayid, int type, String format) {
        if (!initRkDisplayOutputManager()) {
            return;
        }
        logd("setColorMode displayid = " + displayid + ", type = " + type + ", format = " + format);

        ReflectUtils.invokeMethod(rkDisplayOutputManager, "setColorMode", new Class[]{int.class, int.class, String.class}, new Object[]{ displayid, type, format });
        saveConfig();
    }

    public static String getColorMode(DisplayInfo mDisplayInfo) {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface", new Class[]{int.class}, new Object[]{ mDisplayInfo.getDisplayId() });
        return (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentColorMode", new Class[]{int.class, int.class}, new Object[]{ mDisplayInfo.getDisplayId(), currMainType });
    }

    public static void setDisplayModeTemp(DisplayInfo di, int index) {
        String mode = getDisplayModes(di).get(index);
        curSetDisplayMode = getCurDisplayMode(di);
        logd("setDisplayModeTemp curSetHdmiMode = " + curSetDisplayMode);
        setDisplayMode(di, mode);
        tmpSetDisplayMode = mode;
    }

    public static void setDisplayModeTemp(DisplayInfo di, String mode) {
        curSetDisplayMode = getCurDisplayMode(di);
        logd("setDisplayModeTemp curSetHdmiMode = "+curSetDisplayMode);
        setDisplayMode(di, mode);
        tmpSetDisplayMode = mode;
    }

    public static void confirmSaveDisplayMode(DisplayInfo di, boolean isSave) {
        if (di == null) {
            return;
        }
        confirmSaveHdmiMode(di, isSave);
    }

    public static String tmpSetDisplayMode = null;
    public static String curSetDisplayMode = null;

    private static void confirmSaveHdmiMode(DisplayInfo mDisplayInfo, boolean isSave) {
        logd(" confirmSaveHdmiMode save = "+isSave);
        if (tmpSetDisplayMode == null) {
            return;
        }
        if (isSave) {
            curSetDisplayMode = tmpSetDisplayMode;
            saveConfig();
        } else {
            setDisplayMode(mDisplayInfo, curSetDisplayMode);
            tmpSetDisplayMode = null;
        }
    }

    private static void setDisplayMode(DisplayInfo mDisplayInfo, String mode) {
        if (!initRkDisplayOutputManager()) {
            return;
        }
        logd(" setHdmiMode mode = " + mode);
        int currMainType = (Integer) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getCurrentInterface",
            new Class[]{int.class}, new Object[]{ mDisplayInfo.getDisplayId() });
        ReflectUtils.invokeMethod(rkDisplayOutputManager, "setMode", new Class[]{int.class, int.class, String.class},
            new Object[]{ mDisplayInfo.getDisplayId(), currMainType, mode});
    }

    private static List<String> processModeStr(List<String> resoStrList) {
        if (resoStrList == null) {
            return null;
        }
        List<String> processedResoStrList = new ArrayList<>();
        List<String> tmpResoStrList = new ArrayList<>();
        for (String reso : resoStrList) {
            if (reso.contains("p") || reso.contains("i")) {
                boolean hasRepeat = false;
                for (String s : tmpResoStrList) {
                    if (s.equals(reso)) {
                        hasRepeat = true;
                        break;
                    }
                }
                if (!hasRepeat) {
                    tmpResoStrList.add(reso);
                }
            }
        }
        return tmpResoStrList;
    }

    private static List<String> readStrListFromFile(String pathname) throws IOException {
        List<String> fileStrings = new ArrayList<>();
        File filename = new File(pathname);
        InputStreamReader reader = new InputStreamReader(new FileInputStream(filename));
        BufferedReader br = new BufferedReader(reader);
        String line;
        while ((line = br.readLine()) != null) {
            fileStrings.add(line);
        }
        logd("readStrListFromFile - " + fileStrings.toString());
        return fileStrings;
    }

    private static String readStrFromFile(String filename) throws IOException {
        logd("readStrFromFile - " + filename);
        File f = new File(filename);
        InputStreamReader reader = new InputStreamReader(new FileInputStream(f));
        BufferedReader br = new BufferedReader(reader);
        String line = br.readLine();
        logd("readStrFromFile - " + line);
        return line;
    }

    private static String[] filterOrginModes(String[] modes) {
        if (modes == null)
            return null;
        List<String> filterModeList = new ArrayList<String>();
        List<String> resModeList = new ArrayList<String>();
        for (int i = 0; i < modes.length; ++i) {
            logd("filterOrginModes->mode:" + modes[i]);
            String itemMode = modes[i];
            int endIndex = itemMode.indexOf("-");
            if (endIndex > 0)
                itemMode = itemMode.substring(0, endIndex);
            if (!resModeList.contains(itemMode)) {
                resModeList.add(itemMode);
                if (!filterModeList.contains(modes[i]))
                    filterModeList.add(modes[i]);
            }
        }
        return filterModeList.toArray(new String[0]);
    }

    private static String[] getFilterModeList(String[] modes) {
        if (modes == null)
            return null;
        String[] filterModes = new String[modes.length];
        for (int i = 0; i < modes.length; ++i) {
            String itemMode = modes[i];
            int endIndex = itemMode.indexOf("-");
            if (endIndex > 0)
                itemMode = itemMode.substring(0, endIndex);
            filterModes[i] = itemMode;
        }
        return filterModes;
    }

    public static int saveConfig() {
        if (!initRkDisplayOutputManager()) {
            return -1;
        }

        int res = (int) ReflectUtils.invokeMethod(rkDisplayOutputManager, "saveConfig", new Class[]{}, new Object[]{});
        return res;
    }

    public static void updateDisplayInfos() {
        if (!initRkDisplayOutputManager()) {
            return;
        }
        int res = (int) ReflectUtils.invokeMethod(rkDisplayOutputManager, "updateDisplayInfos", new Class[]{}, new Object[]{});
        logd("updateDisplayInfos res = " + res);
    }

    public static String[] getAndroidModes(DisplayManager mDisplayManager) {
        List<String> modes = new ArrayList();
        Display mDisplay = mDisplayManager.getDisplays()[0];
        if (mDisplay != null) {
            Mode[] androidModes = mDisplay.getSupportedModes();
            for (Mode mode : androidModes) {
                modes.add(String.format(Locale.getDefault(), "%dx%dp%.2f", mode.getPhysicalWidth(),
                        mode.getPhysicalHeight(), mode.getRefreshRate()));
            }
        }
        return modes.toArray(new String[modes.size()]);
    }

    public static String[] getAndroidModesIndex(DisplayManager mDisplayManager) {
        List<String> indexs = new ArrayList();
        Display mDisplay = mDisplayManager.getDisplays()[0];
        if (mDisplay != null) {
            Mode[] androidModes = mDisplay.getSupportedModes();
            for (Mode mode : androidModes) {
                indexs.add(String.valueOf(mode.getModeId()));
            }
        }
        return indexs.toArray(new String[indexs.size()]);
    }

    public static String getCurrentAndroidMode(DisplayManager mDisplayManager) {
        String modeStr = "";
        Display mDisplay = mDisplayManager.getDisplays()[0];
        if (mDisplay != null) {
            Mode mode = mDisplay.getMode();
            modeStr = String.format(Locale.getDefault(), "%dx%dp%.2f", mode.getPhysicalWidth(),
                    mode.getPhysicalHeight(), mode.getRefreshRate());
        }
        return modeStr;
    }

    public static int getHdrResolutionSupport(int displayId, String mode) {
        if (!initRkDisplayOutputManager()) {
            return 0;
        }

        int ret = (int) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getResolutionSupported",
                new Class[] { int.class, String.class }, new Object[] { displayId, mode });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

    public static boolean isHDR10Status() {
        if (!initRkDisplayOutputManager()) {
            return false;
        }

        boolean ret = (boolean) ReflectUtils.invokeMethod(rkDisplayOutputManager, "isHDR10Status",
                new Class[] { }, new Object[] { });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

    public static boolean setHDR10Enabled(boolean enable) {
        if (!initRkDisplayOutputManager()) {
            return false;
        }

        boolean ret = (boolean) ReflectUtils.invokeMethod(rkDisplayOutputManager, "setHDR10Enabled",
                new Class[] { boolean.class }, new Object[] { enable });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

    public static boolean setHDRVividEnabled(String mode) {
        if (!initRkDisplayOutputManager()) {
            return false;
        }
        boolean ret = (boolean) ReflectUtils.invokeMethod(rkDisplayOutputManager, "setHDRVividEnabled",
                new Class[] { String.class }, new Object[] { mode });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

    public static String getHDRVividStatus() {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        String ret = (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getHDRVividStatus",
                new Class[] { }, new Object[] { });
        return ret;
    }

    public static String getHDRVividCurrentBrightness() {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        String ret = (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getHDRVividCurrentBrightness",
                new Class[] { }, new Object[] { });
        return ret;
    }

    public static boolean setHDRVividMaxBrightness(String selectBrightness) {
        if (!initRkDisplayOutputManager()) {
            return false;
        }
        boolean ret = (boolean) ReflectUtils.invokeMethod(rkDisplayOutputManager, "setHDRVividMaxBrightness",
                new Class[] { String.class }, new Object[] { selectBrightness });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

    public static String getHDRVividCapacity() {
        if (!initRkDisplayOutputManager()) {
            return null;
        }
        String ret = (String) ReflectUtils.invokeMethod(rkDisplayOutputManager, "getHDRVividCapacity",
                new Class[] { }, new Object[] { });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

    public static boolean setHDRVividCapacity(String capacity) {
        if (!initRkDisplayOutputManager()) {
            return false;
        }
        boolean ret = (boolean) ReflectUtils.invokeMethod(rkDisplayOutputManager, "setHDRVividCapacity",
                new Class[] { String.class }, new Object[] { capacity });
        logd("getHdrResolutionSupport->res:" + ret);
        return ret;
    }

}
