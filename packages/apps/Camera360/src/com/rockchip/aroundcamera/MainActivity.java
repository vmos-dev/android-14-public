package com.rockchip.aroundcamera;

import android.Manifest;
import android.annotation.NonNull;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ImageFormat;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
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
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Size;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.widget.Button;
import android.widget.LinearLayout;
import java.util.Arrays;
import java.util.Collections;
import java.util.Timer;
import java.util.TimerTask;

import com.rockchip.aroundcamera.AroundCameraJNI;

public class MainActivity extends Activity {
    private final static String TAG = "RK_AroundCamera";

    private AroundCameraJNI mCameraJNI;
    private int bitmp_w, bitmp_h;
    private int buffer_index = 0;
    private boolean algo_native_run = false;
    private Bitmap fSkiaBitmap;
    private int front_state,back_state,left_state,right_state,lr_state,top_state;
    private boolean algo_running = false;
    private boolean machine_yaxun = false;
    private SkiaDrawView my_draw_view;

    private final static int CAMERA_NUM = 4;

    private CameraRunThread[] mCameraRunThread = new CameraRunThread[CAMERA_NUM];
    private boolean[] mCameraRunning = new boolean[CAMERA_NUM];

    private HardwareBuffer[][] mHardwardbufferArray = new HardwareBuffer[2][CAMERA_NUM];
    private Handler mProcessHandler;

    private CameraManager mCameraManager;
    private String[] mCameraIds = new String[CAMERA_NUM];
    private CameraDevice[] mCameraDevice = new CameraDevice[CAMERA_NUM];
    private ImageReader[] mImageReader = new ImageReader[CAMERA_NUM];
    private ImageReader[] mImageReaderFrame = new ImageReader[CAMERA_NUM];
    private Handler mCameraHanlder;
    private CaptureRequest.Builder[] mPreviewBuilder = new CaptureRequest.Builder[CAMERA_NUM];
    private CameraCaptureSession[] mCameraCaptureSession = new CameraCaptureSession[CAMERA_NUM];

    private final static int CAMERA_OK = 10001;

    private final int CAMERA_OPENNING = 1;
    private final int CAMERA_OPENNED = 2;
    private final int CAMERA_PREVIEWING = 3;
    private final int CAMERA_PREVIEWED = 4;
    private final int CAMERA_CLOSING = 5;
    private final int CAMERA_CLOSED = 6;
    private final int CAMERA_ERROR = 7;

    private Button btn_front;
    private Button btn_back;
    private Button btn_left;
    private Button btn_right;
    private Button btn_lr;
    private Button btn_top;
    private Button button_set;
    private Button button_capture;

    private Timer fAnimationTimer;

    private int[] mCameraStatus = new int[]{CAMERA_CLOSED, CAMERA_CLOSED,CAMERA_CLOSED,CAMERA_CLOSED};

    HandlerThread mProcesshandlerThread;
    HandlerThread mCamerahandlerThread;

