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

package com.android.car.remoteaccess;

import static android.car.user.CarUserManager.USER_LIFECYCLE_EVENT_TYPE_UNLOCKED;

import static com.android.car.remoteaccess.RemoteAccessStorage.RemoteAccessDbHelper.DATABASE_NAME;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.spyOn;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.Car;
import android.car.hardware.power.CarPowerManager;
import android.car.hardware.power.ICarPowerStateListener;
import android.car.remoteaccess.CarRemoteAccessManager;
import android.car.remoteaccess.ICarRemoteAccessCallback;
import android.car.remoteaccess.RemoteTaskClientRegistrationInfo;
import android.car.user.CarUserManager.UserLifecycleEvent;
import android.car.user.CarUserManager.UserLifecycleListener;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.res.Resources;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.util.ArraySet;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import com.android.car.CarLocalServices;
import com.android.car.CarServiceUtils;
import com.android.car.R;
import com.android.car.hal.PowerHalService;
import com.android.car.power.CarPowerManagementService;
import com.android.car.remoteaccess.RemoteAccessStorage.ClientIdEntry;
import com.android.car.remoteaccess.hal.RemoteAccessHalCallback;
import com.android.car.remoteaccess.hal.RemoteAccessHalWrapper;
import com.android.car.systeminterface.SystemInterface;
import com.android.car.user.CarUserService;
import com.android.compatibility.common.util.PollingCheck;
import com.android.internal.annotations.GuardedBy;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.io.File;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Set;

@RunWith(MockitoJUnitRunner.class)
public final class CarRemoteAccessServiceUnitTest {

    private static final String TAG = CarRemoteAccessServiceUnitTest.class.getSimpleName();
    private static final long WAIT_TIMEOUT_MS = 5000;
    private static final long ALLOWED_SYSTEM_UP_TIME_FOR_TESTING_MS = 5000;
    private static final int TASK_UNBIND_DELAY_MS = 1000;
    private static final String WAKEUP_SERVICE_NAME = "android_wakeup_service";
    private static final String TEST_VEHICLE_ID = "test_vehicle";
    private static final String TEST_PROCESSOR_ID = "test_processor";
    private static final String PERMISSION_NOT_GRANTED_PACKAGE = "life.is.beautiful";
    private static final String PERMISSION_GRANTED_PACKAGE_ONE = "we.are.the.world";
    private static final String PERMISSION_GRANTED_PACKAGE_TWO = "android.automotive.os";
    private static final int UID_PERMISSION_NOT_GRANTED_PACKAGE = 1;
    private static final int UID_PERMISSION_GRANTED_PACKAGE_ONE = 2;
    private static final int UID_PERMISSION_GRANTED_PACKAGE_TWO = 3;
    private static final String CLASS_NAME_ONE = "Hello";
    private static final String CLASS_NAME_TWO = "Best";
    private static final List<PackagePrepForTest> AVAILABLE_PACKAGES = List.of(
            createPackagePrepForTest(PERMISSION_GRANTED_PACKAGE_ONE,
                    CLASS_NAME_ONE, /* permissionGranted= */ true,
                    UID_PERMISSION_GRANTED_PACKAGE_ONE),
            createPackagePrepForTest(PERMISSION_GRANTED_PACKAGE_TWO,
                    CLASS_NAME_TWO, /* permissionGranted= */ true,
                    UID_PERMISSION_GRANTED_PACKAGE_TWO),
            createPackagePrepForTest(PERMISSION_NOT_GRANTED_PACKAGE, "Happy",
                    /* permissionGranted= */ false,
                    UID_PERMISSION_NOT_GRANTED_PACKAGE)
    );
    private static final List<ClientIdEntry> PERSISTENT_CLIENTS = List.of(
            new ClientIdEntry("12345", System.currentTimeMillis(), "we.are.the.world"),
            new ClientIdEntry("98765", System.currentTimeMillis(), "android.automotive.os")
    );

    private CarRemoteAccessService mService;
    private ICarRemoteAccessCallbackImpl mRemoteAccessCallback;
    private CarPowerManagementService mOldCarPowerManagementService;
    private CarUserService mOldCarUserService;
    private File mDatabaseFile;
    private Context mContext;
    private RemoteAccessStorage mRemoteAccessStorage;
    private Runnable mBootComplete;
    private boolean mBootCompleted;

    @Mock private Resources mResources;
    @Mock private PackageManager mPackageManager;
    @Mock private RemoteAccessHalWrapper mRemoteAccessHalWrapper;
    @Mock private SystemInterface mSystemInterface;
    @Mock private CarPowerManagementService mCarPowerManagementService;
    @Mock private CarUserService mCarUserService;
    @Mock private PowerHalService mPowerHalService;
    @Mock private CarRemoteAccessService.CarRemoteAccessServiceDep mDep;
    @Mock private UserManager mUserManager;

    @Captor private ArgumentCaptor<UserLifecycleListener> mUserLifecycleListenerCaptor;
    @Captor private ArgumentCaptor<RemoteTaskClientRegistrationInfo> mRegistrationInfoCaptor;

    private CarRemoteAccessService newServiceWithSystemUpTime(long systemUpTime) {
        CarRemoteAccessService service =  new CarRemoteAccessService(mContext, mSystemInterface,
                mPowerHalService, mDep, /* remoteAccessHal= */ null, mRemoteAccessStorage,
                systemUpTime, /* inMemoryStorage= */ true);
        service.setRemoteAccessHalWrapper(mRemoteAccessHalWrapper);
        return service;
    }

