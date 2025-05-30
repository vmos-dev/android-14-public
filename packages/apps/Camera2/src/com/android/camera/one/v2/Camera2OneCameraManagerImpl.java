/*
 * Copyright (C) 2015 The Android Open Source Project
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


package com.android.camera.one.v2;

import android.annotation.TargetApi;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.os.Build.VERSION_CODES;
import android.os.Handler;

import com.android.camera.debug.Log;
import com.android.camera.debug.Log.Tag;
import com.android.camera.device.CameraId;
import com.android.camera.one.OneCamera.Facing;
import com.android.camera.one.OneCameraAccessException;
import com.android.camera.one.OneCameraCharacteristics;
import com.android.camera.one.OneCameraManager;
import com.android.camera.util.AndroidServices;
import com.android.camera.util.ApiHelper;
import com.google.common.base.Optional;

import java.util.Hashtable;

import javax.annotation.Nonnull;

/**
 * Pick camera ids from a list of devices based on defined characteristics.
 */
@TargetApi(VERSION_CODES.LOLLIPOP)
public class Camera2OneCameraManagerImpl extends CameraManager.AvailabilityCallback
        implements OneCameraManager {
    private static final Tag TAG = new Tag("Camera2OneCamMgr");
    /**
     * Create a new camera2 api hardware manager.
     */
    public static Optional<Camera2OneCameraManagerImpl> create() {
        if (!ApiHelper.HAS_CAMERA_2_API) {
            return Optional.absent();
        }
        CameraManager cameraManager;
        try {
            cameraManager = AndroidServices.instance().provideCameraManager();
        } catch (IllegalStateException ex) {
            Log.e(TAG, "camera2.CameraManager is not available.");
            return Optional.absent();
        }
        Camera2OneCameraManagerImpl hardwareManager =
              new Camera2OneCameraManagerImpl(cameraManager);
        return Optional.of(hardwareManager);
    }

    private final CameraManager mCameraManager;
    private Hashtable<Facing, String> mCameraFacingCache = new Hashtable<Facing, String>();
    private AvailabilityCallback mAvailabilityCallback;

    public Camera2OneCameraManagerImpl(CameraManager cameraManger) {
        mCameraManager = cameraManger;

        //Camera facing queries depending on camera implementation can be
        //expensive and involve additional IPC with side effects. Cache front&
        //back camera ids as early as possible.
        if (mCameraManager != null) {
            mCameraFacingCache.clear();
            findFirstCameraFacing(Facing.BACK);
            findFirstCameraFacing(Facing.FRONT);
        }
    }

    @Override
    public boolean hasCamera() {
        try {
            String[] ids = mCameraManager.getCameraIdList();
            return ids != null && ids.length > 0;
        } catch (CameraAccessException ex) {
            Log.e(TAG, "Unable to read camera list.", ex);
            return false;
        }
    }

    @Override
    public boolean hasCameraFacing(@Nonnull Facing direction) {
        return findCameraId(direction) != null;
    }

    @Override
    public CameraId findFirstCamera() {
        try {
            String[] ids = mCameraManager.getCameraIdList();
            if(ids != null && ids.length > 0) {
                return CameraId.from(ids[0]);
            }
        } catch (CameraAccessException ex) {
            Log.e(TAG, "Unable to read camera list.", ex);
        }

        return null;
    }

    @Override
    public CameraId findFirstCameraFacing(@Nonnull Facing facing) {
        String cameraId = findCameraId(facing);
        return (cameraId != null) ? CameraId.from(cameraId) : null;
    }

    @Override
    public OneCameraCharacteristics getOneCameraCharacteristics(
          @Nonnull CameraId key)
          throws OneCameraAccessException {
        return new OneCameraCharacteristicsImpl(getCameraCharacteristics(key));
    }

    @Override
    public void setAvailabilityCallback(AvailabilityCallback callback, Handler handler) {
        mAvailabilityCallback = callback;
        mCameraManager.registerAvailabilityCallback(this, handler);
    }

    @Override
    public void clearAvailabilityCallback(AvailabilityCallback callback) {
        mAvailabilityCallback = callback;
        mCameraManager.unregisterAvailabilityCallback(this);
    }

    public CameraCharacteristics getCameraCharacteristics(
          @Nonnull CameraId key)
          throws OneCameraAccessException {
        try {
            return mCameraManager.getCameraCharacteristics(key.getValue());
        } catch (CameraAccessException ex) {
            throw new OneCameraAccessException("Unable to get camera characteristics", ex);
        }
    }

    @Override
    public void onCameraAccessPrioritiesChanged() {
        if (mAvailabilityCallback != null) {
            mAvailabilityCallback.onCameraAccessPrioritiesChanged();
        }
    }

    /** Returns the ID of the first camera facing the given direction. */
    private String findCameraId(Facing facing) {
        String id = mCameraFacingCache.get(facing);
        if (id != null) {
            return id;
        }

        if (facing == Facing.FRONT) {
            id = findFirstFrontCameraId();
        } else {
            id = findFirstBackCameraId();
        }

        if (id != null) {
            mCameraFacingCache.put(facing, id);
        }
        return id;
    }

    /** Returns the ID of the first back-facing camera. */
    private String findFirstBackCameraId() {
        Log.d(TAG, "Getting First BACK Camera");
        String cameraId = findFirstCameraIdFacing(CameraCharacteristics.LENS_FACING_BACK);
        if (cameraId == null) {
            Log.w(TAG, "No back-facing camera found.");
            Log.d(TAG, "Getting First FRONT Camera");
            //xcq add
            cameraId = findFirstCameraIdFacing(CameraCharacteristics.LENS_FACING_FRONT);
        }
        return cameraId;
    }

    /** Returns the ID of the first front-facing camera. */
    private String findFirstFrontCameraId() {
        Log.d(TAG, "Getting First FRONT Camera");
        String cameraId = findFirstCameraIdFacing(CameraCharacteristics.LENS_FACING_FRONT);
        if (cameraId == null) {
            Log.w(TAG, "No front-facing camera found,try to find external facing camera.");
            cameraId = findFirstCameraIdFacing(CameraCharacteristics.LENS_FACING_EXTERNAL);
            if (cameraId == null) {
                Log.w(TAG, "No external camera found.");
            }
        }
        return cameraId;
    }


    /** Returns the ID of the first camera facing the given direction. */
    private String findFirstCameraIdFacing(int facing) {
        try {
            String[] cameraIds = mCameraManager.getCameraIdList();
            for (String cameraId : cameraIds) {
                CameraCharacteristics characteristics = mCameraManager
                      .getCameraCharacteristics(cameraId);
                if (characteristics.get(CameraCharacteristics.LENS_FACING) == facing) {
                    return cameraId;
                }
            }
        } catch (CameraAccessException ex) {
            Log.w(TAG, "Unable to get camera ID", ex);
        }
        return null;
    }

}
