/*
 * Copyright 2023 Rockchip Electronics S.LSI Co. LTD
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

package com.android.settings.display;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import androidx.fragment.app.DialogFragment;
import java.io.Serializable;
import com.android.settings.R;

public class ConfirmSetModeDialogFragment extends DialogFragment {

    private OnDialogDismissListener mListener;

    public static ConfirmSetModeDialogFragment newInstance(DisplayInfo di, OnDialogDismissListener listener) {
        ConfirmSetModeDialogFragment frag = new ConfirmSetModeDialogFragment();
        Bundle b = new Bundle();
        b.putSerializable("displayinfo", di);
        b.putSerializable("listener", listener);
        frag.setArguments(b);
        return frag;
    }

    private AlertDialog mHdmiResoSetConfirmDialog;
    private boolean mIsOk = false;

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mDisplayInfo = (DisplayInfo) getArguments().getSerializable("displayinfo");
        mListener = (OnDialogDismissListener) getArguments().getSerializable("listener");
        mHdmiResoSetConfirmDialog = new AlertDialog.Builder(getActivity())
                .setTitle(getString(R.string.confirm_dialog_title))
                .setMessage(getString(R.string.confirm_dialog_message) + " 10s")
                .setPositiveButton(getString(R.string.okay), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mIsOk = true;
                        mHdmiResoSetConfirmDialog.dismiss();
                    }
                }).setNegativeButton(getString(R.string.cancel), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mIsOk = false;
                        mHdmiResoSetConfirmDialog.dismiss();
                    }
                }).create();
        startTimeCount();
        return mHdmiResoSetConfirmDialog;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        stopTimeCount();
        //DrmDisplaySetting.confirmSaveDisplayMode(mDisplayInfo, mIsOk);
        mListener.onDismiss(mIsOk);
        mIsOk = false;
    }

    private DisplayInfo mDisplayInfo;

    private static final int TYPE_NEGATIVE = 0;

    private int mNegativeCount = 10;
    private boolean isTimeCountRunning = false;

    public void startTimeCount() {
        mNegativeCount = 10;
        isTimeCountRunning = true;
        myHander.sendEmptyMessageDelayed(TYPE_NEGATIVE, 1000);
    }

    public void stopTimeCount() {
        isTimeCountRunning = false;
    }
    private Handler myHander = new Handler() {
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case TYPE_NEGATIVE:
                    if (!isTimeCountRunning) {
                        return;
                    }
                    if (mNegativeCount > 0) {
                        if (mHdmiResoSetConfirmDialog != null) {
                            String text = getString(R.string.confirm_dialog_message);
                            text = text + " " + mNegativeCount + "s";
                            mHdmiResoSetConfirmDialog.setMessage(text);
                        }
                        mNegativeCount--;
                        myHander.sendEmptyMessageDelayed(TYPE_NEGATIVE, 1000);
                    } else {
                        DrmDisplaySetting.confirmSaveDisplayMode(mDisplayInfo, false);
                        mHdmiResoSetConfirmDialog.dismiss();
                    }
                    break;
            }
        }};
    public interface OnDialogDismissListener extends Serializable {
        void onDismiss(boolean isok);
    }
}
