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

package com.android.car.bluetooth;

import static android.car.settings.CarSettings.Secure.KEY_BLUETOOTH_DEVICES;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.BluetoothStatusCodes;
import android.bluetooth.BluetoothUuid;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelUuid;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.RequiresDevice;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Unit tests for {@link BluetoothDeviceManager}
 *
 * Run:
 * atest BluetoothDeviceManagerTest
 */
@RequiresDevice
public class BluetoothDeviceManagerTest extends AbstractExtendedMockitoBluetoothTestCase {
    private static final int CONNECT_TIMEOUT_MS = 8000;
    private static final int ADAPTER_STATE_ANY = 0;
    private static final int ADAPTER_STATE_OFF = 1;
    private static final int ADAPTER_STATE_OFF_NOT_PERSISTED = 2;
    private static final int ADAPTER_STATE_ON = 3;

    private static final List<String> EMPTY_DEVICE_LIST = Arrays.asList();
    private static final List<String> SINGLE_DEVICE_LIST = Arrays.asList("DE:AD:BE:EF:00:00");
    private static final List<String> SMALL_DEVICE_LIST = Arrays.asList(
            "DE:AD:BE:EF:00:00",
            "DE:AD:BE:EF:00:01",
            "DE:AD:BE:EF:00:02");
    private static final List<String> LARGE_DEVICE_LIST = Arrays.asList(
            "DE:AD:BE:EF:00:00",
            "DE:AD:BE:EF:00:01",
            "DE:AD:BE:EF:00:02",
            "DE:AD:BE:EF:00:03",
            "DE:AD:BE:EF:00:04",
            "DE:AD:BE:EF:00:05",
            "DE:AD:BE:EF:00:06",
            "DE:AD:BE:EF:00:07");

    private static final String EMPTY_SETTINGS_STRING = "";
    private static final String SINGLE_SETTINGS_STRING = makeSettingsString(SINGLE_DEVICE_LIST);
    private static final String SMALL_SETTINGS_STRING = makeSettingsString(SMALL_DEVICE_LIST);
    private static final String LARGE_SETTINGS_STRING = makeSettingsString(LARGE_DEVICE_LIST);

    private final String mSettingsKey = KEY_BLUETOOTH_DEVICES;

    private List<BluetoothDevice>  mDeviceList;

    BluetoothDeviceManager mDeviceManager;

    // Tests assume the auto connecting devices only support MAP
    private static final String MAP_CLIENT_ACTION =
            "android.bluetooth.mapmce.profile.action.CONNECTION_STATE_CHANGED";

    private static final String PAN_ACTION =
            "android.bluetooth.pan.profile.action.CONNECTION_STATE_CHANGED";

    private ParcelUuid[] mLocalUuids = new ParcelUuid[] {
            BluetoothUuid.MAP, BluetoothUuid.MNS};
    private ParcelUuid[] mUuids = new ParcelUuid[] {
            BluetoothUuid.MAS};

    private Handler mHandler;
    private static final Object HANDLER_TOKEN = new Object();

    private Context mTargetContext;
    private MockContext mMockContext;

    @Mock private BluetoothAdapter mMockBluetoothAdapter;
    @Mock private BluetoothManager mMockBluetoothManager;

    //--------------------------------------------------------------------------------------------//
    // Setup/TearDown                                                                             //
    //--------------------------------------------------------------------------------------------//

