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

package com.android.car.carlauncher;

import static android.car.settings.CarSettings.Secure.KEY_PACKAGES_DISABLED_ON_RESOURCE_OVERUSE;
import static android.content.pm.ApplicationInfo.CATEGORY_AUDIO;
import static android.content.pm.ApplicationInfo.CATEGORY_VIDEO;
import static android.content.pm.PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
import static android.content.pm.PackageManager.MATCH_DISABLED_UNTIL_USED_COMPONENTS;

import static com.android.car.carlauncher.AppLauncherUtils.APP_TYPE_LAUNCHABLES;
import static com.android.car.carlauncher.AppLauncherUtils.APP_TYPE_MEDIA_SERVICES;
import static com.android.car.carlauncher.AppLauncherUtils.PACKAGES_DISABLED_ON_RESOURCE_OVERUSE_SEPARATOR;
import static com.android.dx.mockito.inline.extended.ExtendedMockito.doReturn;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.car.Car;
import android.car.content.pm.CarPackageManager;
import android.car.media.CarMediaManager;
import android.car.test.mocks.AbstractExtendedMockitoTestCase;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.LauncherActivityInfo;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.res.Resources;
import android.os.Bundle;
import android.os.UserHandle;
import android.provider.Settings;
import android.service.media.MediaBrowserService;
import android.util.ArraySet;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;
import java.util.stream.Collectors;

@RunWith(AndroidJUnit4.class)
@SmallTest
public final class AppLauncherUtilsTest extends AbstractExtendedMockitoTestCase {
    private static final String TEST_DISABLED_APP_1 = "com.android.car.test.disabled1";
    private static final String TEST_DISABLED_APP_2 = "com.android.car.test.disabled2";
    private static final String TEST_ENABLED_APP = "com.android.car.test.enabled";
    // Default media app
    private static final String TEST_MEDIA_TEMPLATE_MBS = "com.android.car.test.mbs";
    // Video app that has a MBS defined but has its own launch activity
    private static final String TEST_VIDEO_MBS = "com.android.car.test.video.mbs";
    // NDO App that has opted in its MBS to launch in car
    private static final String TEST_NDO_MBS_LAUNCHABLE = "com.android.car.test.mbs.launchable";
    // NDO App that has opted out its MBS to launch in car
    private static final String TEST_NDO_MBS_NOT_LAUNCHABLE =
            "com.android.car.test.mbs.notlaunchable";

    private static final String CUSTOM_MEDIA_PACKAGE = "com.android.car.radio";
    private static final String CUSTOM_MEDIA_CLASS = "com.android.car.radio.service";
    private static final String CUSTOM_MEDIA_COMPONENT = CUSTOM_MEDIA_PACKAGE
            + "/" + CUSTOM_MEDIA_CLASS;

    private static final String TEST_MIRROR_APP_PKG = "com.android.car.test.mirroring";

    @Mock private Context mMockContext;
    @Mock private LauncherApps mMockLauncherApps;
    @Mock private PackageManager mMockPackageManager;
    @Mock private AppLauncherUtils.ShortcutsListener mMockShortcutsListener;

    @Mock private Resources mResources;

    @Mock private LauncherActivityInfo mRadioLauncherActivityInfo;

    private CarMediaManager mCarMediaManager;
    private CarPackageManager mCarPackageManager;
    private Car mCar;

    @Before
    public void setUp() throws Exception {
        mCar = Car.createCar(mMockContext, /* handler = */ null, Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER,
                (car, ready) -> {
                    if (!ready) {
                        mCarPackageManager = null;
                        mCarMediaManager = null;
                        return;
                    }
                    mCarPackageManager = (CarPackageManager) car.getCarManager(Car.PACKAGE_SERVICE);
                    mCarMediaManager = (CarMediaManager) car.getCarManager(Car.CAR_MEDIA_SERVICE);
                    when(mMockContext.getPackageManager()).thenReturn(mMockPackageManager);
                });
    }

