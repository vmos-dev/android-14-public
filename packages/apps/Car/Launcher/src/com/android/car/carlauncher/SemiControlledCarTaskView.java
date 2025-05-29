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

package com.android.car.carlauncher;

import static android.app.WindowConfiguration.WINDOWING_MODE_MULTI_WINDOW;
import static android.view.Display.DEFAULT_DISPLAY;

import static com.android.car.carlauncher.TaskViewManager.DBG;

import android.app.Activity;
import android.app.ActivityManager;
import android.car.app.CarActivityManager;
import android.content.ComponentName;
import android.util.Log;
import android.view.SurfaceControl;
import android.window.WindowContainerTransaction;

import androidx.annotation.VisibleForTesting;

import com.android.wm.shell.ShellTaskOrganizer;
import com.android.wm.shell.common.SyncTransactionQueue;
import com.android.wm.shell.taskview.TaskViewTransitions;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicReference;

/**
 * A Semi-controlled {@link CarTaskView} is where the apps are meant to stay temporarily. It always
 * works when a {@link LaunchRootCarTaskView} has been set up.
 *
 * It serves these use-cases:
 * <ul>
 *     <li>Should be used when the apps that are meant to be in it can be started from anywhere
 *     in the system. i.e. when the host app has no control over their launching.</li>
 *     <li>Suitable for apps like Assistant or Setup-Wizard.</li>
 * </ul>
 */
final class SemiControlledCarTaskView extends CarTaskView {
    private static final String TAG = SemiControlledCarTaskView.class.getSimpleName();
    private final Executor mCallbackExecutor;
    private final SemiControlledCarTaskViewCallbacks mCallbacks;
    private final ShellTaskOrganizer mShellTaskOrganizer;
    private final SyncTransactionQueue mSyncQueue;
    private final LinkedHashMap<Integer, ActivityManager.RunningTaskInfo> mChildrenTaskStack =
            new LinkedHashMap<>();
    private final ArrayList<ComponentName> mAllowListedActivities;
    private final AtomicReference<CarActivityManager> mCarActivityManagerRef;

    private ActivityManager.RunningTaskInfo mRootTask;

    private final ShellTaskOrganizer.TaskListener mRootTaskListener =
            new ShellTaskOrganizer.TaskListener() {
                @Override
                public void onTaskAppeared(ActivityManager.RunningTaskInfo taskInfo,
                        SurfaceControl leash) {
                    // The first call to onTaskAppeared() is always for the root-task.
                    if (mRootTask == null && !taskInfo.hasParentTask()) {
                        setRootTask(taskInfo);
                        SemiControlledCarTaskView.this.dispatchTaskAppeared(taskInfo, leash);

                        CarActivityManager carAm = mCarActivityManagerRef.get();
                        if (carAm != null) {
                            carAm.setPersistentActivitiesOnRootTask(
                                    mAllowListedActivities,
                                    taskInfo.token.asBinder());
                        } else {
                            Log.wtf(TAG, "CarActivityManager is null, cannot call "
                                    + "setPersistentActivitiesOnRootTask " + taskInfo);
                        }
                        mCallbackExecutor.execute(() -> mCallbacks.onTaskViewReady());
                        return;
                    }

                    if (DBG) {
                        Log.d(TAG, "onTaskAppeared " + taskInfo.taskId + " - "
                                + taskInfo.baseActivity);
                    }

                    mChildrenTaskStack.put(taskInfo.taskId, taskInfo);
                }

                @Override
                public void onTaskInfoChanged(ActivityManager.RunningTaskInfo taskInfo) {
                    if (mRootTask != null && mRootTask.taskId == taskInfo.taskId) {
                        SemiControlledCarTaskView.this.dispatchTaskInfoChanged(taskInfo);
                    }
                    if (DBG) {
                        Log.d(TAG, "onTaskInfoChanged " + taskInfo.taskId + " - "
                                + taskInfo.baseActivity);
                    }
                    if (taskInfo.isVisible && mChildrenTaskStack.containsKey(taskInfo.taskId)) {
                        // Remove the task and insert again so that it jumps to the end of
                        // the queue.
                        mChildrenTaskStack.remove(taskInfo.taskId);
                        mChildrenTaskStack.put(taskInfo.taskId, taskInfo);
                    }
                }

                @Override
                public void onTaskVanished(ActivityManager.RunningTaskInfo taskInfo) {
                    if (mRootTask != null && mRootTask.taskId == taskInfo.taskId) {
                        SemiControlledCarTaskView.this.dispatchTaskVanished(taskInfo);
                    }
                    if (DBG) {
                        Log.d(TAG, "onTaskVanished " + taskInfo.taskId + " - "
                                + taskInfo.baseActivity);
                    }
                    if (mChildrenTaskStack.containsKey(taskInfo.taskId)) {
                        mChildrenTaskStack.remove(taskInfo.taskId);
                    }
                }

                @Override
                public void onBackPressedOnTaskRoot(ActivityManager.RunningTaskInfo taskInfo) {
                    if (mChildrenTaskStack.size() == 0) {
                        Log.d(TAG, "Root task is empty, do nothing.");
                        return;
                    }

                    ActivityManager.RunningTaskInfo topTask = getTopTaskInTheRootTask();
                    WindowContainerTransaction wct = new WindowContainerTransaction();
                    // removeTask() will trigger onTaskVanished which will remove the task locally
                    // from mChildrenTaskStack
                    wct.removeTask(topTask.token);
                    mSyncQueue.queue(wct);
                }
            };

