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

package com.android.settings.ramextension;

import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;

import androidx.fragment.app.DialogFragment;

import com.android.settings.R;

import java.io.Serializable;

public class ConfirmDialogFragment extends DialogFragment {

    private AlertDialog mConfirmDialog;
    private OnDialogDismissListener mListener;
    private boolean mIsOk = false;

    public static ConfirmDialogFragment newInstance(OnDialogDismissListener listener) {
        ConfirmDialogFragment frag = new ConfirmDialogFragment();
        Bundle b = new Bundle();
        b.putSerializable("listener", listener);
        frag.setArguments(b);
        return frag;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mListener = (OnDialogDismissListener) getArguments().getSerializable("listener");
        mConfirmDialog = new AlertDialog.Builder(getActivity())
                .setTitle(getString(R.string.warning_button_text))
                .setMessage(getString(R.string.ram_extension_reboot_warn))
                .setPositiveButton(getString(R.string.ram_extension_reboot), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mIsOk = true;
                        mConfirmDialog.dismiss();
                    }
                }).setNegativeButton(getString(R.string.ram_extension_cancel), new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        mIsOk = false;
                        mConfirmDialog.dismiss();
                    }
                }).create();
        return mConfirmDialog;
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        super.onDismiss(dialog);
        mListener.onDismiss(mIsOk);
        mIsOk = false;
    }

    public interface OnDialogDismissListener extends Serializable {
        void onDismiss(boolean isok);
    }
}
