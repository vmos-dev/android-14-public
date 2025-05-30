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
 * limitations under the License
 */

package com.android.tv.settings.name;

import android.app.Activity;
import android.app.FragmentManager;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentTransaction;
import androidx.leanback.app.GuidedStepSupportFragment;
import androidx.leanback.widget.GuidanceStylist;
import androidx.leanback.widget.GuidedAction;
import androidx.leanback.widget.GuidedActionsStylist;

import com.android.tv.settings.R;
import com.android.tv.settings.name.setup.DeviceNameFlowStartActivity;
import com.android.tv.settings.util.AccessibilityHelper;
import com.android.tv.settings.util.GuidedActionsAlignUtil;

import java.util.List;

/**
 * Fragment responsible for adding new device name.
 */
public class DeviceNameSetCustomFragment extends GuidedStepSupportFragment {

    private GuidedAction mEditAction;

    public static DeviceNameSetCustomFragment newInstance() {
        return new DeviceNameSetCustomFragment();
    }

    @Override
    public GuidanceStylist onCreateGuidanceStylist() {
        return GuidedActionsAlignUtil.createGuidanceStylist();
    }

    @Override
    public GuidedActionsStylist onCreateActionsStylist() {
        return new GuidedActionsStylist() {
            @Override
            public int onProvideItemLayoutId() {
                return R.layout.guided_step_input_action;
            }
            @Override
            protected void setupImeOptions(GuidedActionsStylist.ViewHolder vh,
                    GuidedAction action) {
                // keep defaults
            }
        };
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        return GuidedActionsAlignUtil.createView(view, this);
    }

    @NonNull
    @Override
    public GuidanceStylist.Guidance onCreateGuidance(Bundle savedInstanceState) {
        return new GuidanceStylist.Guidance(
                getString(R.string.select_device_name_title, Build.MODEL),
                getString(R.string.select_device_name_description),
                null,
                null);
    }

    @Override
    public void onCreateActions(@NonNull List<GuidedAction> actions, Bundle savedInstanceState) {
        mEditAction = new GuidedAction.Builder(getContext())
                .title("")
                .editable(true)
                .build();
        actions.add(mEditAction);
    }

    @Override
    public void onResume() {
        super.onResume();
        // openInEditMode(mEditAction);
    }

    // Overriding this method removes the unpreferable enter transition animation of this fragment.
    @Override
    protected void onProvideFragmentTransitions() {
        setEnterTransition(null);
    }

    @Override
    public long onGuidedActionEditedAndProceed(GuidedAction action) {
        final CharSequence name = action.getTitle();
        if (TextUtils.isGraphic(name)) {
            DeviceManager.setDeviceName(getActivity(), name.toString());
            getActivity().setResult(Activity.RESULT_OK);

            // Set the flag for the appropriate exit animation for setup.
            if (getActivity() instanceof DeviceNameFlowStartActivity) {
                ((DeviceNameFlowStartActivity) getActivity()).setResultOk(true);
            }
            DeviceNameSuggestionStatus.getInstance(
                    getActivity().getApplicationContext()).setFinished();
            getActivity().finish();
            return super.onGuidedActionEditedAndProceed(action);
        } else {
            popBackStackToGuidedStepSupportFragment(
                    DeviceNameSetCustomFragment.class, FragmentManager.POP_BACK_STACK_INCLUSIVE);
            return GuidedAction.ACTION_ID_CANCEL;
        }
    }

    @Override
    protected void onAddSharedElementTransition(
            FragmentTransaction ft, GuidedStepSupportFragment disappearing) {
        // no-op
    }

    @Override
    public void onGuidedActionEditCanceled(GuidedAction action) {
        // We need to ensure the IME is closed before navigating back. See b/233207859.
        AccessibilityHelper.dismissKeyboard(getActivity(), getView());

        // We need to "pop to" current fragment with INCLUSIVE flag instead of popping to previous
        // fragment because DeviceNameSetFragment was set to be root and not added on backstack.
        popBackStackToGuidedStepSupportFragment(
                DeviceNameSetCustomFragment.class, FragmentManager.POP_BACK_STACK_INCLUSIVE);
    }
}
