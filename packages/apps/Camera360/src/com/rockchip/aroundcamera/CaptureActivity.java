package com.rockchip.aroundcamera;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.HardwareBuffer;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.support.annotation.NonNull;
import android.util.Log;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.Random;

public class CaptureActivity extends Activity {
    private final static String TAG = "RK_CaptureActivity";

    private final static int CAMERA_NUM = 4;

    private SurfaceView surfaceView0;
    private SurfaceView surfaceView1;
    private SurfaceView surfaceView2;
    private SurfaceView surfaceView3;

    private TestArrayAdapter[] adapter = new TestArrayAdapter[CAMERA_NUM];

    private Spinner mPictureListSpinner0;
    private Spinner mPictureListSpinner1;
    private Spinner mPictureListSpinner2;
    private Spinner mPictureListSpinner3;

    private Button mCapturebtn0;
    private Button mCapturebtn1;
    private Button mCapturebtn2;
    private Button mCapturebtn3;

    private Button mReturnMain;

    private CameraRunThread[] mCameraRunThread = new CameraRunThread[CAMERA_NUM];
    private boolean[] mCameraRunning = new boolean[CAMERA_NUM];

    private Size[][] mPictureSizes = new Size[CAMERA_NUM][];
    private Size[][] mPreviewSize = new Size[CAMERA_NUM][];
    private int[] mCurPicIndex = new int[CAMERA_NUM];

    private Random[] mRandom = new Random[CAMERA_NUM];

    private int[] mFrameCount = new int[CAMERA_NUM];
    private int[] mFrameGetCount = new int[CAMERA_NUM];
    private int[] mFrameRandom = new int[CAMERA_NUM];
    private boolean[] mFrameGet = new boolean[CAMERA_NUM];

    private CameraManager mCameraManager;
    private String[] mCameraIds = new String[CAMERA_NUM];
    private SurfaceTexture[] mSurfaceTexture = new SurfaceTexture[CAMERA_NUM];
    private SurfaceHolder[] mSurfaceHolder = new SurfaceHolder[CAMERA_NUM];
    private CameraDevice[] mCameraDevice = new CameraDevice[CAMERA_NUM];
    private ImageReader[] mImageReader = new ImageReader[CAMERA_NUM];
    private ImageReader[] mImageReaderFrame = new ImageReader[CAMERA_NUM];
    private Handler mCameraHanlder;
    private CaptureRequest.Builder[] mPreviewBuilder = new CaptureRequest.Builder[CAMERA_NUM];
    private CaptureRequest.Builder[] mPictureBuilder = new CaptureRequest.Builder[CAMERA_NUM];
    private CameraCaptureSession[] mCameraCaptureSession = new CameraCaptureSession[CAMERA_NUM];

    private final int CAMERA_OPENNING = 1;
    private final int CAMERA_OPENNED = 2;
    private final int CAMERA_PREVIEWING = 3;
    private final int CAMERA_PREVIEWED = 4;
    private final int CAMERA_CLOSING = 5;
    private final int CAMERA_CLOSED = 6;
    private final int CAMERA_ERROR = 7;

    private int[] mCameraStatus = new int[]{CAMERA_CLOSED, CAMERA_CLOSED,CAMERA_CLOSED,CAMERA_CLOSED};