    @Before
    public void setUp() {
        mOldCarPowerManagementService = CarLocalServices.getService(
                CarPowerManagementService.class);
        CarLocalServices.removeServiceForTest(CarPowerManagementService.class);
        CarLocalServices.addService(CarPowerManagementService.class, mCarPowerManagementService);
        mOldCarUserService = CarLocalServices.getService(CarUserService.class);
        CarLocalServices.removeServiceForTest(CarUserService.class);
        CarLocalServices.addService(CarUserService.class, mCarUserService);

        mContext = InstrumentationRegistry.getTargetContext().createDeviceProtectedStorageContext();
        spyOn(mContext);

        // doReturn().when() pattern is necessary because mContext is spied.
        doReturn(mPackageManager).when(mContext).getPackageManager();
        doReturn(mResources).when(mContext).getResources();
        doReturn(mUserManager).when(mContext).getSystemService(UserManager.class);
        doReturn(true).when(mContext).bindServiceAsUser(any(), any(), anyInt(), any());
        doNothing().when(mContext).unbindService(any());
        when(mUserManager.isUserUnlocked(any())).thenReturn(true);
        mDatabaseFile = mContext.getDatabasePath(DATABASE_NAME);
        when(mResources.getInteger(R.integer.config_allowedSystemUptimeForRemoteAccess))
                .thenReturn(300);
        when(mRemoteAccessHalWrapper.getWakeupServiceName()).thenReturn(WAKEUP_SERVICE_NAME);
        when(mRemoteAccessHalWrapper.getVehicleId()).thenReturn(TEST_VEHICLE_ID);
        when(mRemoteAccessHalWrapper.getProcessorId()).thenReturn(TEST_PROCESSOR_ID);
        when(mRemoteAccessHalWrapper.notifyApStateChange(anyBoolean(), anyBoolean()))
                .thenReturn(true);
        when(mCarPowerManagementService.getLastShutdownState())
                .thenReturn(CarRemoteAccessManager.NEXT_POWER_STATE_OFF);
        when(mSystemInterface.getSystemCarDir()).thenReturn(mDatabaseFile.getParentFile());
        mockPackageInfo();
        setVehicleInUse(/* inUse= */ false);

        when(mPackageManager.getNameForUid(UID_PERMISSION_NOT_GRANTED_PACKAGE)).thenReturn(
                PERMISSION_NOT_GRANTED_PACKAGE);
        when(mPackageManager.getNameForUid(UID_PERMISSION_GRANTED_PACKAGE_ONE)).thenReturn(
                PERMISSION_GRANTED_PACKAGE_ONE);
        when(mPackageManager.getNameForUid(UID_PERMISSION_GRANTED_PACKAGE_TWO)).thenReturn(
                PERMISSION_GRANTED_PACKAGE_TWO);

        mRemoteAccessCallback = new ICarRemoteAccessCallbackImpl();
        mRemoteAccessStorage = new RemoteAccessStorage(mContext, mSystemInterface,
                /* inMemoryStorage= */ true);
        mService = newServiceWithSystemUpTime(ALLOWED_SYSTEM_UP_TIME_FOR_TESTING_MS);
    }

    @After
    public void tearDown() {
        mService.release();
        CarServiceUtils.finishAllHandlerTasks();

        CarLocalServices.removeServiceForTest(CarPowerManagementService.class);
        CarLocalServices.addService(CarPowerManagementService.class, mOldCarPowerManagementService);
        CarLocalServices.removeServiceForTest(CarUserService.class);
        CarLocalServices.addService(CarUserService.class, mOldCarUserService);

        if (mDatabaseFile.exists() && !mDatabaseFile.delete()) {
            Log.e(TAG, "Failed to delete the database file: " + mDatabaseFile.getAbsolutePath());
        }
    }

