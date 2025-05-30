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
package android.sdksandbox.test.scenario.sampleclient;

import android.os.SystemClock;
import android.platform.test.scenario.annotation.Scenario;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

@Scenario
@RunWith(JUnit4.class)
public class RemoteRenderAd {

    private static final int NUMBER_OF_ADS = 3;
    private static final int TIME_BETWEEN_RENDERS_S = 2;
    private SdkSandboxTestHelper mSdkSandboxTestHelper = new SdkSandboxTestHelper();

    @AfterClass
    public static void tearDown() throws IOException {
        SdkSandboxTestHelper.closeClientApp();
    }

    @Before
    public void setup() throws Exception {
        mSdkSandboxTestHelper.openClientApp();
        mSdkSandboxTestHelper.loadSandboxSdk();
    }

    @Test
    public void testRemoteRenderAd() {

        // Loop to remote render ad multiple times sequentially
        for (int i = 0; i < NUMBER_OF_ADS; i++) {
            mSdkSandboxTestHelper.remoteRenderAd();
            SystemClock.sleep(TimeUnit.SECONDS.toMillis(TIME_BETWEEN_RENDERS_S));
        }
        SystemClock.sleep(TimeUnit.SECONDS.toMillis(2));
    }
}
