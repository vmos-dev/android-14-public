/*
 * Copyright (C) 2012 The Android Open Source Project
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

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.util.Size;
import android.view.TextureView;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import com.android.camera.QrCamera;
import com.android.camera.QrDecorateView;
import com.android.camera2.R;
import java.net.MalformedURLException;
import java.net.URL;

public class RKScannerQRCodeActivity extends Activity implements
        TextureView.SurfaceTextureListener,
        QrCamera.ScannerCallback {
    private static final String TAG = "RKScannerQRCodeActivity";

    private QrCamera mCamera;
    private TextureView mTextureView;
    private QrDecorateView mDecorateView;
    private TextView mQrResultTextView, mQrActionTextView;
    private Button mQrActionBtn;
    private LinearLayout mBottomLinearLayout;
    private String mCodeResult;
    private int mCodeResultState;
    private final static int CODE_RESULT_ACTION_OPEN_URL = 0;
    private final static int CODE_RESULT_ACTION_COPY_TEXT = 1;
    private final static int CODE_RESULT_ACTION_ERROR = 2;
    public static boolean isLandscape(Context context) {
        Configuration configuration = context.getResources().getConfiguration();
        return configuration.orientation == Configuration.ORIENTATION_LANDSCAPE;
    }

    private Handler mHandler = new Handler()
    {
        public void handleMessage(Message msg)
        {
            if (msg.what == CODE_RESULT_ACTION_OPEN_URL) {
                mBottomLinearLayout.setVisibility(View.VISIBLE);
                mQrActionTextView.setText(getString(R.string.qr_code_action_text));
                mQrResultTextView.setText((String)msg.obj);
                mQrActionBtn.setVisibility(View.VISIBLE);
                mQrActionBtn.setText(getString(R.string.qr_action_button_text));
            } else if (msg.what == CODE_RESULT_ACTION_COPY_TEXT) {
                mBottomLinearLayout.setVisibility(View.VISIBLE);
                mQrActionTextView.setText(getString(R.string.bar_code_action_text));
                mQrResultTextView.setText((String)msg.obj);
                mQrActionBtn.setVisibility(View.VISIBLE);
                mQrActionBtn.setText(getString(R.string.bar_action_button_text));
            } else if (msg.what == CODE_RESULT_ACTION_ERROR) {
                mBottomLinearLayout.setVisibility(View.INVISIBLE);
                Toast.makeText(getBaseContext(), getString(R.string.toast_scan_error), Toast.LENGTH_SHORT).show();
            }
        }
    };

    @Override
    protected void onCreate(Bundle bundle) {
        super.onCreate(bundle);
        setContentView(R.layout.activity_scanner_qr_code);

        mTextureView = (TextureView) findViewById(R.id.preview_view);
        mDecorateView = (QrDecorateView) findViewById(R.id.decorate_view);
        mBottomLinearLayout = (LinearLayout) findViewById(R.id.bottm_view);
        mQrResultTextView = (TextView) findViewById(R.id.result_view);
        mQrActionTextView = (TextView) findViewById(R.id.action_view);
        mQrActionBtn = (Button) findViewById(R.id.action_button);
        mQrActionBtn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mCodeResultState == CODE_RESULT_ACTION_OPEN_URL) {
                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(mCodeResult));
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    try {
                        startActivity(intent);
                    } catch (ActivityNotFoundException e) {
                        Toast.makeText(getBaseContext(), getString(R.string.toast_no_browser_on_device), Toast.LENGTH_SHORT).show();
                    }
                } else if (mCodeResultState == CODE_RESULT_ACTION_COPY_TEXT) {
                    ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clip = ClipData.newPlainText("label", mCodeResult);
                    clipboard.setPrimaryClip(clip);
                    Toast.makeText(getBaseContext(), getString(R.string.toast_copy_success), Toast.LENGTH_SHORT).show();
                }
                initCamera();
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (mTextureView.isAvailable()) {
            Log.d(TAG, "isAvailable: ");
            initCamera();
        } else {
            mTextureView.setSurfaceTextureListener(this);
        }
        mBottomLinearLayout.setVisibility(View.INVISIBLE);
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause");
        destroyCamera();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        destroyCamera();
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width, int height) {
        initCamera();
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width, int height) {

    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        destroyCamera();
        return false;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {

    }

     @Override
    public Size getViewSize() {
        return new Size(mTextureView.getWidth(), mTextureView.getHeight());
    }

    @Override
    public Rect getFramePosition(Size previewSize, int cameraOrientation) {
        Rect rect = new Rect();
        if (isLandscape(this)) {
            rect = new Rect(0, 0, previewSize.getWidth(), previewSize.getHeight());
        } else {
            rect = new Rect(0, 0, previewSize.getHeight(), previewSize.getWidth());
        }
        return rect;
    }

    @Override
    public void setTransform(Matrix transform) {
        mTextureView.setTransform(transform);
    }

    @Override
    public boolean isValid(String qrCode) {
        return true;
    }

    /**
     * This method is only called when QrCamera.ScannerCallback.isValid returns true;
     */
    @Override
    public void handleSuccessfulResult(String qrCode) {
        Log.d(TAG, "qrCode:" + qrCode);
        Message message = mHandler.obtainMessage();
        if (isURL(qrCode)) {
            mCodeResult = qrCode;
            mCodeResultState = CODE_RESULT_ACTION_OPEN_URL;
            message.obj = qrCode;
            message.what = CODE_RESULT_ACTION_OPEN_URL;
            mHandler.sendMessage(message);
        } else {
            if (isText(qrCode)) {
                mCodeResult = qrCode;
                mCodeResultState = CODE_RESULT_ACTION_COPY_TEXT;
                message.obj = qrCode;
                message.what = CODE_RESULT_ACTION_COPY_TEXT;
                mHandler.sendMessage(message);
            } else {
                mCodeResultState = CODE_RESULT_ACTION_ERROR;
                message.what = CODE_RESULT_ACTION_ERROR;
                mHandler.sendMessage(message);
            }
        }
        destroyCamera();
    }

    @Override
    public void handleCameraFailure() {
        destroyCamera();
    }

    private void initCamera() {
        Log.d(TAG, "initCamera");
        // Check if the camera has already created.
        if (mCamera == null) {
            mCamera = new QrCamera(getBaseContext(), this);
            mCamera.start(mTextureView);
        }
    }

    private void destroyCamera() {
        Log.d(TAG, "destroyCamera");
        if (mCamera != null) {
            mCamera.stop();
            mCamera = null;
        }
    }

    private boolean isURL(String input) {
        try {
            new URL(input);
            return true;
        } catch (MalformedURLException e) {
            return false;
        }
    }

    private boolean isText(String input) {
        return input != null && !input.isEmpty();
    }
}
