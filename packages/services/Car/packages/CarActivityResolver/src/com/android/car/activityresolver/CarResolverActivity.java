/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.car.activityresolver;

import android.os.Bundle;
import android.os.UserHandle;
import android.view.View;
import android.view.ViewTreeObserver;
import android.widget.ListView;
import java.util.List;
import com.android.internal.R;
import com.android.internal.app.ResolverActivity;
import com.android.internal.app.ResolverViewPager;

/**
 * An automotive variant of the resolver activity which does not use the safe forwarding mode and
 * which supports rotary.
 */
public final class CarResolverActivity extends ResolverActivity
        implements ViewTreeObserver.OnGlobalLayoutListener {

    private ResolverViewPager mProfilePager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        ENABLE_TABBED_VIEW = false;
        super.onCreate(savedInstanceState);

        setSafeForwardingMode(false);

        mProfilePager = findViewById(R.id.profile_pager);
        mProfilePager.getViewTreeObserver().addOnGlobalLayoutListener(this);
        mProfileView = null;
    }

    @Override
    protected void onDestroy() {
        mProfilePager.getViewTreeObserver().removeOnGlobalLayoutListener(this);

        super.onDestroy();
    }

    @Override
    public void onGlobalLayout() {
        ListView listView = findViewById(R.id.resolver_list);
        if (listView != null) {
            // Items must be focusable for rotary.
            listView.setItemsCanFocus(true);

            // Set click listeners for rotary.
            for (int i = 0; i < listView.getChildCount(); i++) {
                View element = listView.getChildAt(i);
                element.setOnClickListener(view -> {
                    int position = listView.getPositionForView(view);
                    long id = listView.getItemIdAtPosition(position);
                    listView.performItemClick(view, position, id);
                });
            }
        }
    }

    protected UserHandle getPersonalProfileUserHandle() {
        return UserHandle.of(UserHandle.myUserId());
    }

    @Override // ResolverListCommunicator
    public void updateProfileViewButton() {
        android.util.Log.i(CarResolverActivity.class.getSimpleName(), "updateProfileViewButton is null");
        if (mProfileView != null) {
            mProfileView.setVisibility(View.GONE);
        }
    }
}