    @After
    public void tearDown() throws Exception {
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
    }

    @Override
    protected void onSessionBuilder(CustomMockitoSessionBuilder session) {
        session.spyStatic(Settings.Secure.class);
    }

    @Test
    public void testGetLauncherApps_MediaCenterAppSwitcher() {
        mockSettingsStringCalls();
        mockPackageManagerQueries();

        when(mMockContext.getResources()).thenReturn(mResources);
        when(mResources.getStringArray(eq(
                com.android.car.media.common.R.array.custom_media_packages)))
                .thenReturn(new String[]{CUSTOM_MEDIA_COMPONENT});

        // Setup custom media component
        when(mMockLauncherApps.getActivityList(any(), any()))
                .thenReturn(List.of(mRadioLauncherActivityInfo));
        when(mRadioLauncherActivityInfo.getComponentName())
                .thenReturn(new ComponentName(CUSTOM_MEDIA_PACKAGE, CUSTOM_MEDIA_CLASS));
        when(mRadioLauncherActivityInfo.getName())
                .thenReturn(CUSTOM_MEDIA_CLASS);

        AppLauncherUtils.LauncherAppsInfo launcherAppsInfo = AppLauncherUtils.getLauncherApps(
                mMockContext, /* appsToHide= */ new ArraySet<>(),
                /* appTypes= */ APP_TYPE_MEDIA_SERVICES,
                /* openMediaCenter= */ false, mMockLauncherApps, mCarPackageManager,
                mMockPackageManager, mCarMediaManager, mMockShortcutsListener,
                TEST_MIRROR_APP_PKG,  /* mirroringAppRedirect= */ null);

        List<AppMetaData> appMetaData = launcherAppsInfo.getLaunchableComponentsList();

        // Only media apps should be present
        assertEquals(Set.of(
                        TEST_MEDIA_TEMPLATE_MBS,
                        TEST_NDO_MBS_LAUNCHABLE,
                        CUSTOM_MEDIA_PACKAGE),
                appMetaData.stream()
                        .map(am -> am.getComponentName().getPackageName())
                        .collect(Collectors.toSet()));

        // This should include all MBS discovered
        assertEquals(5, launcherAppsInfo.getMediaServices().size());

        mockPmGetApplicationEnabledSetting(COMPONENT_ENABLED_STATE_ENABLED, TEST_DISABLED_APP_1,
                TEST_DISABLED_APP_2);

        launchAllApps(appMetaData);

        // Media apps should do only switching and not launch activity
        verify(mMockContext, never()).startActivity(any(), any());
    }
    @Test
    public void testGetLauncherApps_Launcher() {
        mockSettingsStringCalls();
        mockPackageManagerQueries();

        when(mMockContext.getResources()).thenReturn(mResources);
        when(mResources.getStringArray(eq(
                com.android.car.media.common.R.array.custom_media_packages)))
                .thenReturn(new String[]{CUSTOM_MEDIA_COMPONENT});

        // Setup custom media component
        when(mMockLauncherApps.getActivityList(any(), any()))
                .thenReturn(List.of(mRadioLauncherActivityInfo));
        when(mRadioLauncherActivityInfo.getComponentName())
                .thenReturn(new ComponentName(CUSTOM_MEDIA_PACKAGE, CUSTOM_MEDIA_CLASS));
        when(mRadioLauncherActivityInfo.getName())
                .thenReturn(CUSTOM_MEDIA_CLASS);

        AppLauncherUtils.LauncherAppsInfo launcherAppsInfo = AppLauncherUtils.getLauncherApps(
                mMockContext, /* appsToHide= */ new ArraySet<>(),
                /* appTypes= */ APP_TYPE_LAUNCHABLES + APP_TYPE_MEDIA_SERVICES,
                /* openMediaCenter= */ true, mMockLauncherApps, mCarPackageManager,
                mMockPackageManager, mCarMediaManager, mMockShortcutsListener,
                TEST_MIRROR_APP_PKG,  /* mirroringAppRedirect= */ null);

        List<AppMetaData> appMetaData = launcherAppsInfo.getLaunchableComponentsList();
        // mMockLauncherApps is never stubbed, only services & disabled activities are expected.

        assertEquals(Set.of(
                        TEST_MEDIA_TEMPLATE_MBS,
                        TEST_NDO_MBS_LAUNCHABLE,
                        CUSTOM_MEDIA_PACKAGE,
                        TEST_DISABLED_APP_1,
                        TEST_DISABLED_APP_2),
                appMetaData.stream()
                        .map(am -> am.getComponentName().getPackageName())
                        .collect(Collectors.toSet()));


        // This should include all MBS discovered
        assertEquals(5, launcherAppsInfo.getMediaServices().size());

        mockPmGetApplicationEnabledSetting(COMPONENT_ENABLED_STATE_ENABLED, TEST_DISABLED_APP_1,
                TEST_DISABLED_APP_2);

        launchAllApps(appMetaData);

        verify(mMockPackageManager).setApplicationEnabledSetting(
                eq(TEST_DISABLED_APP_1), eq(COMPONENT_ENABLED_STATE_ENABLED), eq(0));

        verify(mMockPackageManager).setApplicationEnabledSetting(
                eq(TEST_DISABLED_APP_2), eq(COMPONENT_ENABLED_STATE_ENABLED), eq(0));

        verify(mMockContext, times(5)).startActivity(any(), any());

        verify(mMockPackageManager, never()).setApplicationEnabledSetting(
                eq(TEST_ENABLED_APP), anyInt(), eq(0));
    }



