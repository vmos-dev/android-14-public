/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tv.settings.connectivity.setup;

import android.content.Context;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.leanback.widget.GuidanceStylist;
import androidx.leanback.widget.GuidedAction;
import androidx.leanback.widget.GuidedActionsStylist;
import androidx.lifecycle.ViewModelProviders;

import com.android.tv.settings.R;
import com.android.tv.settings.connectivity.util.State;
import com.android.tv.settings.connectivity.util.StateMachine;

import java.util.List;

/**
 * State responsible for entering ssid.
 */
public class EnterSsidState implements State {
    private final FragmentActivity mActivity;
    private Fragment mFragment;

    public EnterSsidState(FragmentActivity activity) {
        mActivity = activity;
    }

    @Override
    public void processForward() {
        mFragment = new EnterSsidFragment();
        FragmentChangeListener listener = (FragmentChangeListener) mActivity;
        if (listener != null) {
            listener.onFragmentChange(mFragment, true);
        }
    }

    @Override
    public void processBackward() {
        mFragment = new EnterSsidFragment();
        FragmentChangeListener listener = (FragmentChangeListener) mActivity;
        if (listener != null) {
            listener.onFragmentChange(mFragment, false);
        }
    }

    @Override
    public Fragment getFragment() {
        return mFragment;
    }

    /**
     * Fragment that needs user to enter SSID of the network.
     */
    public static class EnterSsidFragment extends WifiConnectivityGuidedStepFragment {
        private UserChoiceInfo mUserChoiceInfo;
        private StateMachine mStateMachine;
        private GuidedAction mAction;

        @NonNull
        @Override
        public GuidanceStylist.Guidance onCreateGuidance(Bundle savedInstanceState) {
            return new GuidanceStylist.Guidance(
                    getString(R.string.title_ssid),
                    null,
                    null,
                    null);
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            mUserChoiceInfo = ViewModelProviders
                    .of(getActivity())
                    .get(UserChoiceInfo.class);
            mStateMachine = ViewModelProviders
                    .of(getActivity())
                    .get(StateMachine.class);
            super.onCreate(savedInstanceState);
        }

        @Override
        public GuidedActionsStylist onCreateActionsStylist() {
            return new GuidedActionsStylist() {
                @Override
                public int onProvideItemLayoutId() {
                    return R.layout.setup_text_input_item;
                }
            };
        }

        @Override
        public void onCreateActions(@NonNull List<GuidedAction> actions,
                Bundle savedInstanceState) {
            Context context = getActivity();
            CharSequence prevSsid = mUserChoiceInfo.getPageSummary(UserChoiceInfo.SSID);
            mAction = new GuidedAction.Builder(context)
                    .editable(true)
                    .title(prevSsid == null ? "" : prevSsid)
                    .build();
            actions.add(mAction);
        }

        @Override
        public void onResume() {
            super.onResume();
            openInEditMode(mAction);
        }

        @Override
        public void onViewCreated(@NonNull View view, Bundle savedInstanceState) {
            // openInEditMode(mAction);
        }

        @Override
        public long onGuidedActionEditedAndProceed(GuidedAction action) {
            final String ssid = action.getTitle().toString();
            if (TextUtils.isEmpty(ssid)) {
                return GuidedAction.ACTION_ID_CURRENT;
            }
            mUserChoiceInfo.put(UserChoiceInfo.SSID, ssid);
            mStateMachine.getListener().onComplete(StateMachine.CONTINUE);
            return action.getId();
        }
    }
}
