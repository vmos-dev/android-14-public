/*
 * Copyright (C) 2023 The Android Open Source Project
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

package android.car.app;

import android.annotation.NonNull;
import android.car.annotation.ApiRequirements;

/**
 * A callback interface for {@link RemoteCarDefaultRootTaskView}.
 *
 * @hide
 */
public interface RemoteCarDefaultRootTaskViewCallback
        extends RemoteCarTaskViewCallback<RemoteCarDefaultRootTaskView> {
    /**
     * Called when the underlying {@link RemoteCarDefaultRootTaskView} instance is created.
     *
     * @param taskView the new newly created {@link RemoteCarDefaultRootTaskView} instance.
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_1,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_1)
    @Override
    default void onTaskViewCreated(@NonNull RemoteCarDefaultRootTaskView taskView) {}

    /**
     * Called when the underlying {@link RemoteCarDefaultRootTaskView} is ready. A
     * {@link RemoteCarDefaultRootTaskView} is considered ready when it has completed all the
     * set up that is required.  This callback is only triggered once.
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_1,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_1)
    @Override
    default void onTaskViewInitialized() {}

    /**
     * Called when the underlying {@link RemoteCarDefaultRootTaskView} is released.
     * This callback is only triggered once.
     */
    @ApiRequirements(minCarVersion = ApiRequirements.CarVersion.UPSIDE_DOWN_CAKE_1,
            minPlatformVersion = ApiRequirements.PlatformVersion.UPSIDE_DOWN_CAKE_1)
    @Override
    default void onTaskViewReleased() {}
}