    public SemiControlledCarTaskView(Activity context,
            ShellTaskOrganizer organizer,
            TaskViewTransitions taskViewTransitions,
            SyncTransactionQueue syncQueue,
            Executor callbackExecutor,
            List<ComponentName> allowListedActivities,
            SemiControlledCarTaskViewCallbacks callbacks,
            AtomicReference<CarActivityManager> carActivityManager) {
        super(context, organizer, taskViewTransitions, syncQueue, true);
        mCallbacks = callbacks;
        mCallbackExecutor = callbackExecutor;
        mCallbackExecutor.execute(() -> mCallbacks.onTaskViewCreated(this));
        mShellTaskOrganizer = organizer;
        mSyncQueue = syncQueue;
        mAllowListedActivities = new ArrayList<>(allowListedActivities);
        mCarActivityManagerRef = carActivityManager;
    }

    /**
     * @return the underlying {@link SemiControlledCarTaskViewCallbacks}.
     */
    SemiControlledCarTaskViewCallbacks getCallbacks() {
        return mCallbacks;
    }

    @Override
    protected void onCarTaskViewInitialized() {
        super.onCarTaskViewInitialized();
        mShellTaskOrganizer.getExecutor().execute(() -> {
            // removeWithTaskOrganizer should be true to signal the system that this root task is
            // inside a TaskView and should not be animated by the core.
            mShellTaskOrganizer.createRootTask(DEFAULT_DISPLAY,
                    WINDOWING_MODE_MULTI_WINDOW,
                    mRootTaskListener, /* removeWithTaskOrganizer= */ true);
        });
    }

    @Override
    public void release() {
        super.release();
        clearRootTask();
    }

    public ActivityManager.RunningTaskInfo getTopTaskInTheRootTask() {
        if (mChildrenTaskStack.isEmpty()) {
            return null;
        }
        ActivityManager.RunningTaskInfo topTask = null;
        Iterator<ActivityManager.RunningTaskInfo> iterator = mChildrenTaskStack.values().iterator();
        while (iterator.hasNext()) {
            topTask = iterator.next();
        }
        return topTask;
    }

    private void clearRootTask() {
        if (mRootTask == null) {
            Log.w(TAG, "Unable to clear root task because it is not created.");
            return;
        }
        // Should run on shell's executor
        mShellTaskOrganizer.deleteRootTask(mRootTask.token);
        mRootTask = null;
    }

    private void setRootTask(ActivityManager.RunningTaskInfo taskInfo) {
        mRootTask = taskInfo;
    }

