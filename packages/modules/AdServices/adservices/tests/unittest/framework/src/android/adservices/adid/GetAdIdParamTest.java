/*
 * Copyright (C) 2022 The Android Open Source Project
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
package android.adservices.adid;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import android.os.Parcel;

import androidx.test.filters.SmallTest;

import org.junit.Test;

/** Unit tests for {@link GetAdIdParam} */
@SmallTest
public final class GetAdIdParamTest {
    private static final String SOME_PACKAGE_NAME = "SomePackageName";
    private static final String SOME_SDK_PACKAGE_NAME = "SomeSdkPackageName";

    @Test
    public void test_nonNull() {
        GetAdIdParam request =
                new GetAdIdParam.Builder()
                        .setAppPackageName(SOME_PACKAGE_NAME)
                        .setSdkPackageName(SOME_SDK_PACKAGE_NAME)
                        .build();

        assertThat(request.getSdkPackageName()).isEqualTo(SOME_SDK_PACKAGE_NAME);
        assertThat(request.getAppPackageName()).isEqualTo(SOME_PACKAGE_NAME);

        // no file descriptor marshalling.
        assertThat(request.describeContents()).isEqualTo(0);

        Parcel parcel = Parcel.obtain();
        request.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);

        GetAdIdParam createdRequest = GetAdIdParam.CREATOR.createFromParcel(parcel);
        assertEquals(false, createdRequest == request);
        assertEquals(GetAdIdParam.CREATOR.newArray(1).length, 1);
    }

    @Test
    public void test_nullAppPackageName_throwsIllegalArgumentException() {
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    GetAdIdParam unusedRequest =
                            new GetAdIdParam.Builder()
                                    // Not setting AppPackagename making it null.
                                    .build();
                });

        // Null AppPackageName.
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    GetAdIdParam unusedRequest =
                            new GetAdIdParam.Builder().setAppPackageName(null).build();
                });
    }

    @Test
    public void test_notSettingAppPackageName_throwsIllegalArgumentException() {
        // Empty AppPackageName.
        assertThrows(
                IllegalArgumentException.class,
                () -> {
                    GetAdIdParam unusedRequest =
                            new GetAdIdParam.Builder().setAppPackageName("").build();
                });
    }
}
