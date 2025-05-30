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

package com.android.ondevicepersonalization.services.process;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.android.ondevicepersonalization.libraries.plugin.PluginInfo;

import com.google.common.collect.ImmutableList;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class ProcessUtilsTest {
    @Test
    public void testGetArchiveList_NullApkList() throws Exception {
        assertTrue(ProcessUtils.getArchiveList(null).isEmpty());
    }

    @Test
    public void testGetArchiveList() throws Exception {
        ImmutableList<PluginInfo.ArchiveInfo> result = ProcessUtils.getArchiveList("fakeApk");
        assertEquals(1, result.size());
        assertEquals("fakeApk", result.get(0).packageName());
    }
}