    private void forceStopInit(ActivityManager activityManager, CarMediaManager carMediaManager,
            ComponentName currentMediaComponentName, ComponentName previousMediaComponentName,
            Map<Integer, Boolean> currentModes, boolean isMedia) {
        when(mMockContext.getSystemService(
                ArgumentMatchers.<Class<ActivityManager>>any())).thenReturn(activityManager);
        when(mMockContext.getResources()).thenReturn(mock(Resources.class));
        if (isMedia) {
            currentModes.forEach((mode, current) -> {
                if (current) {
                    when(carMediaManager.getMediaSource(mode)).thenReturn(
                            currentMediaComponentName);
                } else {
                    when(carMediaManager.getMediaSource(mode)).thenReturn(
                            previousMediaComponentName);
                }
            });
            List<ComponentName> lastMediaSources = new ArrayList<>();
            lastMediaSources.add(currentMediaComponentName);
            if (previousMediaComponentName != null) {
                lastMediaSources.add(previousMediaComponentName);
            }
            when(carMediaManager.getLastMediaSources(anyInt())).thenReturn(lastMediaSources);
        } else {
            when(carMediaManager.getMediaSource(anyInt())).thenReturn(previousMediaComponentName);
        }
    }

    @Test
    public void forceStopNonMediaApp_shouldStopApp() {
        String packageName = "com.example.app";
        CharSequence displayName = "App";
        ActivityManager activityManager = mock(ActivityManager.class);
        CarMediaManager carMediaManager = mock(CarMediaManager.class);
        forceStopInit(activityManager, carMediaManager,
                /* currentMediaComponentName= */null, /* previousMediaComponentName= */null,
                /* currentModes= */Map.of(), /* isMedia= */false);
        Map<ComponentName, ResolveInfo> mediaServices = new HashMap<>();

        AppLauncherUtils.forceStop(packageName, mMockContext, displayName, carMediaManager,
                mediaServices, mMockShortcutsListener);

        verify(activityManager).forceStopPackage(packageName);
        verify(mMockShortcutsListener).onStopAppSuccess(nullable(String.class));
        verify(carMediaManager, never()).setMediaSource(nullable(ComponentName.class), anyInt());
    }