    private HardwareBuffer mErrorBuffer = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "onCreate");
        this.requestWindowFeature(Window.FEATURE_NO_TITLE);//去掉标题栏
        setContentView(R.layout.activity_main);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        SetMainUI();
        mCameraJNI = new AroundCameraJNI();
        mCameraJNI.init();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.CUPCAKE) {
            new AsyncTask<Void, Void, Void>() {
                @Override
                protected Void doInBackground(Void... voids) {
                    CameraHanlderStart();
                    return null;
                }

                @Override
                protected void onPostExecute(Void aVoid) {
                    ProcessHanlderStart();
                    SetSkiaDrawView();
                }
            }.execute();
        }
    }

    // 简单消息提示框
    private void showExitDialog() {
        new AlertDialog.Builder(this)
                .setTitle("注意")
                .setMessage("该功能仅在车辆行驶状态下显现直观效果！")
                .setPositiveButton("确定", null)
                .show();
    }

    private void initErrorHardwareBuffer(Bitmap bitmap) {
        try {
            int width = bitmap.getWidth();
            int height = bitmap.getHeight();

            mErrorBuffer = HardwareBuffer.create(width, height, HardwareBuffer.RGBA_8888, 1, HardwareBuffer.USAGE_GPU_COLOR_OUTPUT);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void SetMainUI() {
        LinearLayout layout=(LinearLayout) findViewById(R.id.linearlay_1);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);
        my_draw_view = new SkiaDrawView(this);
        layout.addView(my_draw_view, params);

        Drawable drawable = getResources().getDrawable(R.drawable.errorimage);
        Bitmap bitmap = ((BitmapDrawable) drawable).getBitmap();
        initErrorHardwareBuffer(bitmap);
        if (mErrorBuffer == null) {
            Log.e(TAG, "mErrorBuffer == null");
        }

        btn_front = (Button) this.findViewById(R.id.button_front);
        btn_front.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(machine_yaxun == false) {
                    if (front_state == 0){
                        btn_front.setText("前视广角");
                        front_state = 2;
                        mCameraJNI.nativeSetSurroundType(0);
                    }else if(front_state == 4 || front_state == 2) {
                        btn_front.setText("顶视\n前视鱼眼");
                        front_state = 3;
                        mCameraJNI.nativeSetSurroundType(1);
                    } else if (front_state == 3) {
                        btn_front.setText("前视3D");
                        front_state = 0;
                        mCameraJNI.nativeSetSurroundType(3);
                    }

                }else{
                    btn_front.setText("顶视\n前视鱼眼");
//                    setAlgoType_java(2);
                }
            }
        });

        btn_back = (Button) this.findViewById(R.id.button_back);
        btn_back.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(machine_yaxun == false) {
                    if (back_state == 0){
                        btn_back.setText("后视广角");
                        back_state = 2;
                        mCameraJNI.nativeSetSurroundType(4);
                    }else if(back_state == 2 || back_state == 4) {
                        btn_back.setText("顶视\n后视鱼眼");
                        back_state = 3;
                        mCameraJNI.nativeSetSurroundType(5);
                    } else if (back_state == 3) {
                        btn_back.setText("后视3D");
                        back_state = 0;
                        mCameraJNI.nativeSetSurroundType(7);
                    }

                }else{
                    btn_back.setText("顶视\n后视鱼眼");
//                    setAlgoType_java(6);
                }
            }
        });

        btn_left = (Button) this.findViewById(R.id.button_left);
        btn_left.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(machine_yaxun == false) {
                    if(left_state == 0 || left_state == 2) {
                        btn_left.setText("顶视\n左视2D");
                        left_state = 1;
                    }else if(left_state == 1){
                        btn_left.setText("顶视\n左视3D");
                        left_state = 2;
                    }

                    mCameraJNI.nativeSetSurroundType(left_state + 7);
                }
            }
        });

        btn_right = (Button) this.findViewById(R.id.button_right);
        btn_right.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(machine_yaxun == false) {
                    if(right_state == 0 || right_state == 2) {
                        btn_right.setText("顶视\n右视2D");
                        right_state = 1;
                    }else if(right_state == 1){
                        btn_right.setText("顶视\n右视3D");
                        right_state = 2;
                    }

                    mCameraJNI.nativeSetSurroundType(right_state + 10);
                }else{
                    btn_right.setText("顶视\n右视2D");
//                    setAlgoType_java(10);
                }
            }
        });

        btn_lr = (Button) this.findViewById(R.id.button_lr);
        btn_lr.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(machine_yaxun == false) {
                    if (lr_state == 0) {
                        btn_lr.setText("底盘透视关");
                        lr_state = 1;
                    } else if (lr_state == 1) {
                        btn_lr.setText("底盘透视开");
                        showExitDialog();
                        lr_state = 0;
                    }
                    mCameraJNI.nativeSetSurroundType(lr_state + 19);
                }
            }
        });

        btn_top = (Button) this.findViewById(R.id.button_top);
        btn_top.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (top_state == 0 || top_state == 2) {
                    btn_top.setText("环绕车3D");
                    top_state = 1;
                } else if (top_state == 1) {
                    btn_top.setText("顶视\n环绕3D");
                    top_state = 2;
                }

                mCameraJNI.nativeSetSurroundType(top_state + 15);
            }
        });

        button_capture = (Button) this.findViewById(R.id.button_capture);
        button_capture.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent();
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                intent.putExtra("CameraIds", mCameraIds);
                intent.setClass(getApplicationContext(), CaptureActivity.class);
                startActivity(intent);
            }
        });
    }

    private void SetSkiaDrawView() {
        fAnimationTimer = new Timer();
        fAnimationTimer.schedule(new TimerTask() {
            public void run() {
                // This will request an update of the SkiaDrawView, even from other threads
                my_draw_view.postInvalidate();
            }
        }, 0, 5);
    }

    private void ProcessHanlderStart(){
        mProcesshandlerThread = new HandlerThread("process");
        mProcesshandlerThread.start();
        mProcessHandler = new Handler(mProcesshandlerThread.getLooper()) {
            @Override
            public void handleMessage(Message msg) {

                if (((msg.what >> 8) & 0xff) == 1 && !isAllElementsFilled(mHardwardbufferArray[buffer_index])) {
                    // 获取传递过来的HardwareBufferArray
                    mHardwardbufferArray[buffer_index][(msg.what & 0xff)] = (HardwareBuffer) msg.obj;
                }

                for(int i = 0;i < CAMERA_NUM;i++) {
                    if (mCameraStatus[i] == CAMERA_ERROR) {
                        mHardwardbufferArray[buffer_index][i] = mErrorBuffer;
                    }
                }

                if (isAllElementsFilled(mHardwardbufferArray[buffer_index]) && !algo_native_run) {
                    // 在这里执行对HardwareBufferArray的操作
                    algo_native_run = true;

                    mCameraJNI.process(mHardwardbufferArray[buffer_index]);
                    Arrays.fill(mHardwardbufferArray[buffer_index], null); //已经完成清空数组
                    algo_native_run = false;
                }

            }
        };
    }

    private void CameraHanlderStart() {
        mCamerahandlerThread = new HandlerThread("camera");
        mCamerahandlerThread.start();
        mCameraHanlder = new Handler(mCamerahandlerThread.getLooper()){
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
                            mCameraRunThread[0] = new CameraRunThread(0);
                            mCameraRunThread[0].start();
                        }
                        break;
                    case 1:
                        if (mCameraStatus[1] > CAMERA_OPENNED) {
                            if (mCameraRunThread[1] != null) {
                                mCameraRunThread[1].interrupt();
                                mCameraRunThread[1] = null;
                            }
                            mCameraRunThread[1] = new CameraRunThread(1);
                            mCameraRunThread[1].start();
                        }
                        break;
                    case 2:
                        if (mCameraStatus[2] > CAMERA_OPENNED) {
                            if (mCameraRunThread[2] != null) {
                                mCameraRunThread[2].interrupt();
                                mCameraRunThread[2] = null;
                            }
                            mCameraRunThread[2] = new CameraRunThread(2);
                            mCameraRunThread[2].start();
                        }
                        break;
                    case 3:
                        if (mCameraStatus[3] > CAMERA_OPENNED) {
                            if (mCameraRunThread[3] != null) {
                                mCameraRunThread[3].interrupt();
                                mCameraRunThread[3] = null;
                            }
                            mCameraRunThread[3] = new CameraRunThread(3);
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    != PackageManager.PERMISSION_GRANTED || checkSelfPermission(Manifest.permission.CAMERA)
                    != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.CAMERA, Manifest.permission.WRITE_EXTERNAL_STORAGE}, CAMERA_OK);
                return;
            }
        }
        init();
    }

    private void init() {
        Log.d(TAG,"init +++");
        mCameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        try {
            String[] cameraIds = mCameraManager.getCameraIdList();
            int base_offset = Integer.valueOf(cameraIds[0]).intValue() >= 100 ? 100 : 0;
            int CurrentCameraIds[] = new int[CAMERA_NUM];
            for (int i = 0; i < CAMERA_NUM; i++) {
                CurrentCameraIds[i] = mCameraJNI.getCameraList()[i] + base_offset;
                mCameraIds[i] = Integer.toString(CurrentCameraIds[i]);
            }

            Log.d(TAG, "cameraids size:" + mCameraIds.length);
            for (int i = 0; i < CAMERA_NUM; i++) {
                Log.d(TAG,   "mCameraStatus:" + mCameraStatus[i]);
                if (mCameraStatus[i] > CAMERA_PREVIEWED)
                    startCameraApi2(i);
            }
            for (int i = 0; i < CAMERA_NUM; i++) {
                mCameraStatus[i] = CAMERA_CLOSED;
                mCameraHanlder.sendEmptyMessage(i);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,String[] permissions,int[] grantResults) {
        switch (requestCode) {
            case CAMERA_OK:
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    init();
                } else {
                    showWaringDialog();
                }
                break;
            default:
                break;
        }
    }

    private void showWaringDialog() {
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("警告！")
                .setMessage("请前往设置->应用->PermissionDemo->权限中打开相关权限，否则功能无法正常运行！")
                .setPositiveButton("确定", new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        // 一般情况下如果用户不授权的话，功能是无法运行的，做退出处理
                        finish();
                    }
                }).show();
    }

    @Override
    protected void onPause() {
        super.onPause();
        for (int i = 0; i < CAMERA_NUM; i++) {
            if (mCameraHanlder!=null) {
                mCameraHanlder.removeMessages(i);
            }
            stopPreview(i);
            closeCameraApi2(i);
            mCameraStatus[i] = CAMERA_CLOSED;
        }
        if (fAnimationTimer != null) {
            fAnimationTimer.cancel();
            fAnimationTimer = null;
        }
        if (fSkiaBitmap != null) {
            fSkiaBitmap.recycle();
            fSkiaBitmap = null;
        }
        if (mCameraJNI != null) {
            mCameraJNI.clear();
        }
        if (mErrorBuffer != null) {
            mErrorBuffer.close();
        }
        Log.d(TAG, "onPause");
        finish();
    }

    @Override
    protected void onStop() {
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "onDestroy");
    }

    private class CameraRunThread extends Thread{
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
            for (int i = 0; i < CAMERA_NUM; i++) {
                if (mIndex == mCameraJNI.getCameraList()[i]) {
                    mCameraRunning[mIndex] = true;
//                    stopPreview(mIndex);
//                    closeCameraApi2(mIndex);
                    startCameraApi2(mIndex);
                    mCameraRunning[mIndex] = false;
                }
            }

        }
    }

    private class MyImageFrameAvailableListener implements ImageReader.OnImageAvailableListener {
        private int mIndex;

        public MyImageFrameAvailableListener(int index){
            mIndex = index;
        }
        @Override
        public void onImageAvailable(ImageReader imageReader) {
            try {
                Image image = imageReader.acquireNextImage();
                //这边添加取流buffer
                if (image != null) {
                    HardwareBuffer hardwareBuffer = image.getHardwareBuffer(); // 生成 HardwareBuffer
                    Message message = mProcessHandler.obtainMessage();
                    message.what = (1 << 8) | mIndex;
                    message.obj = hardwareBuffer;
                    mProcessHandler.sendMessage(message);
                    image.close();
                }
            } catch (Exception e) {
                Log.e(TAG, "image close:" + e);
            } finally {

            }
        }
    }

    private boolean isAllElementsFilled(HardwareBuffer[] hardwarebufferarray) {
        for (HardwareBuffer hardwarebuffer : hardwarebufferarray) {
            if (hardwarebuffer == null) {
                return false;
            }
        }
        return true;
    }

    private class MyCamStateCallback extends CameraDevice.StateCallback {
        private int mIndex;
        public MyCamStateCallback(int index){
            mIndex = index;
        }
        @Override
        public void onOpened(@NonNull CameraDevice cameraDevice) {
            Log.d(TAG, "camera " + mIndex + " opened");

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

    private class MyPreCaptureCallback extends CameraCaptureSession.CaptureCallback {
        private int mIndex;

        public MyPreCaptureCallback(int index) {
            mIndex = index;
        }

        @Override
        public void onCaptureCompleted(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull TotalCaptureResult result) {
            super.onCaptureCompleted(session, request, result);

        }

        @Override
        public void onCaptureFailed(@NonNull CameraCaptureSession session, @NonNull CaptureRequest request, @NonNull CaptureFailure failure) {
            super.onCaptureFailed(session, request, failure);

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
            try {
                CaptureRequest request = mPreviewBuilder[mIndex].build();
                cameraCaptureSession.setRepeatingRequest(request, new MyPreCaptureCallback(mIndex), mCameraHanlder);
                mCameraCaptureSession[mIndex] = cameraCaptureSession;
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

        @Override
        public void onClosed (@NonNull CameraCaptureSession session) {
            super.onClosed(session);
            mCameraCaptureSession[mIndex] = null;
        }

    }


    private void startCameraApi2(int index) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    != PackageManager.PERMISSION_GRANTED || checkSelfPermission(Manifest.permission.CAMERA)
                    != PackageManager.PERMISSION_GRANTED)
                return;
        }
        Log.d(TAG, "camera " + index + " start open");
        mCameraStatus[index] = CAMERA_OPENNING;
        try {
            StreamConfigurationMap map = mCameraManager.getCameraCharacteristics(mCameraIds[index]).get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);

            Size adaptsize = getMatchingSize(map);
            Log.d(TAG,"adaptsize = "+adaptsize);

            mImageReader[index] = ImageReader.newInstance(adaptsize.getWidth(),
                    adaptsize.getHeight(), ImageFormat.JPEG, 2);
            Log.d(TAG, "jpeg surface:" + mImageReader[index].getSurface());
            mImageReaderFrame[index] = ImageReader.newInstance(adaptsize.getWidth(), adaptsize.getHeight(), ImageFormat.YUV_420_888, 3);
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

    private Size getMatchingSize(StreamConfigurationMap map){
        Size selectSize = new Size(1920, 1080);
        Size bestSize = null;
        try {
            Size[] sizes = map.getOutputSizes(ImageFormat.JPEG);
            DisplayMetrics displayMetrics = getResources().getDisplayMetrics(); //因为我这里是将预览铺满屏幕,所以直接获取屏幕分辨率
            int deviceWidth = displayMetrics.widthPixels; //屏幕分辨率宽
            int deviceHeigh = displayMetrics.heightPixels; //屏幕分辨率高
            int minSizeDiff = Integer.MAX_VALUE;
            Log.d(TAG, "getMatchingSize: deviceWidth=" + deviceWidth);
            Log.d(TAG, "getMatchingSize: deviceHeigh=" + deviceHeigh);
            if (sizes == null){
                Log.e(TAG,"getMatchingSize sizes = null !");
                return selectSize;
            }
            for (Size size : sizes) {
                int sizeDiff = Math.abs(size.getWidth() - deviceWidth) +
                        Math.abs(size.getHeight() - deviceHeigh);
                if (sizeDiff < minSizeDiff) {
                    minSizeDiff = sizeDiff;
                    bestSize = size;
                }
            }
            if (bestSize != null) {
                Log.d(TAG, "getMatchingSize: bestSize.getWidth()="+bestSize.getWidth());
                Log.d(TAG, "getMatchingSize: bestSize.getHeight()="+bestSize.getHeight());
                return bestSize;
            }
        } catch (Exception e) {
            Log.e(TAG, "getMatchingSize error");
            e.printStackTrace();
        }
        Log.d(TAG,"without bestSize , return 1920x1080");
        return selectSize;
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


    private void startPreview(int index) {
        if (mCameraDevice[index] == null) return;
        Log.d(TAG, "start preview " + index);
        try {
            mCameraStatus[index] =  CAMERA_PREVIEWING;

            mPreviewBuilder[index] = mCameraDevice[index].createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);

            mPreviewBuilder[index].addTarget(mImageReaderFrame[index].getSurface());

            Log.d(TAG, "createCaptureSession for frame capture only");

            mCameraDevice[index].createCaptureSession(Collections.singletonList(mImageReaderFrame[index].getSurface()),
                    new MyCaptureSessionCallback(index), mCameraHanlder);
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

    public class SkiaDrawView extends View {

        public SkiaDrawView(Context ctx) {
            super(ctx);
        }
        public SkiaDrawView(Context context, AttributeSet attrs){
            super(context, attrs);
        }

        private int mX,mY,moveX,moveY;

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            Log.d(TAG,"onSizeChanged");
            // Create a bitmap for skia to draw into
            bitmp_w = w;
            bitmp_h = h;

            String debug = String.format("onSizeChanged w %d, h %d oldw %d oldh %d",w,h,oldw,oldh);
            Log.d("MyApplication", debug);

            fSkiaBitmap = Bitmap.createBitmap(bitmp_w, bitmp_h, Bitmap.Config.RGB_565);
            if(algo_running == false){
                algo_running = true;
            }
        }

        @Override
        protected void onDraw(Canvas canvas) {
            // Call into our C++ code that renders to the bitmap using Skia
            if(algo_running && fSkiaBitmap != null){
                mCameraJNI.nativeSetBitmap(fSkiaBitmap, SystemClock.elapsedRealtime());
                // Present the bitmap on the screen
                canvas.drawBitmap(fSkiaBitmap, 0, 0, null);
            }
        }

        @Override
        public boolean onTouchEvent(MotionEvent event) {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN://手指按下
                    //按下的时候获取手指触摸的坐标
                    mX = (int) event.getRawX();
                    mY = (int) event.getRawY();
                    break;
                case MotionEvent.ACTION_MOVE://手指滑动
                    moveX = (int) event.getRawX();
                    moveY = (int) event.getRawY();
                    mCameraJNI.onScreenTouch(mX, mY, moveX, moveY);
                    break;
                case MotionEvent.ACTION_UP://手指松开
                    break;
            }
            return true;
        }
    }
}