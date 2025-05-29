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
 *
 */

package com.android.customization.model.picker.quickaffordance.data.repository

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.filters.SmallTest
import com.android.customization.picker.quickaffordance.data.repository.KeyguardQuickAffordancePickerRepository
import com.android.systemui.shared.customization.data.content.CustomizationProviderClient
import com.android.systemui.shared.customization.data.content.FakeCustomizationProviderClient
import com.android.wallpaper.config.BaseFlags
import com.google.common.truth.Truth.assertThat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.UnconfinedTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@OptIn(ExperimentalCoroutinesApi::class)
@SmallTest
@RunWith(RobolectricTestRunner::class)
class KeyguardQuickAffordancePickerRepositoryTest {

    private lateinit var underTest: KeyguardQuickAffordancePickerRepository

    private lateinit var testScope: TestScope
    private lateinit var client: FakeCustomizationProviderClient

    @Before
    fun setUp() {
        client = FakeCustomizationProviderClient()
        val coroutineDispatcher = UnconfinedTestDispatcher()
        testScope = TestScope(coroutineDispatcher)
        Dispatchers.setMain(coroutineDispatcher)

        underTest =
            KeyguardQuickAffordancePickerRepository(
                client = client,
                scope = testScope.backgroundScope,
                flags =
                    object : BaseFlags() {
                        override fun getCachedFlags(
                            context: Context
                        ): List<CustomizationProviderClient.Flag> {
                            return runBlocking { client.queryFlags() }
                        }
                    },
                context = ApplicationProvider.getApplicationContext()
            )
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    // We need at least one test to prevent Studio errors
    @Test
    fun creationSucceeds() {
        assertThat(underTest).isNotNull()
    }
}