    @Test
    public void forceStopCurrentPlaybackOnlyMediaApp_shouldSetPlaybackOnlyToPreviousAndStopApp() {
        String packageName = "com.example.app";
        CharSequence displayName = "App";
        ActivityManager activityManager = mock(ActivityManager.class);
        CarMediaManager carMediaManager = mock(CarMediaManager.class);
        ComponentName currentMediaComponentName = new ComponentName(packageName,
                "com.example.service");
        ComponentName previousMediaComponentName = new ComponentName("test", "test");
        Map<Integer, Boolean> currentModes = new HashMap<>();
        currentModes.put(CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK, true);
        currentModes.put(CarMediaManager.MEDIA_SOURCE_MODE_BROWSE, false);
        forceStopInit(activityManager, carMediaManager, currentMediaComponentName,
                previousMediaComponentName, /* currentModes= */currentModes, /* isMedia= */true);
        Map<ComponentName, ResolveInfo> mediaServices = new HashMap<>();

        AppLauncherUtils.forceStop(packageName, mMockContext, displayName, carMediaManager,
                mediaServices, mMockShortcutsListener);

        verify(activityManager).forceStopPackage(packageName);
        verify(mMockShortcutsListener).onStopAppSuccess(nullable(String.class));
        verify(carMediaManager).setMediaSource(previousMediaComponentName,
                CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK);
        verify(carMediaManager, never()).setMediaSource(previousMediaComponentName,
                CarMediaManager.MEDIA_SOURCE_MODE_BROWSE);

    }

    @Test
    public void forceStopCurrentMediaApp_noHistory_shouldSetToOtherMediaServiceAndStopApp() {
        String packageName = "com.example.app";
        CharSequence displayName = "App";
        ActivityManager activityManager = mock(ActivityManager.class);
        CarMediaManager carMediaManager = mock(CarMediaManager.class);
        ComponentName currentMediaComponentName = new ComponentName(packageName,
                "com.example.service");
        ComponentName otherMediaComponentName = new ComponentName("other.package", "other.test");
        Map<Integer, Boolean> currentModes = new HashMap<>();
        currentModes.put(CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK, true);
        currentModes.put(CarMediaManager.MEDIA_SOURCE_MODE_BROWSE, true);
        forceStopInit(activityManager, carMediaManager, currentMediaComponentName,
                /* previousMediaComponentName= */null,
                /* currentModes= */currentModes, /* isMedia= */true);
        Map<ComponentName, ResolveInfo> mediaServices = new HashMap<>();
        mediaServices.put(otherMediaComponentName, mock(ResolveInfo.class));

        AppLauncherUtils.forceStop(packageName, mMockContext, displayName, carMediaManager,
                mediaServices, mMockShortcutsListener);

        verify(activityManager).forceStopPackage(packageName);
        verify(mMockShortcutsListener).onStopAppSuccess(nullable(String.class));
        verify(carMediaManager, times(2))
                .setMediaSource(eq(otherMediaComponentName), anyInt());
    }

    @Test
    public void forceStopNonCurrentMediaApp_shouldOnlyStopApp() {
        String packageName = "com.example.app";
        CharSequence displayName = "App";
        ActivityManager activityManager = mock(ActivityManager.class);
        CarMediaManager carMediaManager = mock(CarMediaManager.class);
        ComponentName currentMediaComponentName = new ComponentName(packageName,
                "com.example.service");
        ComponentName previousMediaComponentName = new ComponentName("test", "test");
        forceStopInit(activityManager, carMediaManager, currentMediaComponentName,
                previousMediaComponentName, /* currentModes= */Collections.emptyMap(),
                /* isMedia= */true);
        Map<ComponentName, ResolveInfo> mediaServices = new HashMap<>();

        AppLauncherUtils.forceStop(packageName, mMockContext, displayName, carMediaManager,
                mediaServices, mMockShortcutsListener);

        verify(activityManager).forceStopPackage(packageName);
        verify(mMockShortcutsListener).onStopAppSuccess(nullable(String.class));
        verify(carMediaManager, never()).setMediaSource(any(ComponentName.class), anyInt());
    }

