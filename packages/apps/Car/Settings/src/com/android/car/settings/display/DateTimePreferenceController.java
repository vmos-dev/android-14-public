/*
 * Copyright (c) 2023 Rockchip Electronics Co., Ltd
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

package com.android.car.settings.display;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.os.UserManager;

import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;

/**
 * Concrete implementation of {@link PreferenceController} which allows for applying the default
 * {@link #getAvailabilityStatus()} and {@link #onApplyUxRestrictions(CarUxRestrictions)}
 * behavior to preferences which do not require additional controller logic.
 */
public final class DateTimePreferenceController extends
        PreferenceController<Preference> {

    private final UserManager mUserManager;

    public DateTimePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mUserManager = UserManager.get(context);
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    @Override
    @AvailabilityStatus
    protected int getDefaultAvailabilityStatus() {
        return mUserManager.isAdminUser() ? AVAILABLE : DISABLED_FOR_PROFILE;
    }
}
