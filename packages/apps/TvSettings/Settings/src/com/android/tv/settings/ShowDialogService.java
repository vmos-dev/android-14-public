package com.android.tv.settings;

import android.app.Dialog;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.Nullable;

/**
 * @author Weng Tao
 */
public class ShowDialogService extends Service {
    private static final String TAG = "ShowDialogService";

    public static final String KEY_DIALOG = "dialog";
    public static final String VALUE_AI_LAB = "AiLabDialog";

    private Dialog mDialog;

    public ShowDialogService() {
        Log.d(TAG, "wttt " + "ShowDialogService() called");
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "wttt " + "onCreate() called");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG,
                "wttt " + "onStartCommand() called with: intent = [" + intent + "], flags = [" + flags
                        + "], startId = [" + startId + "]");
        if (intent != null && intent.getExtras() != null) {
            String passOnStr = (String) intent.getExtras().get(KEY_DIALOG);
            Log.d(TAG, "onBind: passOnStr = " + passOnStr);
            if (!TextUtils.isEmpty(passOnStr) && passOnStr.equals(VALUE_AI_LAB)) {
                if (mDialog != null && mDialog.isShowing()) {
                    Log.d(TAG, "onStartCommand: mDialog is showing");
                    mDialog.dismiss();
                }
                mDialog = new AiLabDialog(getApplication(), R.style.transparent_dialog);
                mDialog.show();
            }
        }
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.d(TAG, "wttt " + "onDestroy() called");
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "wttt " + "onBind() called with: intent = [" + intent + "]");
        return null;
    }
}