    private void mockPackageManagerQueries() {
        // setup a media template app that uses media service
        ApplicationInfo mbsAppInfo = new ApplicationInfo();
        mbsAppInfo.category = CATEGORY_AUDIO;
        ResolveInfo mbs = constructServiceResolveInfo(TEST_MEDIA_TEMPLATE_MBS);
        try {
            when(mMockPackageManager.getApplicationInfo(mbs.getComponentInfo().packageName, 0))
                    .thenReturn(mbsAppInfo);
            when(mMockPackageManager.getServiceInfo(mbs.getComponentInfo().getComponentName(),
                    PackageManager.GET_META_DATA))
                    .thenReturn(new ServiceInfo());
            when(mMockPackageManager.getLaunchIntentForPackage(mbs.getComponentInfo().packageName))
                    .thenReturn(null);
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }

        // setup a NDO Video app that has MBS but also its own activity, MBS won't be surfaced
        ApplicationInfo videoAppInfo = new ApplicationInfo();
        videoAppInfo.category = CATEGORY_VIDEO;
        ResolveInfo videoApp = constructServiceResolveInfo(TEST_VIDEO_MBS);
        try {
            when(mMockPackageManager.getApplicationInfo(videoApp.getComponentInfo().packageName,
                    0))
                    .thenReturn(videoAppInfo);
            when(mMockPackageManager.getServiceInfo(videoApp.getComponentInfo().getComponentName(),
                    PackageManager.GET_META_DATA))
                    .thenReturn(new ServiceInfo());
            when(mMockPackageManager.getLaunchIntentForPackage(
                    videoApp.getComponentInfo().packageName))
                    .thenReturn(new Intent());
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }

        // setup a NDO app that has MBS opted in to launch in car
        ApplicationInfo launchableMBSInfo = new ApplicationInfo();
        launchableMBSInfo.category = CATEGORY_VIDEO;
        ResolveInfo launchableMBSApp = constructServiceResolveInfo(TEST_NDO_MBS_LAUNCHABLE);
        try {
            when(mMockPackageManager.getApplicationInfo(
                    launchableMBSApp.getComponentInfo().packageName,
                    0))
                    .thenReturn(launchableMBSInfo);
            ServiceInfo value = new ServiceInfo();
            value.metaData = new Bundle();

            value.metaData.putBoolean("androidx.car.app.launchable", true);

            when(mMockPackageManager.getServiceInfo(
                    launchableMBSApp.getComponentInfo().getComponentName(),
                    PackageManager.GET_META_DATA))
                    .thenReturn(value);
            when(mMockPackageManager.getLaunchIntentForPackage(
                    launchableMBSApp.getComponentInfo().packageName))
                    .thenReturn(new Intent());
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }

        // setup a NDO app that has MBS opted out of launch in car
        ApplicationInfo notlaunchableMBSInfo = new ApplicationInfo();
        notlaunchableMBSInfo.category = CATEGORY_VIDEO;
        ResolveInfo notlaunchableMBSApp = constructServiceResolveInfo(TEST_NDO_MBS_NOT_LAUNCHABLE);
        try {
            when(mMockPackageManager.getApplicationInfo(
                    notlaunchableMBSApp.getComponentInfo().packageName, 0))
                    .thenReturn(notlaunchableMBSInfo);
            ServiceInfo value = new ServiceInfo();
            value.metaData = new Bundle();

            value.metaData.putBoolean("androidx.car.app.launchable", false);

            when(mMockPackageManager.getServiceInfo(
                    notlaunchableMBSApp.getComponentInfo().getComponentName(),
                    PackageManager.GET_META_DATA))
                    .thenReturn(value);
            when(mMockPackageManager.getLaunchIntentForPackage(
                    notlaunchableMBSApp.getComponentInfo().packageName))
                    .thenReturn(new Intent());
        } catch (PackageManager.NameNotFoundException e) {
            throw new RuntimeException(e);
        }

        when(mMockPackageManager.queryIntentServices(any(), anyInt())).thenAnswer(args -> {
            Intent intent = args.getArgument(0);
            if (intent.getAction().equals(MediaBrowserService.SERVICE_INTERFACE)) {
                return Arrays.asList(mbs, videoApp, notlaunchableMBSApp, launchableMBSApp,
                        constructServiceResolveInfo(CUSTOM_MEDIA_PACKAGE));
            }
            return new ArrayList<>();
        });

        // setup activities
        when(mMockPackageManager.queryIntentActivities(any(), any())).thenAnswer(args -> {
            Intent intent = args.getArgument(0);
            PackageManager.ResolveInfoFlags flags = args.getArgument(1);
            List<ResolveInfo> resolveInfoList = new ArrayList<>();
            if (intent.getAction().equals(Intent.ACTION_MAIN)) {
                if ((flags.getValue() & MATCH_DISABLED_UNTIL_USED_COMPONENTS) != 0) {
                    resolveInfoList.add(constructActivityResolveInfo(TEST_DISABLED_APP_1));
                    resolveInfoList.add(constructActivityResolveInfo(TEST_DISABLED_APP_2));
                }
                // Keep custom media component in both MBS and Activity with Launch Intent
                resolveInfoList.add(constructActivityResolveInfo(CUSTOM_MEDIA_PACKAGE));
                // Add apps which will have their own Launcher Activity
                resolveInfoList.add(constructActivityResolveInfo(TEST_VIDEO_MBS));
                resolveInfoList.add(constructActivityResolveInfo(TEST_NDO_MBS_LAUNCHABLE));
                resolveInfoList.add(constructActivityResolveInfo(TEST_NDO_MBS_NOT_LAUNCHABLE));
            }

            return resolveInfoList;
        });
    }

