/*
 * Copyright (C) 2018 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.settings.biometrics.face;

import android.app.Activity;
import android.app.settings.SettingsEnums;
import android.hardware.face.FaceManager;
import android.view.Surface;
import com.android.settings.biometrics.BiometricEnrollSidecar;

import java.util.Arrays;

/**
 * Sidecar fragment to handle the state around face enrollment
 */
public class FaceEnrollSidecar extends BiometricEnrollSidecar implements FacePreviewListener{
    private final String TAG = "FaceEnrollSidecar";
    private final int[] mDisabledFeatures;

    private FaceUpdater mFaceUpdater;

    public FaceEnrollSidecar(int[] disabledFeatures) {
        mDisabledFeatures = Arrays.copyOf(disabledFeatures, disabledFeatures.length);
    }

    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        mFaceUpdater = new FaceUpdater(activity);
    }

    @Override
    public void startEnrollment() {
        super.startEnrollment();
//        mFaceUpdater.enroll(mUserId, mToken, mEnrollmentCancel,
//                mEnrollmentCallback, mDisabledFeatures);
    }

    private FaceManager.EnrollmentCallback mEnrollmentCallback
            = new FaceManager.EnrollmentCallback() {

        @Override
        public void onEnrollmentProgress(int remaining) {
            FaceEnrollSidecar.super.onEnrollmentProgress(remaining);
        }

        @Override
        public void onEnrollmentHelp(int helpMsgId, CharSequence helpString) {
            FaceEnrollSidecar.super.onEnrollmentHelp(helpMsgId, helpString);
        }

        @Override
        public void onEnrollmentError(int errMsgId, CharSequence errString) {
            FaceEnrollSidecar.super.onEnrollmentError(errMsgId, errString);
        }
    };

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.FACE_ENROLL_SIDECAR;
    }

    @Override
    public void onPreviewCreate(Surface surface) {
        mFaceUpdater.enroll(mUserId, mToken, mEnrollmentCancel,
                mEnrollmentCallback, mDisabledFeatures,surface,false);
    }
}