    private void verifyBindingStartedForPackages(String[] packageNames, String[] classNames) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);

        mService.init();

        verify(mContext, times(packageNames.length)).bindServiceAsUser(intentCaptor.capture(),
                any(ServiceConnection.class), anyInt(), any(UserHandle.class));

        Set<ComponentName> gotComponents = new ArraySet<>();
        Set<ComponentName> wantComponents = new ArraySet<>();
        for (int i = 0; i < packageNames.length; i++) {
            Intent intent = intentCaptor.getAllValues().get(i);
            gotComponents.add(intent.getComponent());
            wantComponents.add(new ComponentName(packageNames[i], classNames[i]));
        }

        assertThat(gotComponents).isEqualTo(wantComponents);
    }

    @Test
    public void testStartRemoteTaskClientService() {
        String[] packageNames = new String[]{PERMISSION_GRANTED_PACKAGE_ONE,
                PERMISSION_GRANTED_PACKAGE_TWO};
        String[] classNames = new String[]{CLASS_NAME_ONE, CLASS_NAME_TWO};

        mService.setAllowedTimeForRemoteTaskClientInitMs(100);
        runBootComplete();
        mService.init();

        verifyBindingStartedForPackages(packageNames, classNames);
        verify(mContext, timeout(WAIT_TIMEOUT_MS).times(2)).unbindService(any());
    }

    @Test
    public void testStartRemoteTaskClientServiceUserLocked() {
        String[] packageNames = new String[]{PERMISSION_GRANTED_PACKAGE_ONE,
                PERMISSION_GRANTED_PACKAGE_TWO};
        String[] classNames = new String[]{CLASS_NAME_ONE, CLASS_NAME_TWO};
        when(mUserManager.isUserUnlocked(any())).thenReturn(false);

        runBootComplete();
        mService.init();

        verify(mUserManager, times(2)).isUserUnlocked(eq(UserHandle.SYSTEM));
        verify(mCarUserService, times(2)).addUserLifecycleListener(any(),
                mUserLifecycleListenerCaptor.capture());
        verify(mContext, never()).bindServiceAsUser(any(), any(), anyInt(), any());

        for (int i = 0; i < packageNames.length; i++) {
            UserLifecycleListener listener = mUserLifecycleListenerCaptor.getAllValues().get(i);
            listener.onEvent(new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED,
                    UserHandle.USER_SYSTEM));
        }

        verifyBindingStartedForPackages(packageNames, classNames);
    }

    // If bindService is called after user unlock but before the intent arrives, we must make sure
    // only one binding is going to happen.
    @Test
    public void testStartRemoteTaskClientServiceUserLocked_bindAgainAfterUnlock() throws Exception {
        String[] packageNames = new String[]{PERMISSION_GRANTED_PACKAGE_ONE,
                PERMISSION_GRANTED_PACKAGE_TWO};
        when(mUserManager.isUserUnlocked(any())).thenReturn(false);

        runBootComplete();
        mService.init();

        verify(mUserManager, times(2)).isUserUnlocked(eq(UserHandle.SYSTEM));
        verify(mCarUserService, times(2)).addUserLifecycleListener(any(),
                mUserLifecycleListenerCaptor.capture());
        verify(mContext, never()).bindServiceAsUser(any(), any(), anyInt(), any());

        // Simulate a task arrives for PACKAGE_ONE which will try to start it.
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        halCallback.onRemoteTaskRequested(clientId, data);

        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
        // Must not start the service since the service is waiting for user unlock.
        verify(mContext, never()).bindServiceAsUser(any(), any(), anyInt(), any());

        // Simulate user_unlock intent arrives.
        for (int i = 0; i < packageNames.length; i++) {
            UserLifecycleListener listener = mUserLifecycleListenerCaptor.getAllValues().get(i);
            listener.onEvent(new UserLifecycleEvent(USER_LIFECYCLE_EVENT_TYPE_UNLOCKED,
                    UserHandle.USER_SYSTEM));
        }

        verify(mContext, times(2)).bindServiceAsUser(any(), any(), anyInt(), any());
    }

    @Test
    public void testStartRemoteTaskClientServiceUserLocked_unbindWhileWaiting() throws Exception {
        when(mUserManager.isUserUnlocked(any())).thenReturn(false);

        runBootComplete();
        mService.init();

        verify(mUserManager, times(2)).isUserUnlocked(eq(UserHandle.SYSTEM));
        verify(mCarUserService, times(2)).addUserLifecycleListener(any(), any());
        verify(mContext, never()).bindServiceAsUser(any(), any(), anyInt(), any());

        // Unbinding services should cancel the wait for user unlock.
        mService.unbindAllServices();

        verify(mCarUserService, times(2)).removeUserLifecycleListener(any());
        verify(mContext, never()).unbindService(any());

        // Simulate a task arrives for PACKAGE_ONE which will try to start it.
        // Should start waiting for user unlock again.
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        halCallback.onRemoteTaskRequested(clientId, data);

        // Should register a new receiver to wait for user unlock again.
        verify(mCarUserService, times(3)).addUserLifecycleListener(any(), any());
    }

    @Test
    public void testCarRemoteAccessServiceInit() throws Exception {
        mService.init();

        verify(mRemoteAccessHalWrapper, timeout(1000)).notifyApStateChange(
                /* isReadyForRemoteTask= */ true, /* isWakeupRequired= */ false);
    }

    @Test
    public void testCarRemoteAccessServiceInit_retryNotifyApState() throws Exception {
        when(mRemoteAccessHalWrapper.notifyApStateChange(anyBoolean(), anyBoolean()))
                .thenReturn(false).thenReturn(false).thenReturn(true);

        mService.init();

        // This should take about 300ms, so waiting 5s is definitely enough.
        verify(mRemoteAccessHalWrapper, timeout(WAIT_TIMEOUT_MS).times(3)).notifyApStateChange(
                anyBoolean(), anyBoolean());
    }

    @Test
    public void testCarRemoteAccessServiceInit_maxRetryNotifyApState() throws Exception {
        when(mRemoteAccessHalWrapper.notifyApStateChange(anyBoolean(), anyBoolean()))
                .thenReturn(false);

        mService.init();

        verify(mRemoteAccessHalWrapper, timeout(WAIT_TIMEOUT_MS).times(10)).notifyApStateChange(
                /* isReadyForRemoteTask= */ true, /* isWakeupRequired= */ false);

        SystemClock.sleep(100);

        // Verify no more retry.
        verify(mRemoteAccessHalWrapper, times(10)).notifyApStateChange(
                /* isReadyForRemoteTask= */ true, /* isWakeupRequired= */ false);
    }

    @Test
    public void testCarRemoteAccessServiceInit_resetRetryCountAfterSuccess() throws Exception {
        when(mRemoteAccessHalWrapper.notifyApStateChange(anyBoolean(), anyBoolean()))
                .thenReturn(false).thenReturn(false).thenReturn(true);

        mService.init();

        verify(mRemoteAccessHalWrapper, timeout(WAIT_TIMEOUT_MS).times(3)).notifyApStateChange(
                /* isReadyForRemoteTask= */ true, /* isWakeupRequired= */ false);

        ICarPowerStateListener powerStateListener = getCarPowerStateListener();
        // Success notifying should not be limited retry count and should reset retry count to 0.
        for (int i = 0; i < 10; i++) {
            powerStateListener.onStateChanged(CarPowerManager.STATE_WAIT_FOR_VHAL, 0);
        }

        // notifyApStateChange is also called when initializaing CarRemoteAccessService.
        verify(mRemoteAccessHalWrapper, times(13)).notifyApStateChange(
                /* isReadyForRemoteTask= */ true, /* isWakeupRequired= */ false);
    }

    @Test
    public void testAddCarRemoteTaskClient() throws Exception {
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        mService.init();

        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        PollingCheck.check("onClientRegistrationUpdated should be called", WAIT_TIMEOUT_MS,
                () -> Objects.equals(mRemoteAccessCallback.getServiceName(), WAKEUP_SERVICE_NAME)
                        && Objects.equals(mRemoteAccessCallback.getVehicleId(), TEST_VEHICLE_ID)
                        && Objects.equals(mRemoteAccessCallback.getProcessorId(), TEST_PROCESSOR_ID)
                        && mRemoteAccessCallback.getClientId() != null);
    }

    @Test
    public void testAddCarRemoteTaskClient_addTwice() throws Exception {
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        ICarRemoteAccessCallbackImpl secondCallback = new ICarRemoteAccessCallbackImpl();
        mService.init();
        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        mService.addCarRemoteTaskClient(secondCallback);

        PollingCheck.check("onClientRegistrationUpdated should be called", WAIT_TIMEOUT_MS,
                () -> Objects.equals(secondCallback.getServiceName(), WAKEUP_SERVICE_NAME)
                        && Objects.equals(secondCallback.getVehicleId(), TEST_VEHICLE_ID)
                        && Objects.equals(secondCallback.getProcessorId(), TEST_PROCESSOR_ID)
                        && secondCallback.getClientId() != null
                        && secondCallback.getClientId().equals(
                                mRemoteAccessCallback.getClientId()));
    }

    @Test
    public void testAddCarRemoteTaskClient_addMultipleClients() throws Exception {
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE)
                .thenReturn(UID_PERMISSION_GRANTED_PACKAGE_TWO);
        ICarRemoteAccessCallbackImpl secondCallback = new ICarRemoteAccessCallbackImpl();
        mService.init();

        mService.addCarRemoteTaskClient(mRemoteAccessCallback);
        mService.addCarRemoteTaskClient(secondCallback);

        PollingCheck.check("Two clients should have different client IDs", WAIT_TIMEOUT_MS,
                () -> {
                    String clientIdOne = mRemoteAccessCallback.getClientId();
                    String clientIdTwo = secondCallback.getClientId();
                    return clientIdOne != null && !clientIdOne.equals(clientIdTwo);
                });
    }

    @Test
    public void testAddCarRemoteTaskClient_persistentClientId() throws Exception {
        String packageName = PERSISTENT_CLIENTS.get(0).uidName;
        String expectedClientId = PERSISTENT_CLIENTS.get(0).clientId;
        when(mDep.getCallingUid()).thenReturn(1234);
        when(mPackageManager.getNameForUid(1234)).thenReturn(packageName);
        setupDatabase();
        mService.init();

        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        PollingCheck.check("onClientRegistrationUpdated should be called", WAIT_TIMEOUT_MS,
                () -> Objects.equals(mRemoteAccessCallback.getServiceName(), WAKEUP_SERVICE_NAME)
                        && Objects.equals(mRemoteAccessCallback.getVehicleId(), TEST_VEHICLE_ID)
                        && Objects.equals(mRemoteAccessCallback.getProcessorId(), TEST_PROCESSOR_ID)
                        && Objects.equals(mRemoteAccessCallback.getClientId(), expectedClientId));
    }

    @Test
    public void testRemoveCarRemoteTaskClient() throws Exception {
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        mService.init();
        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);
    }

    @Test
    public void testRemoveCarRemoteTaskClient_removeNotAddedClient() throws Exception {
        mService.init();
        // Removing unregistered ICarRemoteAccessCallback is no-op.
        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);
    }

    @Test
    public void testRemoveCarRemoteTaskClient_removeActiveTasks() throws Exception {
        // Only use one package.
        mockPackageInfo(1);
        mService.init();
        mService.setTaskUnbindDelayMs(100);
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();

        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        // Starts an active task.
        halCallback.onRemoteTaskRequested(clientId, data);

        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
        String taskId = mRemoteAccessCallback.getTaskId();

        // This should clear the active tasks, after 100ms, the client should be unbound and the
        // device should be shutdown.
        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);

        // This should throw exception because clientId is not valid.
        assertThrows(IllegalArgumentException.class, () -> mService.reportRemoteTaskDone(
                clientId, taskId));

        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        // This should throw exception because the task was cleared so the task ID is not valid.
        assertThrows(IllegalArgumentException.class, () -> mService.reportRemoteTaskDone(
                clientId, taskId));

        // The client must be unbound.
        verify(mContext, timeout(WAIT_TIMEOUT_MS)).unbindService(any());
        // The device must be shutdown.
        verify(mCarPowerManagementService, timeout(WAIT_TIMEOUT_MS)).requestShutdownAp(
                anyInt(), anyBoolean());
    }

    @Test
    public void testRemoteTaskRequested() throws Exception {
        mService.init();
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();

        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        halCallback.onRemoteTaskRequested(clientId, data);

        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
        assertWithMessage("Data").that(mRemoteAccessCallback.getData()).asList()
                .containsExactlyElementsIn(new Byte[]{1, 2, 3, 4});
    }

    @Test
    public void testRemoteTaskRequested_removedClient() throws Exception {
        mService.init();
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);

        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);

        assertWithMessage("Task ID").that(mRemoteAccessCallback.getTaskId()).isNull();
    }

    @Test
    public void testRemoteTaskRequested_clientRegisteredAfterRequest() throws Exception {
        mService.init();
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);
        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);

        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
    }

    @Test
    public void testRemoteTaskRequested_persistentClientRegisteredAfterRequest() throws Exception {
        String packageName = PERSISTENT_CLIENTS.get(0).uidName;
        String clientId = PERSISTENT_CLIENTS.get(0).clientId;
        when(mDep.getCallingUid()).thenReturn(1234);
        when(mPackageManager.getNameForUid(1234)).thenReturn(packageName);
        RemoteAccessHalCallback halCallback = mService.getRemoteAccessHalCallback();
        setupDatabase();
        mService.init();
        runBootComplete();

        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);
        SystemClock.sleep(500);
        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
    }

    @Test
    public void testRemoteTaskRequested_withTwoClientsRegistered() throws Exception {
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE)
                .thenReturn(UID_PERMISSION_GRANTED_PACKAGE_TWO);
        ICarRemoteAccessCallbackImpl secondCallback = new ICarRemoteAccessCallbackImpl();
        mService.init();
        runBootComplete();
        mService.addCarRemoteTaskClient(mRemoteAccessCallback);
        mService.addCarRemoteTaskClient(secondCallback);
        PollingCheck.check("Client is registered", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getClientId() != null);
        PollingCheck.check("Client is registered", WAIT_TIMEOUT_MS,
                () -> secondCallback.getClientId() != null);
        String clientId = mRemoteAccessCallback.getClientId();
        mService.removeCarRemoteTaskClient(secondCallback);
        RemoteAccessHalCallback halCallback = mService.getRemoteAccessHalCallback();

        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);

        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
        assertWithMessage("Task ID").that(secondCallback.getTaskId()).isNull();
    }

    @Test
    public void testReportTaskDone_withPowerStateOff() throws Exception {
        mService.init();
        prepareReportTaskDoneTest();
        String clientId = mRemoteAccessCallback.getClientId();
        String taskId = mRemoteAccessCallback.getTaskId();
        mService.setPowerStatePostTaskExecution(
                CarRemoteAccessManager.NEXT_POWER_STATE_OFF, /* runGarageMode= */ true);

        mService.reportRemoteTaskDone(clientId, taskId);

        // Need to wait TASK_UNBIND_DELAY_MS before shutdown happens.
        verify(mCarPowerManagementService, never()).requestShutdownAp(anyInt(), anyBoolean());
        verify(mCarPowerManagementService, timeout(WAIT_TIMEOUT_MS)).requestShutdownAp(
                CarRemoteAccessManager.NEXT_POWER_STATE_OFF, /* runGarageMode= */ true);
    }

    @Test
    public void testReportRemoteTaskDone_withPowerStateS2R() throws Exception {
        mService.init();
        prepareReportTaskDoneTest();
        String clientId = mRemoteAccessCallback.getClientId();
        String taskId = mRemoteAccessCallback.getTaskId();
        mService.setPowerStatePostTaskExecution(
                CarRemoteAccessManager.NEXT_POWER_STATE_SUSPEND_TO_RAM, /* runGarageMode= */ true);

        mService.reportRemoteTaskDone(clientId, taskId);

        verify(mCarPowerManagementService, timeout(WAIT_TIMEOUT_MS)).requestShutdownAp(
                CarRemoteAccessManager.NEXT_POWER_STATE_SUSPEND_TO_RAM, /* runGarageMode= */ true);
    }

    @Test
    public void testReportRemoteTaskDone_withPowerStateOn() throws Exception {
        mService.init();
        prepareReportTaskDoneTest();
        String clientId = mRemoteAccessCallback.getClientId();
        String taskId = mRemoteAccessCallback.getTaskId();
        mService.setPowerStatePostTaskExecution(
                CarRemoteAccessManager.NEXT_POWER_STATE_ON, /* runGarageMode= */ false);

        mService.reportRemoteTaskDone(clientId, taskId);
        SystemClock.sleep(TASK_UNBIND_DELAY_MS);

        verify(mCarPowerManagementService, never()).requestShutdownAp(
                CarRemoteAccessManager.NEXT_POWER_STATE_SUSPEND_TO_RAM, /* runGarageMode= */ true);
    }

    @Test
    public void testReportRemoteTaskDone_vehicleInUse() throws Exception {
        setVehicleInUse(/* inUse= */ true);
        mService.init();
        prepareReportTaskDoneTest();
        String clientId = mRemoteAccessCallback.getClientId();
        String taskId = mRemoteAccessCallback.getTaskId();
        mService.setPowerStatePostTaskExecution(
                CarRemoteAccessManager.NEXT_POWER_STATE_OFF, /* runGarageMode= */ false);

        mService.reportRemoteTaskDone(clientId, taskId);
        SystemClock.sleep(TASK_UNBIND_DELAY_MS);

        verify(mCarPowerManagementService, never()).requestShutdownAp(
                CarRemoteAccessManager.NEXT_POWER_STATE_SUSPEND_TO_RAM, /* runGarageMode= */ true);
    }

    @Test
    public void testReportTaskDone_wrongTaskId() throws Exception {
        mService.init();
        prepareReportTaskDoneTest();
        String clientId = mRemoteAccessCallback.getClientId();
        String wrongTaskId = mRemoteAccessCallback.getTaskId() + "_WRONG";

        assertThrows(IllegalArgumentException.class,
                () -> mService.reportRemoteTaskDone(clientId, wrongTaskId));
    }

    @Test
    public void testReportTaskDone_wrongClientId() throws Exception {
        mService.init();
        prepareReportTaskDoneTest();
        String wrongClientId = mRemoteAccessCallback.getClientId() + "_WRONG";
        String taskId = mRemoteAccessCallback.getTaskId();

        assertThrows(IllegalArgumentException.class,
                () -> mService.reportRemoteTaskDone(wrongClientId, taskId));
    }

    @Test
    public void testUnbindServiceAfterTaskComplete() throws Exception {
        mService.init();
        mService.setTaskUnbindDelayMs(100);
        setVehicleInUse(/* inUse= */ true);
        ICarRemoteAccessCallbackImpl callback1 = new ICarRemoteAccessCallbackImpl();
        ICarRemoteAccessCallbackImpl callback2 = new ICarRemoteAccessCallbackImpl();
        runBootComplete();
        prepareReportTaskDoneTest(callback1, UID_PERMISSION_GRANTED_PACKAGE_ONE);
        String clientId1 = callback1.getClientId();
        String taskId1 = callback1.getTaskId();
        prepareReportTaskDoneTest(callback2, UID_PERMISSION_GRANTED_PACKAGE_TWO);
        String clientId2 = callback2.getClientId();
        String taskId2 = callback2.getTaskId();
        mService.setPowerStatePostTaskExecution(
                CarRemoteAccessManager.NEXT_POWER_STATE_OFF, /* runGarageMode= */ true);

        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        mService.reportRemoteTaskDone(clientId1, taskId1);

        // package one should be unbound after 100ms.
        verify(mContext, timeout(WAIT_TIMEOUT_MS)).unbindService(any());
        verify(mCarPowerManagementService, never()).requestShutdownAp(anyInt(), anyBoolean());

        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_TWO);
        mService.reportRemoteTaskDone(clientId2, taskId2);

        // package two should be unbound since no active tasks.
        verify(mContext, timeout(WAIT_TIMEOUT_MS).times(2)).unbindService(any());
    }

    @Test
    public void testUnbindServiceAfterTimeout() throws Exception {
        // Only use one package.
        mockPackageInfo(1);
        mService = newServiceWithSystemUpTime(2000L);
        // If no task arrive, the service will be unbound after 1000ms.
        mService.setAllowedTimeForRemoteTaskClientInitMs(1000);
        mService.init();
        setVehicleInUse(/* inUse= */ true);
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();

        // This task will timeout at 2000.
        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);

        SystemClock.sleep(1000);

        // This task will timeout at 3000.
        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);

        SystemClock.sleep(1000);

        // This is time 2000, the first task timed out but the second task is still active.
        verify(mContext, never()).unbindService(any());

        // Techcnially the service should timeout at time 3000, we leave a 1000ms buffer.
        verify(mContext, timeout(2000)).unbindService(any());
    }

    @Test
    public void testNotifyApPowerState_waitForVhal() throws Exception {
        mService.init();
        mService.setTaskUnbindDelayMs(100);
        ICarPowerStateListener powerStateListener = getCarPowerStateListener();
        verify(mRemoteAccessHalWrapper, timeout(1000)).notifyApStateChange(
                anyBoolean(), anyBoolean());

        powerStateListener.onStateChanged(CarPowerManager.STATE_WAIT_FOR_VHAL, 0);
        // notifyApStateChange is also called when initializaing CarRemoteAccessService.
        verify(mRemoteAccessHalWrapper, times(2)).notifyApStateChange(
                /* isReadyForRemoteTask= */ true, /* isWakeupRequired= */ false);
        verify(mCarPowerManagementService, never())
                .finished(eq(CarPowerManager.STATE_WAIT_FOR_VHAL), any());
    }

    @Test
    public void testNotifyApPowerState_shutdownPrepare() throws Exception {
        mService.init();
        ICarPowerStateListener powerStateListener = getCarPowerStateListener();
        verify(mRemoteAccessHalWrapper, timeout(1000)).notifyApStateChange(
                anyBoolean(), anyBoolean());

        powerStateListener.onStateChanged(CarPowerManager.STATE_SHUTDOWN_PREPARE, 0);
        verify(mRemoteAccessHalWrapper).notifyApStateChange(/* isReadyForRemoteTask= */ false,
                /* isWakeupRequired= */ false);
        verify(mCarPowerManagementService).finished(eq(CarPowerManager.STATE_SHUTDOWN_PREPARE),
                any());
    }

    @Test
    public void testUnbindAllServiceOnShutdownPrepare() throws Exception {
        mService.init();
        ICarPowerStateListener powerStateListener = getCarPowerStateListener();
        verify(mRemoteAccessHalWrapper, timeout(1000)).notifyApStateChange(
                anyBoolean(), anyBoolean());
        mBootComplete.run();

        powerStateListener.onStateChanged(CarPowerManager.STATE_SHUTDOWN_PREPARE, 0);

        verify(mRemoteAccessHalWrapper).notifyApStateChange(/* isReadyForRemoteTask= */ false,
                /* isWakeupRequired= */ false);
        verify(mCarPowerManagementService).finished(eq(CarPowerManager.STATE_SHUTDOWN_PREPARE),
                any());
        verify(mContext, times(2)).unbindService(any());

        // Unbind service multiple times must do nothing.
        powerStateListener.onStateChanged(CarPowerManager.STATE_SHUTDOWN_PREPARE, 0);

        verify(mContext, times(2)).unbindService(any());
    }

    @Test
    public void testNotifyApPowerState_postShutdownEnter() throws Exception {
        mService.init();
        ICarPowerStateListener powerStateListener = getCarPowerStateListener();
        verify(mRemoteAccessHalWrapper, timeout(1000)).notifyApStateChange(
                anyBoolean(), anyBoolean());

        powerStateListener.onStateChanged(CarPowerManager.STATE_POST_SHUTDOWN_ENTER, 0);
        verify(mRemoteAccessHalWrapper).notifyApStateChange(/* isReadyForRemoteTask= */ false,
                /* isWakeupRequired= */ true);
        verify(mCarPowerManagementService).finished(eq(CarPowerManager.STATE_POST_SHUTDOWN_ENTER),
                any());
    }

    @Test
    public void testWrappingUpCarRemoteAccessServiceAfterAllowedTime() throws Exception {
        // Use a shorter time for testing.
        mService = newServiceWithSystemUpTime(100L);

        mService.init();
        mService.setPowerStatePostTaskExecution(CarRemoteAccessManager.NEXT_POWER_STATE_OFF,
                /* runGarageMode= */ false);

        verify(mCarPowerManagementService, timeout(WAIT_TIMEOUT_MS))
                .requestShutdownAp(CarRemoteAccessManager.NEXT_POWER_STATE_OFF,
                        /* runGarageMode= */ false);
    }

    @Test
    public void testWrappingUpCarRemoteAccessServiceAfterAllowedTime_vehicleInUse()
            throws Exception {
        // Use a shorter time for testing.
        mService = newServiceWithSystemUpTime(100L);
        setVehicleInUse(/* inUse= */ true);
        mService.init();
        mService.setPowerStatePostTaskExecution(CarRemoteAccessManager.NEXT_POWER_STATE_OFF,
                /* runGarageMode= */ false);
        SystemClock.sleep(100);

        verify(mCarPowerManagementService, never())
                .requestShutdownAp(CarRemoteAccessManager.NEXT_POWER_STATE_OFF,
                        /* runGarageMode= */ false);
    }

    @Test
    public void testTaskArriveAfterAllowedTime() throws Exception {
        // Require at least 1 seconds for task to be executed. Otherwise, taskExecutionTimeInSec
        // will become 0.
        mService = newServiceWithSystemUpTime(1000L);
        mService.init();
        mService.setPowerStatePostTaskExecution(CarRemoteAccessManager.NEXT_POWER_STATE_OFF,
                /* runGarageMode= */ false);

        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        SystemClock.sleep(1000);

        halCallback.onRemoteTaskRequested(clientId, data);

        SystemClock.sleep(1000);

        assertWithMessage("Must not dispatch remote task after shutdown is supposed to start")
                .that(mRemoteAccessCallback.getTaskId()).isNull();
    }

    // Allowed system up time must not take effect if vehicle is currently in use.
    @Test
    public void testTaskArriveAfterAllowedTime_vehicleInUse() throws Exception {
        // Require at least 1 seconds for task to be executed. Otherwise, taskExecutionTimeInSec
        // will become 0.
        mService = newServiceWithSystemUpTime(1000L);
        // Boot complete to trigger package search.
        runBootComplete();
        mService.init();
        mService.setPowerStatePostTaskExecution(CarRemoteAccessManager.NEXT_POWER_STATE_OFF,
                /* runGarageMode= */ false);
        setVehicleInUse(true);

        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        SystemClock.sleep(1000);

        halCallback.onRemoteTaskRequested(clientId, data);

        PollingCheck.check("Remote task received", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
        // We will round up 100ms to 1 sec.
        assertThat(mRemoteAccessCallback.getTaskMaxDurationInSec()).isEqualTo(1);
    }

    // Allowed system up time must not take effect if next power state is on.
    @Test
    public void testTaskArriveAfterAllowedTime_nextPowerStateOn() throws Exception {
        // Require at least 1 seconds for task to be executed. Otherwise, taskExecutionTimeInSec
        // will become 0.
        mService = newServiceWithSystemUpTime(1000L);
        // Boot complete to trigger package search.
        runBootComplete();
        mService.init();
        mService.setPowerStatePostTaskExecution(CarRemoteAccessManager.NEXT_POWER_STATE_ON,
                /* runGarageMode= */ false);

        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        byte[] data = new byte[]{1, 2, 3, 4};
        SystemClock.sleep(1000);

        halCallback.onRemoteTaskRequested(clientId, data);

        PollingCheck.check("Remote task received", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
    }

    @Test
    public void testAllowedSystemUptimeMs() {
        when(mResources.getInteger(R.integer.config_allowedSystemUptimeForRemoteAccess))
                .thenReturn(300);

        mService = new CarRemoteAccessService(mContext, mSystemInterface, mPowerHalService);

        assertThat(mService.getAllowedSystemUptimeMs()).isEqualTo(300_000L);
    }

    @Test
    public void testAllowedSystemUptimeMs_lessThanMinSystemUptime() {
        // MIN_SYSTEM_UPTIME_FOR_REMOTE_ACCESS_IN_SEC is 30s.
        when(mResources.getInteger(R.integer.config_allowedSystemUptimeForRemoteAccess))
                .thenReturn(10);

        mService = new CarRemoteAccessService(mContext, mSystemInterface, mPowerHalService);

        assertThat(mService.getAllowedSystemUptimeMs()).isEqualTo(30_000L);
    }

    @Test
    public void testCallbackOnRemoteTaskRequestedException_activieTasksCleared() throws Exception {
        runBootComplete();
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        RemoteAccessHalCallback halCallback = mService.getRemoteAccessHalCallback();
        ICarRemoteAccessCallback clientCallback = mock(ICarRemoteAccessCallback.class);
        IBinder mockBinder = mock(IBinder.class);
        when(clientCallback.asBinder()).thenReturn(mockBinder);
        doThrow(new RemoteException()).when(clientCallback).onRemoteTaskRequested(any(), any(),
                any(), anyInt());
        mService.addCarRemoteTaskClient(clientCallback);

        verify(clientCallback, timeout(WAIT_TIMEOUT_MS)).onClientRegistrationUpdated(
                mRegistrationInfoCaptor.capture());

        String clientId = mRegistrationInfoCaptor.getValue().getClientId();

        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);

        verify(clientCallback).onRemoteTaskRequested(eq(clientId), any(), eq(null), anyInt());
        assertThat(mService.getActiveTaskCount()).isEqualTo(0);
    }

    @Test
    public void testNotifyShutdownStarting() throws Exception {
        // Should be notified shutdown at 5100 - 5000 = 100ms.
        mService = newServiceWithSystemUpTime(5100);
        runBootComplete();
        mService.init();
        prepareCarRemoteTaskClient();

        PollingCheck.check("shutdownStarted is notified", 5000,
                () -> mRemoteAccessCallback.isShutdownStarting());
    }

    @Test
    public void testNotifyShutdownStarting_noNotifyVehicleInUse() throws Exception {
        // Should be notified shutdown at 5100 - 5000 = 100ms.
        mService = newServiceWithSystemUpTime(5100);
        runBootComplete();
        mService.init();
        prepareCarRemoteTaskClient();
        setVehicleInUse(true);

        SystemClock.sleep(1000);
        assertWithMessage("client is not notifyed shutdown when vehicle is in use").that(
                mRemoteAccessCallback.isShutdownStarting()).isFalse();
    }

    @Test
    public void testNotifyShutdownStarting_noNotifyNextPowerStateOn() throws Exception {
        // Should be notified shutdown at 5100 - 5000 = 100ms.
        mService = newServiceWithSystemUpTime(5100);
        when(mCarPowerManagementService.getLastShutdownState())
                .thenReturn(CarRemoteAccessManager.NEXT_POWER_STATE_ON);
        runBootComplete();
        mService.init();
        prepareCarRemoteTaskClient();

        SystemClock.sleep(1000);
        assertWithMessage("client is not notifyed shutdown when next power state is on").that(
                mRemoteAccessCallback.isShutdownStarting()).isFalse();
    }

    @Test
    public void testReportReadyForShutdown() throws Exception {
        mService = newServiceWithSystemUpTime(10000);
        runBootComplete();
        mService.init();
        ICarRemoteAccessCallback clientCallback1 = mock(ICarRemoteAccessCallback.class);
        IBinder mockBinder1 = mock(IBinder.class);
        when(clientCallback1.asBinder()).thenReturn(mockBinder1);
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        mService.addCarRemoteTaskClient(clientCallback1);
        verify(clientCallback1, timeout(WAIT_TIMEOUT_MS)).onClientRegistrationUpdated(
                mRegistrationInfoCaptor.capture());
        String clientId1 = mRegistrationInfoCaptor.getValue().getClientId();

        ICarRemoteAccessCallback clientCallback2 = mock(ICarRemoteAccessCallback.class);
        IBinder mockBinder2 = mock(IBinder.class);
        when(clientCallback2.asBinder()).thenReturn(mockBinder2);
        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_TWO);
        mService.addCarRemoteTaskClient(clientCallback2);
        verify(clientCallback2, timeout(WAIT_TIMEOUT_MS)).onClientRegistrationUpdated(
                mRegistrationInfoCaptor.capture());
        String clientId2 = mRegistrationInfoCaptor.getValue().getClientId();

        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_ONE);
        mService.confirmReadyForShutdown(clientId1);

        SystemClock.sleep(100);

        // clientCallback2 is still not ready for shutdown.
        verify(mCarPowerManagementService, never()).requestShutdownAp(anyInt(), anyBoolean());

        when(mDep.getCallingUid()).thenReturn(UID_PERMISSION_GRANTED_PACKAGE_TWO);
        // ClientId mismatch must throw exception.
        assertThrows(IllegalArgumentException.class,
                () -> mService.confirmReadyForShutdown(clientId1));

        mService.confirmReadyForShutdown(clientId2);

        verify(mCarPowerManagementService, timeout(WAIT_TIMEOUT_MS)).requestShutdownAp(
                CarRemoteAccessManager.NEXT_POWER_STATE_OFF, /* runGarageMode= */ false);
    }

    @Test
    public void testPendingRequestTimeout() throws Exception {
        mService.setMaxTaskPendingMs(100);
        mService.init();
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);

        // The task should be pending because client is not registered yet.
        halCallback.onRemoteTaskRequested(clientId, new byte[]{});

        SystemClock.sleep(100);

        assertThat(mRemoteAccessCallback.getTaskId()).isNull();

        // Register the client after max task pending time.
        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        SystemClock.sleep(100);

        assertThat(mRemoteAccessCallback.getTaskId()).isNull();
    }

    @Test
    public void testPendingRequestNotTimeout() throws Exception {
        mService.setMaxTaskPendingMs(1000);
        mService.init();
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        mService.removeCarRemoteTaskClient(mRemoteAccessCallback);

        // The task should be pending because client is not registered yet.
        halCallback.onRemoteTaskRequested(clientId, new byte[]{});

        SystemClock.sleep(100);

        assertThat(mRemoteAccessCallback.getTaskId()).isNull();

        // Register the client before max task pending time.
        mService.addCarRemoteTaskClient(mRemoteAccessCallback);

        PollingCheck.check("Task is delivered", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
    }

    private ICarPowerStateListener getCarPowerStateListener() {
        ArgumentCaptor<ICarPowerStateListener> internalListenerCaptor =
                ArgumentCaptor.forClass(ICarPowerStateListener.class);
        verify(mCarPowerManagementService).registerListenerWithCompletion(
                internalListenerCaptor.capture());
        return internalListenerCaptor.getValue();
    }

    private RemoteAccessHalCallback prepareCarRemoteTaskClient() throws Exception {
        return prepareCarRemoteTaskClient(mRemoteAccessCallback,
                UID_PERMISSION_GRANTED_PACKAGE_ONE);
    }

    private RemoteAccessHalCallback prepareCarRemoteTaskClient(
            ICarRemoteAccessCallbackImpl callback, int uid) throws Exception {
        when(mDep.getCallingUid()).thenReturn(uid);
        RemoteAccessHalCallback halCallback = mService.getRemoteAccessHalCallback();
        mService.addCarRemoteTaskClient(callback);
        PollingCheck.check("Client is registered", WAIT_TIMEOUT_MS,
                () -> callback.getClientId() != null);
        return halCallback;
    }

    private void runBootComplete() {
        if (!mBootCompleted) {
            mBootCompleted = true;
            mBootComplete.run();
        }
    }

    private void prepareReportTaskDoneTest() throws Exception {
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient();
        String clientId = mRemoteAccessCallback.getClientId();
        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);
        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> mRemoteAccessCallback.getTaskId() != null);
    }

    private void prepareReportTaskDoneTest(ICarRemoteAccessCallbackImpl callback, int uid)
            throws Exception {
        runBootComplete();
        RemoteAccessHalCallback halCallback = prepareCarRemoteTaskClient(callback, uid);
        String clientId = callback.getClientId();
        halCallback.onRemoteTaskRequested(clientId, /* data= */ null);
        PollingCheck.check("onRemoteTaskRequested should be called", WAIT_TIMEOUT_MS,
                () -> callback.getTaskId() != null);
    }

    private void mockPackageInfo() {
        mockPackageInfo(AVAILABLE_PACKAGES.size());
    }

    private void mockPackageInfo(int size) {
        List<ResolveInfo> resolveInfos = new ArrayList<>(size);
        for (int i = 0; i < size; i++) {
            PackagePrepForTest packagePrep = AVAILABLE_PACKAGES.get(i);
            ResolveInfo resolveInfo = packagePrep.resolveInfo;
            resolveInfos.add(resolveInfo);
            String packageName = resolveInfo.serviceInfo.packageName;
            int permission = packagePrep.permissionGranted ? PackageManager.PERMISSION_GRANTED
                    : PackageManager.PERMISSION_DENIED;
            when(mPackageManager.checkPermission(Car.PERMISSION_USE_REMOTE_ACCESS, packageName))
                    .thenReturn(permission);
            when(mPackageManager.checkPermission(Car.PERMISSION_CONTROL_REMOTE_ACCESS, packageName))
                    .thenReturn(permission);
        }
        when(mPackageManager.queryIntentServicesAsUser(any(), anyInt(), any()))
                .thenReturn(resolveInfos);
        doAnswer(inv -> {
            Runnable runnable = inv.getArgument(0);
            mBootComplete = runnable;
            return null;
        }).when(mSystemInterface)
                .scheduleActionForBootCompleted(any(Runnable.class), any(Duration.class));
    }

    private static PackagePrepForTest createPackagePrepForTest(String packageName, String className,
            boolean permissionGranted, int uid) {
        PackagePrepForTest packagePrep = new PackagePrepForTest();
        packagePrep.resolveInfo = new ResolveInfo();
        packagePrep.resolveInfo.serviceInfo = new ServiceInfo();
        packagePrep.resolveInfo.serviceInfo.packageName = packageName;
        packagePrep.resolveInfo.serviceInfo.name = className;
        packagePrep.resolveInfo.serviceInfo.applicationInfo = new ApplicationInfo();
        packagePrep.resolveInfo.serviceInfo.applicationInfo.uid = uid;
        packagePrep.permissionGranted = permissionGranted;
        return packagePrep;
    }

    private static final class PackagePrepForTest {
        public ResolveInfo resolveInfo;
        public boolean permissionGranted;
    }

    private void setVehicleInUse(boolean inUse) {
        when(mPowerHalService.isVehicleInUse()).thenReturn(inUse);
    }

    private void setupDatabase() {
        for (int i = 0; i < PERSISTENT_CLIENTS.size(); i++) {
            ClientIdEntry entry = PERSISTENT_CLIENTS.get(i);
            mRemoteAccessStorage.updateClientId(entry);
        }
    }

    private static final class ICarRemoteAccessCallbackImpl extends ICarRemoteAccessCallback.Stub {
        private final Object mLock = new Object();

        @GuardedBy("mLock")
        private String mServiceName;
        @GuardedBy("mLock")
        private String mVehicleId;
        @GuardedBy("mLock")
        private String mProcessorId;
        @GuardedBy("mLock")
        private String mClientId;
        @GuardedBy("mLock")
        private String mTaskId;
        @GuardedBy("mLock")
        private byte[] mData;
        @GuardedBy("mLock")
        private boolean mShutdownStarting;
        @GuardedBy("mLock")
        private int mTaskMaxDurationInSec;

        @Override
        public void onClientRegistrationUpdated(RemoteTaskClientRegistrationInfo info) {
            synchronized (mLock) {
                mServiceName = info.getServiceId();
                mVehicleId = info.getVehicleId();
                mProcessorId = info.getProcessorId();
                mClientId = info.getClientId();
            }
        }

        @Override
        public void onClientRegistrationFailed() {
        }

        @Override
        public void onRemoteTaskRequested(String clientId, String taskId, byte[] data,
                int taskMaxDurationInSec) {
            synchronized (mLock) {
                mClientId = clientId;
                mTaskId = taskId;
                mData = data;
                mTaskMaxDurationInSec = taskMaxDurationInSec;
            }
        }

        @Override
        public void onShutdownStarting() {
            synchronized (mLock) {
                mShutdownStarting = true;
            }
        }

        public String getServiceName() {
            synchronized (mLock) {
                return mServiceName;
            }
        }

        public String getVehicleId() {
            synchronized (mLock) {
                return mVehicleId;
            }
        }

        public String getProcessorId() {
            synchronized (mLock) {
                return mProcessorId;
            }
        }

        public String getClientId() {
            synchronized (mLock) {
                return mClientId;
            }
        }

        public String getTaskId() {
            synchronized (mLock) {
                return mTaskId;
            }
        }

        public byte[] getData() {
            synchronized (mLock) {
                return mData;
            }
        }

        public int getTaskMaxDurationInSec() {
            synchronized (mLock) {
                return mTaskMaxDurationInSec;
            }
        }

        public boolean isShutdownStarting() {
            synchronized (mLock) {
                return mShutdownStarting;
            }
        }
    }
}
