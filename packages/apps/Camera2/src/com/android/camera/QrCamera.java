/*
 * Copyright (C) 2022 The Android Open Source Project
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

package com.android.camera;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.ArrayMap;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.TextureView;
import android.view.WindowManager;
import android.widget.Toast;
import com.android.camera2.R;
import com.google.zxing.BarcodeFormat;
import com.google.zxing.BinaryBitmap;
import com.google.zxing.DecodeHintType;
import com.google.zxing.MultiFormatReader;
import com.google.zxing.ReaderException;
import com.google.zxing.Result;
import com.google.zxing.common.HybridBinarizer;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

public class QrCamera extends Handler {
    private static final String TAG = "QrCamera";

    private final int maxPreviewSize = 1280 * 720;

    private static Map<DecodeHintType, List<BarcodeFormat>> HINTS = new ArrayMap<>();
    private static List<BarcodeFormat> FORMATS = new ArrayList<>();

    static {
        FORMATS.add(BarcodeFormat.QR_CODE);
        FORMATS.add(BarcodeFormat.UPC_A);
        FORMATS.add(BarcodeFormat.UPC_E);
        FORMATS.add(BarcodeFormat.EAN_13);
        FORMATS.add(BarcodeFormat.EAN_8);
        FORMATS.add(BarcodeFormat.CODE_39);
        FORMATS.add(BarcodeFormat.CODE_128);
        FORMATS.add(BarcodeFormat.ITF);
        HINTS.put(DecodeHintType.POSSIBLE_FORMATS, FORMATS);
    }

    private CameraManager mCameraManager;
    private CameraDevice mCameraDevice;
    private CameraCaptureSession mCameraCaptureSessions;
    private CaptureRequest.Builder mCaptureRequestBuilder;
    private Size mPreviewSize;
    private ImageReader mImageReader;
    private Handler mBackgroundHandler;
    private HandlerThread mBackgroundThread;
    private Context mContext;
    private ScannerCallback mScannerCallback;
    private MultiFormatReader mReader;
    private QrYuvLuminanceSource mImage;
    private SurfaceTexture mSurface;
    private int mCameraOrientation;
    private TextureView mTextureView;

    public QrCamera(Context context, ScannerCallback callback) {
        mContext = context;
        mCameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        mScannerCallback = callback;
        mReader = new MultiFormatReader();
        mReader.setHints(HINTS);
    }

    private CameraDevice.StateCallback mCameraStateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice camera) {
            Log.d(TAG, "Camera device opened");
            mCameraDevice = camera;
            createCameraPreview();
        }

        @Override
        public void onDisconnected( CameraDevice camera) {
            Log.d(TAG, "Camera device disconnected");
            mCameraDevice.close();
            mCameraDevice = null;
        }

        @Override
        public void onError( CameraDevice camera, int error) {
            Log.e(TAG, "Camera device error: " + error);
            mCameraDevice.close();
            mCameraDevice = null;
        }
    };

    protected void startBackgroundThread() {
        if (mBackgroundThread == null) {
            mBackgroundThread = new HandlerThread("Camera Background");
            mBackgroundThread.start();
        }
        if (mBackgroundHandler == null) {
            mBackgroundHandler = new Handler(mBackgroundThread.getLooper());
        }
    }

    protected void stopBackgroundThread() {
        if (mBackgroundThread != null) {
            mBackgroundThread.quitSafely();
            try {
                mBackgroundThread.join();
                mBackgroundThread = null;
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
        if (mBackgroundHandler != null) {
            mBackgroundHandler = null;
        }
    }

    /**
     * The function start camera preview and capture pictures to decode QR code continuously in a
     * background task.
     *
     * @param surface The surface to be used for live preview.
     */
    public void start(TextureView textureView) {
        initCamera(textureView);
    }

    private boolean initCamera(TextureView textureView) {
        mTextureView = textureView;
        String frontCameraId = null;
        String backCameraId = null;
        String extCameraId = null;
        if (mCameraManager == null) {
            return false;
        }

        try {
            for (String cameraId : mCameraManager.getCameraIdList()) {
                CameraCharacteristics characteristics = mCameraManager.getCameraCharacteristics(cameraId);
                Integer facing = characteristics.get(CameraCharacteristics.LENS_FACING);
                if (facing == CameraCharacteristics.LENS_FACING_FRONT) {
                    frontCameraId = cameraId;
                } else if (facing == CameraCharacteristics.LENS_FACING_BACK) {
                    backCameraId = cameraId;
                } else if (facing == CameraCharacteristics.LENS_FACING_EXTERNAL) {
                    extCameraId = cameraId;
                }
            }
            if (null != backCameraId && !backCameraId.isEmpty()) {
                CameraCharacteristics characteristics = mCameraManager.getCameraCharacteristics(backCameraId);
                StreamConfigurationMap configMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                Size[] supportedSizes = configMap.getOutputSizes(android.graphics.ImageFormat.YUV_420_888);
                mPreviewSize = getBestPreviewSize(supportedSizes);
                mCameraManager.openCamera(backCameraId, mCameraStateCallback, mBackgroundHandler);
            } else if (null != frontCameraId && !frontCameraId.isEmpty()) {
                CameraCharacteristics characteristics = mCameraManager.getCameraCharacteristics(backCameraId);
                StreamConfigurationMap configMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                Size[] supportedSizes = configMap.getOutputSizes(android.graphics.ImageFormat.YUV_420_888);
                mPreviewSize = getBestPreviewSize(supportedSizes);
                mCameraManager.openCamera(frontCameraId, mCameraStateCallback, mBackgroundHandler);
            } else if (null != extCameraId && !extCameraId.isEmpty()) {
                CameraCharacteristics characteristics = mCameraManager.getCameraCharacteristics(backCameraId);
                StreamConfigurationMap configMap = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                Size[] supportedSizes = configMap.getOutputSizes(android.graphics.ImageFormat.YUV_420_888);
                mPreviewSize = getBestPreviewSize(supportedSizes);
                mCameraManager.openCamera(extCameraId, mCameraStateCallback, mBackgroundHandler);
            } else {
                Log.e(TAG, "No back Camera.");
                Toast.makeText(mContext, mContext.getString(R.string.toast_no_back_camera), Toast.LENGTH_SHORT).show();
                return false;
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
            mScannerCallback.handleCameraFailure();
        }
        return true;
    }

    /**
     * The function stop camera preview and background decode task. Caller call this function when
     * the surface is being destroyed.
     */
    public void stop() {
        Log.d(TAG, "stop");
        closeCamera();
        stopBackgroundThread();
    }

    private void closeCamera() {
        if (mCameraDevice != null) {
            mCameraDevice.close();
            mCameraDevice = null;
        }
        if (mImageReader != null) {
            mImageReader.close();
            mImageReader = null;
        }
    }

    /** The scanner which includes this QrCodeCamera class should implement this */
    public interface ScannerCallback {

        /**
         * The function used to handle the decoding result of the QR code.
         *
         * @param result the result QR code after decoding.
         */
        void handleSuccessfulResult(String result);

        /** Request the QR code scanner to handle the failure happened. */
        void handleCameraFailure();

        /**
         * The function used to get the background View size.
         *
         * @return Includes the background view size.
         */
        Size getViewSize();

        /**
         * The function used to get the frame position inside the view
         *
         * @param previewSize       Is the preview size set by camera
         * @param cameraOrientation Is the orientation of current Camera
         * @return The rectangle would like to crop from the camera preview shot.
         */
        Rect getFramePosition(Size previewSize, int cameraOrientation);

        /**
         * Sets the transform to associate with preview area.
         *
         * @param transform The transform to apply to the content of preview
         */
        void setTransform(Matrix transform);

        /**
         * Verify QR code is valid or not. The camera will stop scanning if this callback returns
         * true.
         *
         * @param qrCode The result QR code after decoding.
         * @return Returns true if qrCode hold valid information.
         */
        boolean isValid(String qrCode);
    }

    private boolean createCameraPreview() {
        if (mContext == null) {
            return false;
        }
        Size viewSize = mScannerCallback.getViewSize();
        setTransformationMatrix(viewSize.getWidth(), viewSize.getHeight());
        try {
            mCaptureRequestBuilder = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            mImageReader = ImageReader.newInstance(mPreviewSize.getWidth(), mPreviewSize.getHeight(),
                    ImageFormat.YUV_420_888, 2);
            mImageReader.setOnImageAvailableListener(mReaderListener, mBackgroundHandler);
            Surface textureSurface = new Surface(mTextureView.getSurfaceTexture());
            Surface imageSurface = mImageReader.getSurface();
            mCaptureRequestBuilder.addTarget(textureSurface);
            mCaptureRequestBuilder.addTarget(imageSurface);
            mCameraDevice.createCaptureSession(Arrays.asList(textureSurface, imageSurface),
                    new CameraCaptureSession.StateCallback() {
                        @Override
                        public void onConfigured(CameraCaptureSession session) {
                            if (mCameraDevice == null) {
                                return;
                            }
                            mCameraCaptureSessions = session;
                            updatePreview();
                        }
                        @Override
                        public void onConfigureFailed(CameraCaptureSession session) {
                            Toast.makeText(mContext, mContext.getString(R.string.toast_config_camera_failed), Toast.LENGTH_SHORT).show();
                        }
                    }, null);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
        return true;
    }

    private void setTransformationMatrix(int viewWidth, int viewHeight) {
        final WindowManager winManager =
                (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        final int rotation = winManager.getDefaultDisplay().getRotation();
        int degrees = 0;
        switch (rotation) {
            case Surface.ROTATION_0:
                degrees = 0;
                break;
            case Surface.ROTATION_90:
                degrees = 90;
                break;
            case Surface.ROTATION_180:
                degrees = 180;
                break;
            case Surface.ROTATION_270:
                degrees = 270;
                break;
        }
        mCameraOrientation = (360 - degrees) % 360;
        final boolean isPortrait = mContext.getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_PORTRAIT;

        final int previewWidth = isPortrait ? mPreviewSize.getWidth() : mPreviewSize.getHeight();
        final int previewHeight = isPortrait ? mPreviewSize.getHeight() : mPreviewSize.getWidth();
        final float ratioPreview = (float) getRatio(previewWidth, previewHeight);

        // Calculate transformation matrix.
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        if (previewWidth > previewHeight) {
            scaleY = scaleX / ratioPreview;
        } else {
            scaleX = scaleY / ratioPreview;
        }
        Matrix transformMatrix = new Matrix();
        RectF viewRect = new RectF(0, 0, viewWidth, viewHeight);
        RectF bufferRect = new RectF(0, 0, mPreviewSize.getHeight(), mPreviewSize.getWidth());
        float centerX = viewRect.centerX();
        float centerY = viewRect.centerY();
        if (mCameraOrientation % 180 != 0) {
            bufferRect.offset(centerX - bufferRect.centerX(), centerY - bufferRect.centerY());
            transformMatrix.setRectToRect(viewRect, bufferRect, Matrix.ScaleToFit.FILL);
            float scale = Math.max(
                    (float) viewHeight / mPreviewSize.getHeight(),
                    (float) viewWidth / mPreviewSize.getWidth());
            transformMatrix.postRotate(mCameraOrientation, centerX, centerY);
            transformMatrix.postScale(scaleX, scaleY, centerX, centerY);
        } else {
            transformMatrix.postRotate(mCameraOrientation, centerX, centerY);
        }
        mScannerCallback.setTransform(transformMatrix);
    }

    private void updatePreview() {
        if (mCameraDevice == null) {
            return;
        }
        mCaptureRequestBuilder.set(CaptureRequest.CONTROL_MODE, CaptureRequest.CONTROL_MODE_AUTO);
        try {
            mCameraCaptureSessions.setRepeatingRequest(mCaptureRequestBuilder.build(), null, mBackgroundHandler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    private final ImageReader.OnImageAvailableListener mReaderListener = new ImageReader.OnImageAvailableListener() {
        @Override
        public void onImageAvailable(ImageReader reader) {
            Image image = null;
            try {
                image = reader.acquireLatestImage();
                if (image != null) {
                    ByteBuffer buffer = image.getPlanes()[0].getBuffer();
                    byte[] data = new byte[buffer.remaining()];
                    buffer.get(data);
                    mImage = getFrameImage(data);
                    Result qrCode = null;
                    try {
                        qrCode = mReader.decodeWithState(
                                        new BinaryBitmap(new HybridBinarizer(mImage)));
                    } catch (ReaderException e) {
                        // No logging since every time the reader cannot decode the
                        // image, this ReaderException will be thrown.
                        e.printStackTrace();
                    } finally {
                        mReader.reset();
                        if (image != null) {
                            image.close();
                        }
                    }
                    if (qrCode != null) {
                        if (mScannerCallback.isValid(qrCode.getText())) {
                            mScannerCallback.handleSuccessfulResult(qrCode.getText());
                            image.close();
                            return;
                        }
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                if (image != null) {
                    image.close();
                }
            }
        }
    };

    private QrYuvLuminanceSource getFrameImage(byte[] imageData) {
        byte[] rotatedData = new byte[imageData.length];
        int width = mPreviewSize.getWidth();
        int height = mPreviewSize.getHeight();
        if (mCameraOrientation % 180 != 0) {
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    rotatedData[x * height + height - y - 1] = imageData[x + y * width];
                }
            }
            int tmp = width;
            width = height;
            height = tmp;
        } else {
            rotatedData = imageData;
        }
        final Rect frame = mScannerCallback.getFramePosition(mPreviewSize, mCameraOrientation);
        final QrYuvLuminanceSource image = new QrYuvLuminanceSource(rotatedData,
                width, height);
        return image;
    }

    /**
     * Get best preview size from the list of camera supported preview sizes. Compares the
     * preview size and aspect ratio to choose the best one.
     */
    private Size getBestPreviewSize(Size[] supportedSizes) {
        final double minRatioDiffPercent = 0.1;
        final Size windowSize = mScannerCallback.getViewSize();
        final double winRatio = getRatio(windowSize.getWidth(), windowSize.getHeight());
        double bestChoiceRatio = 0;
        Size bestChoice = new Size(0, 0);
        for (Size size : supportedSizes) {
            if (size.getWidth() * size.getHeight() <= maxPreviewSize) {
                double ratio = getRatio(size.getWidth(), size.getHeight());
                if (size.getHeight() * size.getWidth() > bestChoice.getWidth() * bestChoice.getHeight()
                        && (Math.abs(bestChoiceRatio - winRatio) / winRatio > minRatioDiffPercent
                        || Math.abs(ratio - winRatio) / winRatio <= minRatioDiffPercent)) {
                    bestChoice = new Size(size.getWidth(), size.getHeight());
                    bestChoiceRatio = getRatio(size.getWidth(), size.getHeight());
                }
            }
        }
        Log.d(TAG, "bestChoice:" + bestChoice);
        return bestChoice;
    }

    private double getRatio(double x, double y) {
        return (x < y) ? x / y : y / x;
    }

    private int getOrientation() {
        return mCameraOrientation;
    }
}