    /**
     * Designates the given {@code activities} to be launched in this SemiControlledCarTaskView.
     * <p>Note: If an activity is already associated with another SemiControlledCarTaskView, it's
     * designates will be overridden.
     *
     * @param activities list of {@link ComponentName} of activities to be designated on the
     *                   SemiControlledCarTaskView
     */
    public void addAllowListedActivities(List<ComponentName> activities) {
        CarActivityManager carAm = mCarActivityManagerRef.get();
        if (carAm == null) {
            Log.wtf(TAG,
                    "CarActivityManager is null, cannot call setPersistentActivitiesOnRootTask to"
                            + " add activities");
            return;
        }
        if (mRootTask == null) {
            Log.wtf(TAG,
                    "RootTask is null, cannot call setPersistentActivitiesOnRootTask to"
                            + " add activities");
            return;
        }
        List<ComponentName> activitiesToAdd = new ArrayList<>();
        for (ComponentName activity : activities) {
            if (!mAllowListedActivities.contains(activity)) {
                activitiesToAdd.add(activity);
            } else {
                if (DBG) {
                    Log.d(TAG, "Activity " + activity
                            + " is already designated to this SemiControlledCarTaskView");
                }
            }
        }
        mAllowListedActivities.addAll(activitiesToAdd);
        carAm.setPersistentActivitiesOnRootTask(activitiesToAdd, mRootTask.token.asBinder());
    }

    /**
     * Remove the designation of the given {@code activities} to SemiControlledCarTaskView.
     * <p>Note: If an activity is already associated with another SemiControlledCarTaskView, it's
     * designates will be overridden.
     *
     * @param activities list of {@link ComponentName} of activities to be designated on the
     *                   SemiControlledCarTaskView
     */
    public void removeAllowListedActivities(List<ComponentName> activities) {
        CarActivityManager carAm = mCarActivityManagerRef.get();
        if (carAm == null) {
            Log.wtf(TAG,
                    "CarActivityManager is null, cannot call setPersistentActivitiesOnRootTask to"
                            + " remove activities");
            return;
        }
        if (mRootTask == null) {
            Log.wtf(TAG,
                    "RootTask is null, cannot call setPersistentActivitiesOnRootTask to"
                            + " add activities");
            return;
        }
        List<ComponentName> activitiesToRemove = new ArrayList<>();
        for (ComponentName activity : activities) {
            if (mAllowListedActivities.contains(activity)) {
                activitiesToRemove.add(activity);
            } else {
                if (DBG) {
                    Log.d(TAG, "Activity " + activity
                            + " was not designated to this SemiControlledCarTaskView");
                }
            }
        }
        mAllowListedActivities.removeAll(activitiesToRemove);
        carAm.setPersistentActivitiesOnRootTask(activitiesToRemove, /* rootTaskToken= */ null);
    }

    /**
     * Sets the designations for SemiControlledCarTaskView. Adds the designation of the given
     * {@code activities} to SemiControlledCarTaskView and removes the designations of activities
     * that are not in {@code activities}.
     *
     * <p>Note:
     * If an activity is already associated with another SemiControlledCarTaskView, it's
     * designates will be overridden.
     *
     * @param activities list of {@link ComponentName} of activities to be designated on the
     *                   SemiControlledCarTaskView
     */
    public void setAllowListedActivities(List<ComponentName> activities) {
        CarActivityManager carAm = mCarActivityManagerRef.get();
        if (carAm == null) {
            Log.wtf(TAG,
                    "CarActivityManager is null, cannot call setPersistentActivitiesOnRootTask to"
                            + " remove activities");
            return;
        }
        if (mRootTask == null) {
            Log.wtf(TAG,
                    "RootTask is null, cannot call setPersistentActivitiesOnRootTask to"
                            + " add activities");
            return;
        }
        List<ComponentName> activitiesToRemove = new ArrayList<>(mAllowListedActivities);
        carAm.setPersistentActivitiesOnRootTask(activitiesToRemove, /* rootTaskToken= */ null);
        carAm.setPersistentActivitiesOnRootTask(activities, mRootTask.token.asBinder());
        mAllowListedActivities.clear();
        mAllowListedActivities.addAll(activities);
    }

    @VisibleForTesting
    List<ComponentName> getPersistentActivities() {
        return mAllowListedActivities;
    }
}