    @Before
    public void setUp() {
        mTargetContext = InstrumentationRegistry.getTargetContext();
        mMockContext = new MockContext(mTargetContext);

        setSettingsDeviceList("");
        assertSettingsContains("");

        mMockContext.addMockedSystemService(BluetoothManager.class, mMockBluetoothManager);
        when(mMockBluetoothManager.getAdapter()).thenReturn(mMockBluetoothAdapter);
        when(mMockBluetoothAdapter.getUuidsList()).thenReturn(Arrays.asList(mLocalUuids));

        /**
         * Mocks {@link BluetoothAdapter#getRemoteDevice(boolean)}
         */
        doAnswer(new Answer<BluetoothDevice>() {
            @Override
            public BluetoothDevice answer(InvocationOnMock invocation) throws Throwable {
                String bdAddr = "";
                Object[] arguments = invocation.getArguments();
                if (arguments != null && arguments.length == 1 && arguments[0] != null) {
                    bdAddr = (String) arguments[0];
                }
                return createDevice(bdAddr);
            }
        }).when(mMockBluetoothAdapter).getRemoteDevice(anyString());

        mHandler = new Handler(Looper.getMainLooper());

        mDeviceManager = BluetoothDeviceManager.create(mMockContext);
        Assert.assertTrue(mDeviceManager != null);

        mDeviceList = new ArrayList<BluetoothDevice>();
    }

    @After
    public void tearDown() {
        mDeviceList = null;
        if (mDeviceManager != null) {
            mDeviceManager.stop();
            mDeviceManager = null;
        }
        if (mHandler != null) {
            mHandler.removeCallbacksAndMessages(HANDLER_TOKEN);
            mHandler = null;
        }
        if (mMockContext != null) {
            mMockContext.release();
            mMockContext = null;
        }
    }

    //--------------------------------------------------------------------------------------------//
    // Utilities                                                                                  //
    //--------------------------------------------------------------------------------------------//

    private void setSettingsDeviceList(String devicesStr) {
        Settings.Secure.putString(mMockContext.getContentResolver(), mSettingsKey, devicesStr);
    }

    private String getSettingsDeviceList() {
        String devices = Settings.Secure.getString(mMockContext.getContentResolver(), mSettingsKey);
        return devices == null ? "" : devices;
    }

    private ArrayList<BluetoothDevice> makeDeviceList(List<String> addresses) {
        ArrayList<BluetoothDevice> devices = new ArrayList<>();
        for (String address : addresses) {
            BluetoothDevice device = createDevice(address, mUuids);
            if (device == null) continue;
            devices.add(device);
        }
        return devices;
    }

    private static String makeSettingsString(List<String> addresses) {
        return TextUtils.join(",", addresses);
    }

    private void setPreconditionsAndStart(int adapterState, String settings,
            List<String> devices) {
        switch (adapterState) {
            case ADAPTER_STATE_ON:
                when(mMockBluetoothAdapter.getState()).thenReturn(BluetoothAdapter.STATE_ON);
                Settings.Global.putInt(mMockContext.getContentResolver(),
                        Settings.Global.BLUETOOTH_ON, 1);
                break;
            case ADAPTER_STATE_OFF:
                when(mMockBluetoothAdapter.getState()).thenReturn(BluetoothAdapter.STATE_OFF);
                Settings.Global.putInt(mMockContext.getContentResolver(),
                        Settings.Global.BLUETOOTH_ON, 0);
                break;
            case ADAPTER_STATE_OFF_NOT_PERSISTED:
                when(mMockBluetoothAdapter.getState()).thenReturn(BluetoothAdapter.STATE_OFF);
                break;
            case ADAPTER_STATE_ANY:
                break;
            default:
                break;
        }

        setSettingsDeviceList(settings);

        mDeviceList = makeDeviceList(devices);

        mDeviceManager.start();

        for (BluetoothDevice device : mDeviceList) {
            mDeviceManager.addDevice(device);
        }
    }

    private void sendAdapterStateChanged(int newState) {
        Assert.assertTrue(mMockContext != null);
        Intent intent = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
        intent.putExtra(BluetoothAdapter.EXTRA_STATE, newState);
        mMockContext.sendBroadcast(intent);
    }

    private void sendBondStateChanged(BluetoothDevice device, int newState) {
        Assert.assertTrue(mMockContext != null);
        Intent intent = new Intent(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothDevice.EXTRA_BOND_STATE, newState);
        mMockContext.sendBroadcast(intent);
    }