    private boolean mPaused = false;
    private boolean mStreamEnable = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d(TAG, "onCreate");
        super.onCreate(savedInstanceState);
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);//去掉标题栏
        setContentView(R.layout.capture_activity);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CameraHanlderStart();
        SetCaptureUI();
    }

    private void SetCaptureUI () {
        mPictureListSpinner0 = findViewById(R.id.picturelist0);
        mPictureListSpinner0.setOnItemSelectedListener(new MySpinnerListener(0));
        mPictureListSpinner1 = findViewById(R.id.picturelist1);
        mPictureListSpinner1.setOnItemSelectedListener(new MySpinnerListener(1));
        mPictureListSpinner2 = findViewById(R.id.picturelist2);
        mPictureListSpinner2.setOnItemSelectedListener(new MySpinnerListener(2));
        mPictureListSpinner3 = findViewById(R.id.picturelist3);
        mPictureListSpinner3.setOnItemSelectedListener(new MySpinnerListener(3));

        surfaceView0 = findViewById(R.id.preview0);
        surfaceView0.getHolder().addCallback(new MySurfaceholderCallback(0));
        surfaceView1 = findViewById(R.id.preview1);
        surfaceView1.getHolder().addCallback(new MySurfaceholderCallback(1));
        surfaceView2 = findViewById(R.id.preview2);
        surfaceView2.getHolder().addCallback(new MySurfaceholderCallback(2));
        surfaceView3 = findViewById(R.id.preview3);
        surfaceView3.getHolder().addCallback(new MySurfaceholderCallback(3));

        mCapturebtn0 = findViewById(R.id.capture0);
        mCapturebtn0.setOnClickListener(mBtnClickListener);
        mCapturebtn0.setClickable(false);
        mCapturebtn1 = findViewById(R.id.capture1);
        mCapturebtn1.setOnClickListener(mBtnClickListener);
        mCapturebtn1.setClickable(false);
        mCapturebtn2 = findViewById(R.id.capture2);
        mCapturebtn2.setOnClickListener(mBtnClickListener);
        mCapturebtn2.setClickable(false);
        mCapturebtn3 = findViewById(R.id.capture3);
        mCapturebtn3.setOnClickListener(mBtnClickListener);
        mCapturebtn3.setClickable(false);

        mReturnMain = findViewById(R.id.return_main);
        mReturnMain.setOnClickListener(mBtnClickListener);
    }

    private void CameraHanlderStart() {
        HandlerThread handlerThread = new HandlerThread("camera");
        handlerThread.start();
        mCameraHanlder = new Handler(handlerThread.getLooper()){
            @Override
            public void handleMessage(Message msg) {
                super.handleMessage(msg);
                switch (msg.what) {
                    case 0:
                        if (mCameraStatus[0] > CAMERA_OPENNED) {
                            if (mCameraRunThread[0] != null) {
                                mCameraRunThread[0].interrupt();
                                mCameraRunThread[0] = null;
                            }
                            mCameraRunThread[0] = new CaptureActivity.CameraRunThread(0);
                            mCameraRunThread[0].start();
                        }
                        break;
                    case 1:
                        if (mCameraStatus[1] > CAMERA_OPENNED) {
                            if (mCameraRunThread[1] != null) {
                                mCameraRunThread[1].interrupt();
                                mCameraRunThread[1] = null;
                            }
                            mCameraRunThread[1] = new CaptureActivity.CameraRunThread(1);
                            mCameraRunThread[1].start();
                        }
                        break;
                    case 2:
                        if (mCameraStatus[2] > CAMERA_OPENNED) {
                            if (mCameraRunThread[2] != null) {
                                mCameraRunThread[2].interrupt();
                                mCameraRunThread[2] = null;
                            }
                            mCameraRunThread[2] = new CaptureActivity.CameraRunThread(2);
                            mCameraRunThread[2].start();
                        }
                        break;
                    case 3:
                        if (mCameraStatus[3] > CAMERA_OPENNED) {
                            if (mCameraRunThread[3] != null) {
                                mCameraRunThread[3].interrupt();
                                mCameraRunThread[3] = null;
                            }
                            mCameraRunThread[3] = new CaptureActivity.CameraRunThread(3);
                            mCameraRunThread[3].start();
                        }
                        break;
                }
            }
        };
    }

    @Override
    protected void onResume() {
        Log.d(TAG, "onResume Build.VERSION.SDK_INT:" + Build.VERSION.SDK_INT);
        super.onResume();
        mPaused = false;
        init();
    }

    private void init() {
        Log.d(TAG,"init +++");
        mCameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        try {
            Intent intent = getIntent();
            Log.d(TAG, "intent hasExtra CameraIds = " + intent.hasExtra("CameraIds"));
            if(intent.hasExtra("CameraIds")) {
                Log.d(TAG, "intent CameraIds success");
                String[] cameraIds = intent.getStringArrayExtra("CameraIds");
                mCameraIds = cameraIds;
            } else {
                Log.e(TAG, "intent CameraIds error , mCameraIds = mCameraManager.getCameraIdList()");
                mCameraIds = mCameraManager.getCameraIdList();
            }
            Log.d(TAG, "cameraids size:" + mCameraIds.length);
            for (int i = 0; i < CAMERA_NUM; i++) {
                Log.d(TAG, "surfaceholder " + i + ":" + mSurfaceHolder[i] + ",mCameraStatus:"
                        + mCameraStatus[i]);
                if (mSurfaceHolder[i] != null && mCameraStatus[i] > CAMERA_PREVIEWED)
                    startCameraApi2(i);
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private class MySpinnerListener implements OnItemSelectedListener {
        private int mIndex;

        public MySpinnerListener(int index) {
            mIndex = index;
        }

        @Override
        public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
            Log.d(TAG, "onItemSelected:" + i);
            if (mPaused || mCurPicIndex[mIndex] == i) return;
            stopPreview(mIndex);
            mImageReader[mIndex] = ImageReader.newInstance(mPictureSizes[mIndex][i].getWidth(),
                    mPictureSizes[mIndex][i].getHeight(), ImageFormat.JPEG, 2);
            mImageReader[mIndex].setOnImageAvailableListener(new MyImageAvailableListener(mIndex), mCameraHanlder);
            startPreview(mIndex);
        }

        @Override
        public void onNothingSelected(AdapterView<?> adapterView) {

        }
    }

    private Button getCaptureBtn(int index) {
        switch (index) {
            case 0:
                return mCapturebtn0;
            case 1:
                return mCapturebtn1;
            case 2:
                return mCapturebtn2;
            case 3:
                return mCapturebtn3;
        }
        return null;
    }

    private Spinner getSpinner(int index) {
        switch (index) {
            case 0:
                return mPictureListSpinner0;
            case 1:
                return mPictureListSpinner1;
            case 2:
                return mPictureListSpinner2;
            case 3:
                return mPictureListSpinner3;
        }
        return null;
    }


    @Override
    protected void onPause() {
        Log.d(TAG, "onPause");
        mPaused = true;
        super.onPause();
        for (int i = 0; i < CAMERA_NUM; i++) {
            if (mStreamEnable) {
                mFrameGet[i] = false;
                mCameraHanlder.removeMessages(i);
            }
            stopPreview(i);
            closeCameraApi2(i);
            mCameraStatus[i] = CAMERA_CLOSED;
        }
        mStreamEnable = false;
        finish();
    }

    @Override
    protected void onStop() {
        Log.d(TAG, "onStop");
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG, "onDestroy");
        super.onDestroy();
    }

    private Button.OnClickListener mBtnClickListener = new Button.OnClickListener(){

        @Override
        public void onClick(View view) {
            if (mPaused) return;
            switch (view.getId()) {
                case R.id.return_main:
                    Intent intent = new Intent();
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    intent.setClass(getApplicationContext(), MainActivity.class);
                    startActivity(intent);
                    break;
                case R.id.capture0:
                    Log.d(TAG, "capture 0");
                    mCapturebtn0.setClickable(false);
                    mPictureListSpinner0.setClickable(false);
                    takePicture(0);
                    break;
                case R.id.capture1:
                    Log.d(TAG, "capture 1");
                    mCapturebtn1.setClickable(false);
                    mPictureListSpinner1.setClickable(false);
                    takePicture(1);
                    break;
                case R.id.capture2:
                    Log.d(TAG, "capture 2");
                    mCapturebtn2.setClickable(false);
                    mPictureListSpinner2.setClickable(false);
                    takePicture(2);
                    break;
                case R.id.capture3:
                    Log.d(TAG, "capture 3");
                    mCapturebtn3.setClickable(false);
                    mPictureListSpinner3.setClickable(false);
                    takePicture(3);
                    break;
            }
        }
    };


    private class MySurfaceholderCallback implements SurfaceHolder.Callback {

        private int mIndex;
        public MySurfaceholderCallback(int index) {
            mIndex = index;
        }

        @Override
        public void surfaceCreated(SurfaceHolder surfaceHolder) {
            Log.d(TAG, "surfaceCreated:" + mIndex);
            mSurfaceHolder[mIndex] = surfaceHolder;
            mSurfaceHolder[mIndex].setFixedSize(1920, 1080);
            if (mCameraStatus[mIndex] > CAMERA_PREVIEWED)
                startCameraApi2(mIndex);
        }

        @Override
        public void surfaceChanged(SurfaceHolder surfaceHolder, int i, int i1, int i2) {
            Log.d(TAG, "surfaceChanged:" + mIndex);
            if (mSurfaceHolder[mIndex] != surfaceHolder) {
                mSurfaceHolder[mIndex] = surfaceHolder;
                mSurfaceHolder[mIndex].setFixedSize(1920, 1080);
            }
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
            Log.d(TAG, "surfaceDestroyed:" + mIndex);
            stopPreview(mIndex);
            closeCameraApi2(mIndex);
            mSurfaceHolder[mIndex] = null;
        }
    }

    private class CameraRunThread extends Thread {
        private int mIndex;
        public CameraRunThread(int index) {
            mIndex = index;
        }
        @Override
        public void run() {
            if (mCameraRunning[mIndex]) {
                Log.e(TAG, "camera " + mIndex + " case3 running");
                return;
            }
            mCameraRunning[mIndex] = true;
            stopPreview(mIndex);
            closeCameraApi2(mIndex);
            mFrameGet[mIndex] = true;
            startCameraApi2(mIndex);
            mCameraRunning[mIndex] = false;
        }

    }

    private class MySurfaceTextureListener implements TextureView.SurfaceTextureListener {
        private int mIndex;

        public MySurfaceTextureListener(int index) {
            mIndex = index;
        }
        @Override
        public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int i, int i1) {
            Log.d(TAG, "onSurfaceTextureAvailable");
            mSurfaceTexture[mIndex] = surfaceTexture;
            if (mCameraStatus[mIndex] > CAMERA_PREVIEWED) {
                startCameraApi2(mIndex);
            }
        }

        @Override
        public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int i, int i1) {
            mSurfaceTexture[mIndex] = surfaceTexture;
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
            stopPreview(mIndex);
            closeCameraApi2(mIndex);
            mSurfaceTexture[mIndex] = null;
            return false;
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {
            if (mSurfaceTexture[mIndex] != surfaceTexture)
                mSurfaceTexture[mIndex] = surfaceTexture;
        }
    }

    private class MyImageAvailableListener implements ImageReader.OnImageAvailableListener {
        private int mIndex;
        public MyImageAvailableListener(int index){
            mIndex = index;
        }
        @Override
        public void onImageAvailable(ImageReader imageReader) {
            Log.d(TAG, "camera " + mIndex + " onImageAvailable");
            if (mPaused) return;
            Image image = imageReader.acquireLatestImage();
            new Thread(new ImageSaver(image, mIndex)).start();
        }
    }

    private class MyImageFrameAvailableListener implements ImageReader.OnImageAvailableListener {
        private int mIndex;
        public MyImageFrameAvailableListener(int index){
            mIndex = index;
        }
        @Override
        public void onImageAvailable(ImageReader imageReader) {
            Log.d(TAG, "camera " + mIndex + " onImageAvailable 222222");
            if (mPaused) return;
            mFrameCount[mIndex]++;
            mFrameGetCount[mIndex]++;
            try {
                Image image = imageReader.acquireNextImage();
                if (image != null) {
                    image.close(); // 释放 Image 对象
                }
            } catch (Exception e) {
                Log.e(TAG, "image close:" + e);
            } finally {

            }
        }
    }

    private class MyCamStateCallback extends CameraDevice.StateCallback {
        private int mIndex;
        public MyCamStateCallback(int index){
            mIndex = index;
        }
        @Override
        public void onOpened(@NonNull CameraDevice cameraDevice) {
            Log.d(TAG, "camera " + mIndex + " opened");
            if (mPaused) return;
            mCameraStatus[mIndex] = CAMERA_OPENNED;
            mCameraDevice[mIndex] = cameraDevice;
            startPreview(mIndex);
        }

        @Override
        public void onDisconnected(@NonNull CameraDevice cameraDevice) {
            Log.d(TAG, "camera " + mIndex + " disconnect");
            mCameraStatus[mIndex] = CAMERA_CLOSED;
        }

        @Override
        public void onError(@NonNull CameraDevice cameraDevice, int i) {
            Log.d(TAG, "camera " + mIndex + " error");
            mCameraStatus[mIndex] = CAMERA_ERROR;
        }
    }

    private class MyPicCaptureCallback extends CameraCaptureSession.CaptureCallback {
        private int mIndex;

        public MyPicCaptureCallback(int index) {
            mIndex = index;
        }

        @Override
        public void onCaptureCompleted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull TotalCaptureResult result) {
            super.onCaptureCompleted(session, request, result);
            Log.d(TAG, "Pic onCaptureCompleted " + mIndex);
            if (mPaused) return;
            try {
                int[] afmode = mCameraManager.getCameraCharacteristics(mCameraIds[mIndex]).get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
                if (Arrays.asList(afmode).contains(CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE))
                    mPreviewBuilder[mIndex].set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
                int[] awbmode = mCameraManager.getCameraCharacteristics(mCameraIds[mIndex]).get(CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES);
                for (int mode : awbmode)
                    Log.d(TAG, "support awbmode:" + mode);
                if (mIndex == 0) {
                    mPreviewBuilder[mIndex].set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_AUTO);
                    Log.d(TAG, "set awb mode cloudy");
                }
                mCameraCaptureSession[mIndex].setRepeatingRequest(mPreviewBuilder[mIndex].build(), new MyPreCaptureCallback(mIndex), mCameraHanlder);
            } catch (Exception e) {
                Log.e(TAG, "capture session config fail:" + e);
                mCameraStatus[mIndex] = CAMERA_ERROR;
            }

        }

        @Override
        public void onCaptureFailed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureFailure failure) {
            super.onCaptureFailed(session, request, failure);
            Log.d(TAG, "Pic onCaptureFailed " + mIndex);
            mCameraStatus[mIndex] = CAMERA_ERROR;
            Toast.makeText(getApplicationContext(),"Camera " + mIndex + " 拍照失败",Toast.LENGTH_SHORT).show();

        }

        @Override
        public void onCaptureStarted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, long timestamp, long frameNumber) {
            super.onCaptureStarted(session, request, timestamp, frameNumber);
            Log.d(TAG, "Pic onCaptureStarted " + mIndex);
            getCaptureBtn(mIndex).setClickable(true);
            getSpinner(mIndex).setClickable(true);
        }

        @Override
        public void onCaptureSequenceAborted(@NonNull CameraCaptureSession session, int sequenceId) {
            super.onCaptureSequenceAborted(session, sequenceId);
            Log.d(TAG, "Pic onCaptureSequenceAborted " + mIndex);
        }

        @Override
        public void onCaptureBufferLost(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull Surface target, long frameNumber) {
            super.onCaptureBufferLost(session, request, target, frameNumber);
            Log.d(TAG, "Pic onCaptureBufferLost " + mIndex);
        }

        @Override
        public void onCaptureSequenceCompleted(@NonNull CameraCaptureSession session, int sequenceId, long frameNumber) {
            super.onCaptureSequenceCompleted(session, sequenceId, frameNumber);
            Log.d(TAG, "Pic onCaptureSequenceCompleted " + mIndex);
        }

        @Override
        public void onCaptureProgressed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureResult partialResult) {
            super.onCaptureProgressed(session, request, partialResult);
            Log.d(TAG, "Pic onCaptureProgressed " + mIndex);
        }
    }

    private class MyPreCaptureCallback extends CameraCaptureSession.CaptureCallback {
        private int mIndex;

        public MyPreCaptureCallback(int index) {
            mIndex = index;
        }

        @Override
        public void onCaptureCompleted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull TotalCaptureResult result) {
            super.onCaptureCompleted(session, request, result);
            //Log.d(TAG, "Pre onCaptureCompleted " + mIndex);
            mCameraStatus[mIndex] = CAMERA_PREVIEWED;

        }

        @Override
        public void onCaptureFailed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureFailure failure) {
            super.onCaptureFailed(session, request, failure);
            Log.d(TAG, "Pre onCaptureFailed " + mIndex);
            mCameraStatus[mIndex] = CAMERA_ERROR;
        }

        @Override
        public void onCaptureStarted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, long timestamp, long frameNumber) {
            super.onCaptureStarted(session, request, timestamp, frameNumber);
            //Log.d(TAG, "Pre onCaptureStarted " + mIndex);
        }

        @Override
        public void onCaptureSequenceAborted(@NonNull CameraCaptureSession session, int sequenceId) {
            super.onCaptureSequenceAborted(session, sequenceId);
            Log.d(TAG, "Pre onCaptureSequenceAborted " + mIndex);
        }

        @Override
        public void onCaptureBufferLost(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull Surface target, long frameNumber) {
            super.onCaptureBufferLost(session, request, target, frameNumber);
            Log.d(TAG, "Pre onCaptureBufferLost " + mIndex);
        }

        @Override
        public void onCaptureSequenceCompleted(@NonNull CameraCaptureSession session, int sequenceId, long frameNumber) {
            super.onCaptureSequenceCompleted(session, sequenceId, frameNumber);
            Log.d(TAG, "Pre onCaptureSequenceCompleted " + mIndex);
        }

        @Override
        public void onCaptureProgressed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureResult partialResult) {
            super.onCaptureProgressed(session, request, partialResult);
            Log.d(TAG, "Pre onCaptureProgressed " + mIndex);
        }
    }

    private class MyCaptureSessionCallback extends CameraCaptureSession.StateCallback {
        private int mIndex;

        public MyCaptureSessionCallback(int index) {
            mIndex = index;
        }
        @Override
        public void onConfigured(@NonNull CameraCaptureSession cameraCaptureSession) {
            Log.d(TAG, "camera " + mIndex + " onConfigured");
            if (mPaused) return;
            try {
                int[] afmode = mCameraManager.getCameraCharacteristics(mCameraIds[mIndex]).get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);
                if (Arrays.asList(afmode).contains(CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE))
                    mPreviewBuilder[mIndex].set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);

                int[] awbmode = mCameraManager.getCameraCharacteristics(mCameraIds[mIndex]).get(CameraCharacteristics.CONTROL_AWB_AVAILABLE_MODES);
                for (int mode : awbmode)
                    Log.d(TAG, "support awbmode:" + mode);
                if (mIndex == 0) {
                    Log.d(TAG, "awb mode incand");
                    mPreviewBuilder[mIndex].set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_AUTO);
                }
                CaptureRequest request = mPreviewBuilder[mIndex].build();
                cameraCaptureSession.setRepeatingRequest(request, new MyPreCaptureCallback(mIndex), mCameraHanlder);
                mCameraCaptureSession[mIndex] = cameraCaptureSession;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (mStreamEnable) return;
                        getCaptureBtn(mIndex).setClickable(true);
                        getSpinner(mIndex).setClickable(true);
                    }
                });
            } catch (Exception e) {
                Log.e(TAG, "capture session config fail:" + e);
                mCameraStatus[mIndex] = CAMERA_ERROR;
            }
        }

        @Override
        public void onConfigureFailed(@NonNull CameraCaptureSession cameraCaptureSession) {
            Log.d(TAG, "camera " + mIndex + " onConfigureFailed");
            mCameraStatus[mIndex] = CAMERA_ERROR;
            closeCameraApi2(mIndex);
        }
    }


    private void startCameraApi2(int index) {
        Log.d(TAG, "camera " + index + " start open");
        mCameraStatus[index] = CAMERA_OPENNING;
        try {
            if (adapter[index] == null) {
                StreamConfigurationMap map = mCameraManager.getCameraCharacteristics(mCameraIds[index]).get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                mPreviewSize[index] = map.getOutputSizes(SurfaceTexture.class);
                for (Size size : mPreviewSize[index])
                    Log.d(TAG, "previewsize:" + size.getWidth() + "x" + size.getHeight());

                mPictureSizes[index] = map.getOutputSizes(ImageFormat.JPEG);
                String[] list = new String[mPictureSizes[index].length];
                for (int i = 0; i < mPictureSizes[index].length; i++) {
                    Size size = mPictureSizes[index][i];
                    Log.d(TAG, "picturesize:" + size.getWidth() + "x" + size.getHeight());
                    list[i] = size.getWidth() + "x" + size.getHeight();
                }
                adapter[index] = new TestArrayAdapter(getApplicationContext(), list);
                adapter[index].setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
                mCurPicIndex[index] = 0;
                getSpinner(index).setSelection(0);
                getSpinner(index).setAdapter(adapter[index]);
            }

            mImageReader[index] = ImageReader.newInstance(mPictureSizes[index][0].getWidth(),
                    mPictureSizes[index][0].getHeight(), ImageFormat.JPEG, 2);
            Log.d(TAG, "jpeg surface:" + mImageReader[index].getSurface());
            mImageReader[index].setOnImageAvailableListener(new MyImageAvailableListener(index), mCameraHanlder);

            mImageReaderFrame[index] = ImageReader.newInstance(1280,
                    960, ImageFormat.YUV_420_888, 3);
            Log.d(TAG, "frame surface:" + mImageReaderFrame[index].getSurface());
            mImageReaderFrame[index].setOnImageAvailableListener(new MyImageFrameAvailableListener(index), mCameraHanlder);
            mCameraManager.openCamera(mCameraIds[index], new MyCamStateCallback(index), mCameraHanlder);
        } catch (SecurityException e) {
            Log.e(TAG, "SecurityException");
            mCameraStatus[index] = CAMERA_ERROR;
        } catch (Exception e) {
            Log.e(TAG, "open camera fail:" + e);
            mCameraStatus[index] = CAMERA_ERROR;
        }
    }

    private void closeCameraApi2(int index) {
        Log.d(TAG, "camera " + index + " start close");
        mCameraStatus[index] = CAMERA_CLOSING;
        if (mCameraDevice[index] != null) mCameraDevice[index].close();
        try {
            if (mImageReader[index] != null) mImageReader[index].close();
        } catch (Exception e) {

        } finally {
            try {
                if (mImageReaderFrame[index] != null) mImageReaderFrame[index].close();
            } catch (Exception e) {

            } finally {
                mCameraStatus[index] = CAMERA_CLOSED;
                Log.d(TAG, "camera " + index + " end close");
            }
        }
    }

    private void takePicture(int index) {
        if (mPaused || mCameraStatus[index] != CAMERA_PREVIEWED) return;
        Log.d(TAG, "camera " + index + " start takePicture");
        try {
            mPictureBuilder[index] = mCameraDevice[index].createCaptureRequest(CameraDevice.TEMPLATE_STILL_CAPTURE);
            mPictureBuilder[index].addTarget(mImageReader[index].getSurface());

            if (index == 0)
                mPictureBuilder[index].set(CaptureRequest.CONTROL_AWB_MODE, CaptureRequest.CONTROL_AWB_MODE_AUTO);

            mCameraCaptureSession[index].stopRepeating();
            mCameraCaptureSession[index].abortCaptures();
            mCameraCaptureSession[index].capture(mPictureBuilder[index].build(), new MyPicCaptureCallback(index), mCameraHanlder);
        } catch (CameraAccessException e) {
            e.printStackTrace();
            mCameraStatus[index] = CAMERA_ERROR;
            closeCameraApi2(index);
        }
    }

    public class ImageSaver implements Runnable {
        private Image mImage;
        private File mImageFile;
        private int mIndex;

        public ImageSaver(Image image, int index) {
            mImage = image;
            mIndex = index;
        }

        @Override
        public void run() {
            Log.d(TAG, "start save camera " + mIndex + " picture");
            ByteBuffer buffer = mImage.getPlanes()[0].getBuffer();
            byte[] data = new byte[buffer.remaining()];
            buffer.get(data);
            long currentTimeMillis = System.currentTimeMillis();
            long currentSeconds = currentTimeMillis / 1000 % 1000000;
            String sixDigitTime = String.format("%06d", currentSeconds);
            String filePath = Environment.getExternalStorageDirectory() + "/DCIM/myPicture" + mIndex + "_"
                    + sixDigitTime + ".jpg";
            mImageFile = new File(filePath);
            FileOutputStream fos = null;
            try {
                fos = new FileOutputStream(mImageFile);
                fos.write(data, 0, data.length);
            } catch (IOException e) {
                e.printStackTrace();
            } finally {
                try {
                    mImage.close();
                } catch (Exception e) {
                    Log.d(TAG, "jpeg image close:" + e);
                } finally {
                    mImageFile = null;
                    if (fos != null) {
                        try {
                            fos.close();
                            fos = null;
                        } catch (IOException e) {
                            e.printStackTrace();
                        }
                    }
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            getCaptureBtn(mIndex).setClickable(true);
                            getSpinner(mIndex).setClickable(true);
                            Toast.makeText(getApplicationContext(),"Camera " + mIndex + " 拍照成功",Toast.LENGTH_SHORT).show();
                        }
                    });
                    addSaveFileToDb(filePath);
                    Log.d(TAG, "end save camera " + mIndex + " picture");
                }

            }
        }
    }

    private void addSaveFileToDb(String filePath) {
        Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
        intent.setData(Uri.fromFile(new File(filePath)));
        sendBroadcast(intent);
    }

    private void startPreview(int index) {
        if (mPaused || mCameraDevice[index] == null) return;
        Log.d(TAG, "start preview " + index);
        try {
            mCameraStatus[index] =  CAMERA_PREVIEWING;
            Surface previewSurface = null;

            mPreviewBuilder[index] = mCameraDevice[index].createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            if (mSurfaceHolder[index] != null) {
                Log.d(TAG, "add preview surface 11");
                //mSurfaceTexture[index].setDefaultBufferSize(/*mPreviewSize[index][0].getWidth()*/1280, 960/*mPreviewSize[index][0].getHeight()*/);
                //previewSurface = new Surface(mSurfaceTexture[index]);
                previewSurface = mSurfaceHolder[index].getSurface();
                Log.d(TAG, "add preview surface:" + previewSurface);
                mPreviewBuilder[index].addTarget(previewSurface);
            }
            //mPreviewBuilder[index].addTarget(mImageReader[index].getSurface());
            if (mFrameGet[index]) {
                Log.d(TAG, "add frame surface:" + mImageReaderFrame[index].getSurface());
                mPreviewBuilder[index].addTarget(mImageReaderFrame[index].getSurface());
                if (previewSurface != null) {
                    Log.d(TAG, "createCaptureSession 3 stream");
                    mCameraDevice[index].createCaptureSession(Arrays.asList(previewSurface, mImageReaderFrame[index].getSurface(),mImageReader[index].getSurface()),
                            new MyCaptureSessionCallback(index), mCameraHanlder);
                } else {
                    Log.d(TAG, "createCaptureSession 2 stream");
                    mCameraDevice[index].createCaptureSession(Arrays.asList(mImageReaderFrame[index].getSurface(),mImageReader[index].getSurface()),
                            new MyCaptureSessionCallback(index), mCameraHanlder);
                }
            } else {
                Log.d(TAG, "createCaptureSession 1 stream");
                mCameraDevice[index].createCaptureSession(Arrays.asList(previewSurface, mImageReader[index].getSurface()),
                        new MyCaptureSessionCallback(index), mCameraHanlder);
            }
        } catch (Exception e) {
            Log.d(TAG, "start preview fail:" + e);
        }
    }

    private void stopPreview(int index) {
        Log.d(TAG, "stopPreviwe " + index);
        if (mCameraCaptureSession[index] != null) {
            try {
                mCameraCaptureSession[index].stopRepeating();
                mCameraCaptureSession[index].abortCaptures();
                mCameraCaptureSession[index].close();
            } catch (CameraAccessException e) {
                e.printStackTrace();
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    private class TestArrayAdapter extends ArrayAdapter<String> {
        private Context mContext;
        private String [] mStringArray;
        public TestArrayAdapter(Context context, String[] stringArray) {
            super(context, android.R.layout.simple_spinner_item, stringArray);
            mContext = context;
            mStringArray=stringArray;
        }

        @Override
        public View getDropDownView(int position, View convertView, ViewGroup parent) {
            //修改Spinner展开后的字体颜色
            if (convertView == null) {
                LayoutInflater inflater = LayoutInflater.from(mContext);
                convertView = inflater.inflate(android.R.layout.simple_spinner_dropdown_item, parent,false);
            }

            TextView tv = (TextView) convertView.findViewById(android.R.id.text1);
            tv.setText(mStringArray[position]);
            tv.setTextSize(30);
            tv.setTextColor(Color.rgb(5,5,5));

            return convertView;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                LayoutInflater inflater = LayoutInflater.from(mContext);
                convertView = inflater.inflate(android.R.layout.simple_spinner_item, parent, false);
            }

            TextView tv = (TextView) convertView.findViewById(android.R.id.text1);
            tv.setText(mStringArray[position]);
            tv.setTextSize(30);
            tv.setTextColor(Color.WHITE);
            return convertView;
        }
    }
}