    private void mockPmGetApplicationEnabledSetting(int enabledState, String... packages) {
        for (String pkg : packages) {
            when(mMockPackageManager.getApplicationEnabledSetting(pkg)).thenReturn(enabledState);
        }
    }

    private void mockSettingsStringCalls() {
        when(mMockContext.createContextAsUser(any(UserHandle.class), anyInt()))
                .thenAnswer(args -> {
                    Context context = mock(Context.class);
                    ContentResolver contentResolver = mock(ContentResolver.class);
                    when(context.getContentResolver()).thenReturn(contentResolver);
                    return context;
                });

        doReturn(TEST_DISABLED_APP_1 + PACKAGES_DISABLED_ON_RESOURCE_OVERUSE_SEPARATOR
                + TEST_DISABLED_APP_2)
                .when(() -> Settings.Secure.getString(any(ContentResolver.class),
                        eq(KEY_PACKAGES_DISABLED_ON_RESOURCE_OVERUSE)));
    }

    private void launchAllApps(List<AppMetaData> appMetaData) {
        for (AppMetaData meta : appMetaData) {
            Consumer<Context> launchCallback = meta.getLaunchCallback();
            launchCallback.accept(mMockContext);
        }
    }

    private static ResolveInfo constructActivityResolveInfo(String packageName) {
        ResolveInfo info = new ResolveInfo();
        info.activityInfo = new ActivityInfo();
        info.activityInfo.packageName = packageName;
        info.activityInfo.name = packageName + ".activity";
        info.activityInfo.applicationInfo = new ApplicationInfo();
        return info;
    }

    private static ResolveInfo constructServiceResolveInfo(String packageName) {
        ResolveInfo info = new ResolveInfo();
        info.serviceInfo = new ServiceInfo();
        info.serviceInfo.packageName = packageName;
        info.serviceInfo.name = packageName + ".service";
        info.serviceInfo.applicationInfo = new ApplicationInfo();
        return info;
    }
}