    private void sendConnectionStateChanged(String profile, BluetoothDevice device, int newState) {
        Assert.assertTrue(mMockContext != null);
        Intent intent = new Intent(profile);
        intent.putExtra(BluetoothDevice.EXTRA_DEVICE, device);
        intent.putExtra(BluetoothProfile.EXTRA_STATE, newState);
        mMockContext.sendBroadcast(intent);
    }

    private void assertSettingsContains(String expected) {
        Assert.assertTrue(expected != null);
        String settings = getSettingsDeviceList();
        if (settings == null) settings = "";
        Assert.assertEquals(expected, settings);
    }

    private void assertDeviceList(List<String> expected) {
        List<BluetoothDevice> devices = mDeviceManager.getDeviceListSnapshot();
        ArrayList<BluetoothDevice> expectedDevices = makeDeviceList(expected);
        Assert.assertTrue(devices != null);
        Assert.assertEquals(expectedDevices.size(), devices.size());
        for (int i = 0; i < expectedDevices.size(); i++) {
            BluetoothDevice expectedDevice = expectedDevices.get(i);
            BluetoothDevice device = devices.get(i);
            Assert.assertEquals(expectedDevice.getAddress(), device.getAddress());
        }
    }

    private BluetoothDevice createDevice(String bdAddr) {
        return createDevice(bdAddr, null);
    }

    private BluetoothDevice createDevice(String bdAddr, ParcelUuid[] uuids) {
        BluetoothDevice device = mock(BluetoothDevice.class);
        when(device.getAddress()).thenReturn(bdAddr);
        when(device.getName()).thenReturn(bdAddr);
        when(device.getUuids()).thenReturn(uuids);
        when(device.connect()).thenReturn(BluetoothStatusCodes.SUCCESS);
        when(device.disconnect()).thenReturn(BluetoothStatusCodes.SUCCESS);
        return device;
    }

    //--------------------------------------------------------------------------------------------//
    // Load from persistent memory tests                                                          //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - Settings contains no devices
     *
     * Actions:
     * - Initialize the device manager
     *
     * Outcome:
     * - device manager should initialize
     */
    @Test
    public void testEmptySettingsString_loadNoDevices() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        assertDeviceList(EMPTY_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - Settings contains a single device
     *
     * Actions:
     * - Initialize the device manager
     *
     * Outcome:
     * - The single device is now located in the device manager's device list
     */
    @Test
    public void testSingleDeviceSettingsString_loadSingleDevice() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, SINGLE_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        assertDeviceList(SINGLE_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - Settings contains several devices
     *
     * Actions:
     * - Initialize the device manager
     *
     * Outcome:
     * - All devices are now in the device manager's list, all in the proper order.
     */
    @Test
    public void testSeveralDevicesSettingsString_loadAllDevices() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, LARGE_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        assertDeviceList(LARGE_DEVICE_LIST);
    }

    //--------------------------------------------------------------------------------------------//
    // Commit to persistent memory tests                                                          //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized and the list contains no devices
     *
     * Actions:
     * - An event forces the device manager to commit it's list
     *
     * Outcome:
     * - The empty list should be written to Settings.Secure as an empty string, ""
     */
    @Test
    public void testNoDevicesCommit_commitEmptyDeviceString() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        sendAdapterStateChanged(BluetoothAdapter.STATE_OFF);
        assertSettingsContains(EMPTY_SETTINGS_STRING);
    }

