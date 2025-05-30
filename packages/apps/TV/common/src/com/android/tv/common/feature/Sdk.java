/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.tv.common.feature;

import android.content.Context;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

/** Holder for SDK version features */
public final class Sdk {

    public static final Feature AT_LEAST_N = new AtLeast(VERSION_CODES.N);

    public static final Feature AT_LEAST_O = new AtLeast(VERSION_CODES.O);

    public static final Feature AT_LEAST_T = new AtLeast(VERSION_CODES.TIRAMISU);

    private static final class AtLeast implements Feature {

        private final int versionCode;

        private AtLeast(int versionCode) {
            this.versionCode = versionCode;
        }

        @Override
        public boolean isEnabled(Context unused) {
            return VERSION.SDK_INT >= versionCode;
        }
    }

    private Sdk() {}
}
