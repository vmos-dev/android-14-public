package com.rockchip.aroundcamera;

import android.graphics.Bitmap;
import android.hardware.HardwareBuffer;

public class AroundCameraJNI {
    private static final String TAG = "AroundCameraJNI";

    //JNI
    public native int native_init();

    public native int native_clear();

    public native void nativeSetSurroundType(int type);

    public native void nativeSetBitmap(Bitmap bitmap,long elapsedRealtime);

    public native void native_onScreenTouch(int x,int y,int movex,int movey);

    public native int native_process(HardwareBuffer[] hardwareBuffer);

    public native int[] native_getCameraList();

    public AroundCameraJNI() {
        getClassLoader();
        System.loadLibrary("rkaroundcamera");
    }

    public static ClassLoader getClassLoader() {
        return AroundCameraJNI.class.getClassLoader();
    }

    public int init() {
        return native_init();
    }

    public int process(HardwareBuffer[] hardwareBuffer) {
        return native_process(hardwareBuffer);
    }

    public void onScreenTouch (int x,int y,int movex,int movey){
        native_onScreenTouch(x,y,movex,movey);
    }

    public int[] getCameraList() {
        return native_getCameraList();
    }

    public int clear() {
        return native_clear();
    }
}