    /**
     * Preconditions:
     * - The device manager contains several devices
     *
     * Actions:
     * - An event forces the device manager to commit it's list
     *
     * Outcome:
     * - The ordered device list should be written to Settings.Secure as a comma separated list
     */
    @Test
    public void testSeveralDevicesCommit_commitAllDeviceString() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, LARGE_DEVICE_LIST);
        sendAdapterStateChanged(BluetoothAdapter.STATE_OFF);
        assertSettingsContains(LARGE_SETTINGS_STRING);
    }

    //--------------------------------------------------------------------------------------------//
    // Add Device tests                                                                           //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized and contains no devices.
     *
     * Actions:
     * - Add a single device
     *
     * Outcome:
     * - The device manager priority list contains the single device
     */
    @Test
    public void testAddSingleDevice_devicesAppearInPriorityList() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SINGLE_DEVICE_LIST);
        assertDeviceList(SINGLE_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains no devices.
     *
     * Actions:
     * - Add several devices
     *
     * Outcome:
     * - The device manager priority list contains all devices, ordered properly
     */
    @Test
    public void testAddMultipleDevices_devicesAppearInPriorityList() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, LARGE_DEVICE_LIST);
        assertDeviceList(LARGE_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains one device
     *
     * Actions:
     * - Add the device that is already in the list
     *
     * Outcome:
     * - The device manager's list remains unchanged with only one device in it
     */
    @Test
    public void testAddDeviceAlreadyInList_priorityListUnchanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SINGLE_DEVICE_LIST);
        BluetoothDevice device = mDeviceList.get(0);
        mDeviceManager.addDevice(device);
        assertDeviceList(SINGLE_DEVICE_LIST);
    }

    //--------------------------------------------------------------------------------------------//
    // Remove Device tests                                                                        //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized and contains no devices.
     *
     * Actions:
     * - Remove a device from the list
     *
     * Outcome:
     * - The device manager does not error out and continues to have an empty list
     */
    @Test
    public void testRemoveDeviceFromEmptyList_priorityListUnchanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        mDeviceManager.removeDevice(createDevice("DE:AD:BE:EF:00:00"));
        assertDeviceList(EMPTY_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Remove the device with the highest priority (front of list)
     *
     * Outcome:
     * - The device manager removes the leading device. The other devices have been shifted down.
     */
    @Test
    public void testRemoveDeviceFront_deviceNoLongerInPriorityList() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        BluetoothDevice device = mDeviceList.get(0);
        mDeviceManager.removeDevice(device);
        ArrayList<String> expected = new ArrayList(SMALL_DEVICE_LIST);
        expected.remove(0);
        assertDeviceList(expected);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Remove a device from the middle of the list
     *
     * Outcome:
     * - The device manager removes the device. The other devices with larger priorities have been
     *   shifted down.
     */
    @Test
    public void testRemoveDeviceMiddle_deviceNoLongerInPriorityList() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        BluetoothDevice device = mDeviceList.get(1);
        mDeviceManager.removeDevice(device);
        ArrayList<String> expected = new ArrayList(SMALL_DEVICE_LIST);
        expected.remove(1);
        assertDeviceList(expected);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Remove the device from the end of the list
     *
     * Outcome:
     * - The device manager removes the device. The other devices remain in their places, unchanged
     */
    @Test
    public void testRemoveDeviceEnd_deviceNoLongerInPriorityList() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        BluetoothDevice device = mDeviceList.get(2);
        mDeviceManager.removeDevice(device);
        ArrayList<String> expected = new ArrayList(SMALL_DEVICE_LIST);
        expected.remove(2); // 00, 01
        assertDeviceList(expected);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Remove a device thats not in the list
     *
     * Outcome:
     * - The device manager's list remains unchanged.
     */
    @Test
    public void testRemoveDeviceNotInList_priorityListUnchanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        BluetoothDevice device = createDevice("10:20:30:40:50:60");
        mDeviceManager.removeDevice(device);
        assertDeviceList(SMALL_DEVICE_LIST);
    }

    //--------------------------------------------------------------------------------------------//
    // GetDeviceConnectionPriority() tests                                                        //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Get the priority of each device in the list
     *
     * Outcome:
     * - The device manager returns the proper priority for each device
     */
    @Test
    public void testGetConnectionPriority_prioritiesReturned() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        for (int i = 0; i < mDeviceList.size(); i++) {
            BluetoothDevice device = mDeviceList.get(i);
            int priority = mDeviceManager.getDeviceConnectionPriority(device);
            Assert.assertEquals(i, priority);
        }
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Get the priority of a device that is not in the list
     *
     * Outcome:
     * - The device manager returns a -1
     */
    @Test
    public void testGetConnectionPriorityDeviceNotInList_negativeOneReturned() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        BluetoothDevice deviceNotPresent = createDevice("10:20:30:40:50:60");
        int priority = mDeviceManager.getDeviceConnectionPriority(deviceNotPresent);
        Assert.assertEquals(-1, priority);
    }

    //--------------------------------------------------------------------------------------------//
    // setDeviceConnectionPriority() tests                                                        //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Set the priority of several devices in the list, testing the following moves:
     *      Mid priority -> higher priority
     *      Mid priority -> lower priority
     *      Highest priority -> lower priority
     *      Lowest priority -> higher priority
     *      Any priority -> same priority
     *
     * Outcome:
     * - Increased prioritied shuffle devices to proper lower priorities, decreased priorities
     *   shuffle devices to proper high priorities, and a request to set the same priority yields no
     *   change.
     */
    @Test
    public void testSetConnectionPriority_listOrderedCorrectly() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);

        // move middle device to front
        BluetoothDevice device = mDeviceList.get(1);
        mDeviceManager.setDeviceConnectionPriority(device, 0);
        Assert.assertEquals(0, mDeviceManager.getDeviceConnectionPriority(device));
        ArrayList<String> expected = new ArrayList(SMALL_DEVICE_LIST);
        Collections.swap(expected, 1, 0); // expected: 00, [01], 02 -> [01], 00, 02
        assertDeviceList(expected);

        // move front device to the end
        mDeviceManager.setDeviceConnectionPriority(device, 2);
        Assert.assertEquals(2, mDeviceManager.getDeviceConnectionPriority(device));
        Collections.swap(expected, 0, 2); // expected: [01], 00, 02 -> 00, 02, [01]
        Collections.swap(expected, 0, 1);
        assertDeviceList(expected);

        // move end device to middle
        mDeviceManager.setDeviceConnectionPriority(device, 1);
        Assert.assertEquals(1, mDeviceManager.getDeviceConnectionPriority(device));
        Collections.swap(expected, 2, 1); // expected: 00, 02, [01] -> 00, [01], 02
        assertDeviceList(expected);

        // move middle to end
        mDeviceManager.setDeviceConnectionPriority(device, 2);
        Assert.assertEquals(2, mDeviceManager.getDeviceConnectionPriority(device));
        Collections.swap(expected, 1, 2); // expected: 00, [01], 02 -> 00, 02, [01]
        assertDeviceList(expected);

        // move end to front
        mDeviceManager.setDeviceConnectionPriority(device, 0);
        Assert.assertEquals(0, mDeviceManager.getDeviceConnectionPriority(device));
        Collections.swap(expected, 2, 0); // expected: 00, 02, [01] -> [01], 00, 02
        Collections.swap(expected, 1, 2);
        assertDeviceList(expected);

        // move front to middle
        mDeviceManager.setDeviceConnectionPriority(device, 1);
        Assert.assertEquals(1, mDeviceManager.getDeviceConnectionPriority(device));
        Collections.swap(expected, 0, 1); // expected: [01], 00, 02 -> 00, [01], 02
        assertDeviceList(expected);

        // move middle to middle (i.e same to same)
        mDeviceManager.setDeviceConnectionPriority(device, 1);
        Assert.assertEquals(1, mDeviceManager.getDeviceConnectionPriority(device));
        assertDeviceList(expected); // expected: 00, [01], 02 -> 00, [01], 02
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Set the priority of a device that is not currently in the list
     *
     * Outcome:
     * - Device is added to the list in the requested spot. Devices with lower priorities have had
     *   their priorities adjusted accordingly.
     */
    @Test
    public void testSetConnectionPriorityNewDevice_listOrderedCorrectly() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);

        // add new device to the middle
        BluetoothDevice device = createDevice("10:20:30:40:50:60");
        mDeviceManager.setDeviceConnectionPriority(device, 1);
        Assert.assertEquals(1, mDeviceManager.getDeviceConnectionPriority(device));
        ArrayList<String> expected = new ArrayList(SMALL_DEVICE_LIST);
        expected.add(1, "10:20:30:40:50:60"); // 00, 60, 01, 02
        assertDeviceList(expected);

        // add new device to the front
        device = createDevice("10:20:30:40:50:61");
        mDeviceManager.setDeviceConnectionPriority(device, 0);
        Assert.assertEquals(0, mDeviceManager.getDeviceConnectionPriority(device));
        expected.add(0, "10:20:30:40:50:61"); // 61, 00, 60, 01, 02
        assertDeviceList(expected);

        // add new device to the end
        device = createDevice("10:20:30:40:50:62");
        mDeviceManager.setDeviceConnectionPriority(device, 5);
        Assert.assertEquals(5, mDeviceManager.getDeviceConnectionPriority(device));
        expected.add(5, "10:20:30:40:50:62"); // 61, 00, 60, 01, 02, 62
        assertDeviceList(expected);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Request to set a priority that exceeds the bounds of the list (upper)
     *
     * Outcome:
     * - No operation is taken
     */
    @Test
    public void testSetConnectionPriorityLargerThanSize_priorityListUnchanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);

        // Attempt to move middle device to end with huge end priority
        BluetoothDevice device = mDeviceList.get(1);
        mDeviceManager.setDeviceConnectionPriority(device, 100000);
        Assert.assertEquals(1, mDeviceManager.getDeviceConnectionPriority(device));
        assertDeviceList(SMALL_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Request to set a priority that exceeds the bounds of the list (lower)
     *
     * Outcome:
     * - No operation is taken
     */
    @Test
    public void testSetConnectionPriorityNegative_priorityListUnchanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);

        // Attempt to move middle device to negative priority
        BluetoothDevice device = mDeviceList.get(1);
        mDeviceManager.setDeviceConnectionPriority(device, -1);
        assertDeviceList(SMALL_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized and contains several devices.
     *
     * Actions:
     * - Request to set a priority for a null device
     *
     * Outcome:
     * - No operation is taken
     */
    @Test
    public void testSetConnectionPriorityNullDevice_priorityListUnchanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        mDeviceManager.setDeviceConnectionPriority(null, 1);
        assertDeviceList(SMALL_DEVICE_LIST);
    }

    //--------------------------------------------------------------------------------------------//
    // beginAutoConnecting() tests                                                                //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The Bluetooth adapter is ON, the device manager is initialized and no devices are in the
     *   list.
     *
     * Actions:
     * - Initiate an auto connection
     *
     * Outcome:
     * - Auto connect returns immediately with no connection attempts made.
     */
    @Test
    public void testAutoConnectNoDevices_returnsImmediately() throws Exception {
        setPreconditionsAndStart(ADAPTER_STATE_ON, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        mDeviceManager.beginAutoConnecting();
        for (BluetoothDevice device : mDeviceList) {
            verify(device, never()).connect();
        }
        Assert.assertFalse(mDeviceManager.isAutoConnecting());
    }

    /**
     * Preconditions:
     * - The Bluetooth adapter is OFF, the device manager is initialized and there are several
     *    devices are in the list.
     *
     * Actions:
     * - Initiate an auto connection
     *
     * Outcome:
     * - Auto connect returns immediately with no connection attempts made.
     */
    @Test
    public void testAutoConnectAdapterOff_returnsImmediately() throws Exception {
        setPreconditionsAndStart(ADAPTER_STATE_OFF, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        mDeviceManager.beginAutoConnecting();
        for (BluetoothDevice device : mDeviceList) {
            verify(device, never()).connect();
        }
        Assert.assertFalse(mDeviceManager.isAutoConnecting());
    }

    /**
     * Preconditions:
     * - The Bluetooth adapter is ON, the device manager is initialized and there are several
     *    devices are in the list.
     *
     * Actions:
     * - Initiate an auto connection
     *
     * Outcome:
     * - Auto connect attempts to connect each device in the list, in order of priority.
     */
    @Test
    public void testAutoConnectSeveralDevices_attemptsToConnectEachDevice() throws Exception {
        setPreconditionsAndStart(ADAPTER_STATE_ON, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);

        mDeviceManager.beginAutoConnecting();

        sendConnectionStateChanged(MAP_CLIENT_ACTION, mDeviceList.get(0),
                BluetoothProfile.STATE_CONNECTED);
        verify(mDeviceList.get(0), timeout(CONNECT_TIMEOUT_MS).times(1)).connect();

        sendConnectionStateChanged(MAP_CLIENT_ACTION, mDeviceList.get(1),
                BluetoothProfile.STATE_CONNECTED);
        verify(mDeviceList.get(1), timeout(CONNECT_TIMEOUT_MS).times(1)).connect();

        sendConnectionStateChanged(MAP_CLIENT_ACTION, mDeviceList.get(2),
                BluetoothProfile.STATE_CONNECTED);
        verify(mDeviceList.get(2), timeout(CONNECT_TIMEOUT_MS).times(1)).connect();
    }

    //--------------------------------------------------------------------------------------------//
    // Bluetooth stack device connection status changed event tests                               //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized, there are no devices in the list.
     *
     * Actions:
     * - A connection action comes in for a profile and the device's priority is
     *   CONNECTION_POLICY_ALLOWED.
     *
     * Outcome:
     * - The device is added to the list. Related/configured trigger profiles are connected.
     */
    @Test
    public void testReceiveDeviceConnectionProfileNotPan_deviceAdded() throws Exception {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        BluetoothDevice device = createDevice(SINGLE_DEVICE_LIST.get(0));
        sendConnectionStateChanged(MAP_CLIENT_ACTION, device, BluetoothProfile.STATE_CONNECTED);
        assertDeviceList(SINGLE_DEVICE_LIST);
        verify(device, times(1)).connect();
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are no devices in the list.
     *
     * Actions:
     * - A connection action comes in for the PAN profile and the device's priority is
     *   CONNECTION_POLICY_ALLOWED.
     *
     * Outcome:
     * - The device is added to the list. Related/configured trigger profiles are *not* connected.
     */
    @Test
    public void testReceiveDeviceConnectionProfilePan_deviceAdded() throws Exception {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        BluetoothDevice device = createDevice(SINGLE_DEVICE_LIST.get(0));
        sendConnectionStateChanged(PAN_ACTION, device, BluetoothProfile.STATE_CONNECTED);
        assertDeviceList(SINGLE_DEVICE_LIST);
        verify(device, never()).connect();
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are is one device in the list.
     *
     * Actions:
     * - A disconnection action comes in for the profile we're tracking and the device's priority is
     *   CONNECTION_POLICY_ALLOWED.
     *
     * Outcome:
     * - The device list is unchanged.
     */
    @Test
    public void testReceiveDeviceDisconnection_listUnchanged() throws Exception {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SINGLE_DEVICE_LIST);
        BluetoothDevice device = mDeviceList.get(0);
        sendConnectionStateChanged(MAP_CLIENT_ACTION, device, BluetoothProfile.STATE_DISCONNECTED);
        assertDeviceList(SINGLE_DEVICE_LIST);
    }

    //--------------------------------------------------------------------------------------------//
    // Bluetooth stack device bond status changed event tests                                     //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized, there is one device in the list.
     *
     * Actions:
     * - A device from the list has unbonded
     *
     * Outcome:
     * - The device is removed from the list.
     */
    @Test
    public void testReceiveDeviceUnbonded_deviceRemoved() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SINGLE_DEVICE_LIST);
        BluetoothDevice device = mDeviceList.get(0);
        sendBondStateChanged(device, BluetoothDevice.BOND_NONE);
        assertDeviceList(EMPTY_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are no devices in the list.
     *
     * Actions:
     * - A device has bonded.
     *
     * Outcome:
     * - Successful bonds don't impact the list, only device connections. The list is unchanged.
     */
    @Test
    public void testReceiveDeviceBonded_deviceListNotChanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        BluetoothDevice device = createDevice(SINGLE_DEVICE_LIST.get(0));
        sendBondStateChanged(device, BluetoothDevice.BOND_BONDED);
        assertDeviceList(EMPTY_DEVICE_LIST);
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are no devices in the list.
     *
     * Actions:
     * - A device is bonding.
     *
     * Outcome:
     * - A mid-bond event don't impact the list, only device connections. The list is unchanged.
     */
    @Test
    public void testReceiveDeviceBonding_deviceListNotChanged() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, EMPTY_DEVICE_LIST);
        BluetoothDevice device = createDevice(SINGLE_DEVICE_LIST.get(0));
        sendBondStateChanged(device, BluetoothDevice.BOND_BONDING);
        assertDeviceList(EMPTY_DEVICE_LIST);
    }

    //--------------------------------------------------------------------------------------------//
    // Bluetooth stack adapter status changed event tests                                         //
    //--------------------------------------------------------------------------------------------//

    /**
     * Preconditions:
     * - The device manager is initialized, there are several devices in the list. The adapter is on
     *   and we are currently connecting devices.
     *
     * Actions:
     * - The adapter is turning off
     *
     * Outcome:
     * - Auto-connecting is cancelled
     */
    @Test
    public void testReceiveAdapterTurningOff_cancel() {
        setPreconditionsAndStart(ADAPTER_STATE_ON, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        mDeviceManager.beginAutoConnecting();
        Assert.assertTrue(mDeviceManager.isAutoConnecting());
        // We have 24 seconds of auto connecting time while we force it to quit
        sendAdapterStateChanged(BluetoothAdapter.STATE_TURNING_OFF);
        Assert.assertFalse(mDeviceManager.isAutoConnecting());
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are several devices in the list. The adapter is on
     *   and we are currently connecting devices.
     *
     * Actions:
     * - The adapter becomes off
     *
     * Outcome:
     * - Auto-connecting is cancelled. The device list is committed
     */
    @Test
    public void testReceiveAdapterOff_cancelAndCommit() {
        setPreconditionsAndStart(ADAPTER_STATE_ON, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        mDeviceManager.beginAutoConnecting();
        Assert.assertTrue(mDeviceManager.isAutoConnecting());
        // We have 24 seconds of auto connecting time while we force it to quit
        sendAdapterStateChanged(BluetoothAdapter.STATE_OFF);
        Assert.assertFalse(mDeviceManager.isAutoConnecting());
        assertSettingsContains(SMALL_SETTINGS_STRING);
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are several devices in the list. The adapter is on
     *   and we are currently connecting devices.
     *
     * Actions:
     * - The adapter sends a turning on. (This can happen in weird cases in the stack where the
     *   adapter is ON but the intent is sent away. Additionally, being ON and sending the intent is
     *   a great way to make sure we called cancel)
     *
     * Outcome:
     * - Auto-connecting is cancelled
     */
    @Test
    public void testReceiveAdapterTurningOn_cancel() {
        setPreconditionsAndStart(ADAPTER_STATE_ON, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        mDeviceManager.beginAutoConnecting();
        Assert.assertTrue(mDeviceManager.isAutoConnecting());
        sendAdapterStateChanged(BluetoothAdapter.STATE_TURNING_ON);
        Assert.assertFalse(mDeviceManager.isAutoConnecting());
    }

    /**
     * Preconditions:
     * - The device manager is initialized, there are several devices in the list.
     *
     * Actions:
     * - The adapter becomes on
     *
     * Outcome:
     * - No connection actions are taken
     */
    @Test
    public void testReceiveAdapterOn_doNothing() {
        setPreconditionsAndStart(ADAPTER_STATE_ANY, EMPTY_SETTINGS_STRING, SMALL_DEVICE_LIST);
        sendAdapterStateChanged(BluetoothAdapter.STATE_ON);
        for (BluetoothDevice device : mDeviceList) {
            verify(device, never()).connect();
        }
    }
}
