/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityOptions;
import android.app.admin.DevicePolicyManager;
import android.car.Car;
import android.car.CarNotConnectedException;
import android.car.content.pm.CarPackageManager;
import android.car.media.CarMediaManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.LauncherActivityInfo;
import android.content.pm.LauncherApps;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.os.Process;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.service.media.MediaBrowserService;
import android.text.TextUtils;
import android.util.ArraySet;
import android.util.Log;
import android.util.Pair;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.car.ui.shortcutspopup.CarUiShortcutsPopup;

import com.google.common.collect.Sets;

import java.lang.annotation.Retention;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.function.Consumer;

/**
 * Util class that contains helper method used by app launcher classes.
 */
public class AppLauncherUtils {
    private static final String TAG = "AppLauncherUtils";
    private static final String ANDROIDX_CAR_APP_LAUNCHABLE = "androidx.car.app.launchable";

    @Retention(SOURCE)
    @IntDef({APP_TYPE_LAUNCHABLES, APP_TYPE_MEDIA_SERVICES})
    @interface AppTypes {}

    static final int APP_TYPE_LAUNCHABLES = 1;
    static final int APP_TYPE_MEDIA_SERVICES = 2;

    static final String PACKAGES_DISABLED_ON_RESOURCE_OVERUSE_SEPARATOR = ";";

    // Max no. of uses tags in automotiveApp XML. This is an arbitrary limit to be defensive
    // to bad input.
    private static final int MAX_APP_TYPES = 64;
    private static final String PACKAGE_URI_PREFIX = "package:";

    private AppLauncherUtils() {
    }

    /**
     * Comparator for {@link AppMetaData} that sorts the list
     * by the "displayName" property in ascending order.
     */
    static final Comparator<AppMetaData> ALPHABETICAL_COMPARATOR = Comparator
            .comparing(AppMetaData::getDisplayName, String::compareToIgnoreCase);

    /**
     * Helper method that launches the app given the app's AppMetaData.
     *
     * @param app the requesting app's AppMetaData
     */
    static void launchApp(Context context, Intent intent) {
        ActivityOptions options = ActivityOptions.makeBasic();
        options.setLaunchDisplayId(context.getDisplayId());
        context.startActivity(intent, options.toBundle());
    }

    /** Bundles application and services info. */
    static class LauncherAppsInfo {
        /*
         * Map of all car launcher components' (including launcher activities and media services)
         * metadata keyed by ComponentName.
         */
        private final Map<ComponentName, AppMetaData> mLaunchables;

        /** Map of all the media services keyed by ComponentName. */
        private final Map<ComponentName, ResolveInfo> mMediaServices;

        LauncherAppsInfo(@NonNull Map<ComponentName, AppMetaData> launchablesMap,
                @NonNull Map<ComponentName, ResolveInfo> mediaServices) {
            mLaunchables = launchablesMap;
            mMediaServices = mediaServices;
        }

        /** Returns true if all maps are empty. */
        boolean isEmpty() {
            return mLaunchables.isEmpty() && mMediaServices.isEmpty();
        }

        /**
         * Returns whether the given componentName is a media service.
         */
        boolean isMediaService(ComponentName componentName) {
            return mMediaServices.containsKey(componentName);
        }

        /** Returns the {@link AppMetaData} for the given componentName. */
        @Nullable
        AppMetaData getAppMetaData(ComponentName componentName) {
            return mLaunchables.get(componentName);
        }

        /** Returns a new list of all launchable components' {@link AppMetaData}. */
        @NonNull
        List<AppMetaData> getLaunchableComponentsList() {
            return new ArrayList<>(mLaunchables.values());
        }

        /** Returns list of Media Services for the launcher **/
        @NonNull
        Map<ComponentName, ResolveInfo> getMediaServices() {
            return mMediaServices;
        }
    }

    private final static LauncherAppsInfo EMPTY_APPS_INFO = new LauncherAppsInfo(
            Collections.emptyMap(), Collections.emptyMap());

    /*
     * Gets the media source in a given package. If there are multiple sources in the package,
     * returns the first one.
     */
    static ComponentName getMediaSource(@NonNull PackageManager packageManager,
            @NonNull String packageName) {
        Intent mediaIntent = new Intent();
        mediaIntent.setPackage(packageName);
        mediaIntent.setAction(MediaBrowserService.SERVICE_INTERFACE);

        List<ResolveInfo> mediaServices = packageManager.queryIntentServices(mediaIntent,
                PackageManager.GET_RESOLVED_FILTER);

        if (mediaServices == null || mediaServices.isEmpty()) {
            return null;
        }
        String defaultService = mediaServices.get(0).serviceInfo.name;
        if (!TextUtils.isEmpty(defaultService)) {
            return new ComponentName(packageName, defaultService);
        }
        return null;
    }

    /**
     * Gets all the components that we want to see in the launcher in unsorted order, including
     * launcher activities and media services.
     *
     * @param appsToHide            A (possibly empty) list of apps (package names) to hide
     * @param appTypes              Types of apps to show (e.g.: all, or media sources only)
     * @param openMediaCenter       Whether launcher should navigate to media center when the
     *                              user selects a media source.
     * @param launcherApps          The {@link LauncherApps} system service
     * @param carPackageManager     The {@link CarPackageManager} system service
     * @param packageManager        The {@link PackageManager} system service
     *                              of such apps are always excluded.
     * @param carMediaManager       The {@link CarMediaManager} system service
     * @return a new {@link LauncherAppsInfo}
     */
    @NonNull
    static LauncherAppsInfo getLauncherApps(
            Context context,
            @NonNull Set<String> appsToHide,
            @AppTypes int appTypes,
            boolean openMediaCenter,
            LauncherApps launcherApps,
            CarPackageManager carPackageManager,
            PackageManager packageManager,
            CarMediaManager carMediaManager,
            ShortcutsListener shortcutsListener,
            String mirroringAppPkgName,
            Intent mirroringAppRedirect) {

        if (launcherApps == null || carPackageManager == null || packageManager == null
                || carMediaManager == null) {
            return EMPTY_APPS_INFO;
        }

        // Using new list since we require a mutable list to do removeIf.
        List<ResolveInfo> mediaServices = new ArrayList<>();
        mediaServices.addAll(
                packageManager.queryIntentServices(
                        new Intent(MediaBrowserService.SERVICE_INTERFACE),
                        PackageManager.GET_RESOLVED_FILTER));

        List<LauncherActivityInfo> availableActivities =
                launcherApps.getActivityList(null, Process.myUserHandle());

        int launchablesSize = mediaServices.size() + availableActivities.size();
        Map<ComponentName, AppMetaData> launchablesMap = new HashMap<>(launchablesSize);
        Map<ComponentName, ResolveInfo> mediaServicesMap = new HashMap<>(mediaServices.size());
        Set<String> mEnabledPackages = new ArraySet<>(launchablesSize);

        Set<String> customMediaComponents = Sets.newHashSet(
                context.getResources().getStringArray(
                        com.android.car.media.common.R.array.custom_media_packages));

        // Process media services
        if ((appTypes & APP_TYPE_MEDIA_SERVICES) != 0) {
            for (ResolveInfo info : mediaServices) {
                String packageName = info.serviceInfo.packageName;
                String className = info.serviceInfo.name;
                ComponentName componentName = new ComponentName(packageName, className);
                mediaServicesMap.put(componentName, info);
                mEnabledPackages.add(packageName);
                if (shouldAddToLaunchables(packageManager, componentName, appsToHide,
                        customMediaComponents, appTypes, APP_TYPE_MEDIA_SERVICES)) {
                    final boolean isDistractionOptimized = true;

                    Intent intent = new Intent(Car.CAR_INTENT_ACTION_MEDIA_TEMPLATE);
                    intent.putExtra(Car.CAR_EXTRA_MEDIA_COMPONENT, componentName.flattenToString());

                    CharSequence displayName = info.serviceInfo.loadLabel(packageManager);
                    AppMetaData appMetaData = new AppMetaData(
                            displayName,
                            componentName,
                            info.serviceInfo.loadIcon(packageManager),
                            isDistractionOptimized,
                            /* isMirroring = */ false,
                            contextArg -> {
                                if (openMediaCenter) {
                                    AppLauncherUtils.launchApp(contextArg, intent);
                                } else {
                                    selectMediaSourceAndFinish(contextArg, componentName,
                                            carMediaManager);
                                }
                            },
                            buildShortcuts(packageName, displayName, shortcutsListener));
                    launchablesMap.put(componentName, appMetaData);
                }
            }
        }

        // Process activities
        if ((appTypes & APP_TYPE_LAUNCHABLES) != 0) {
            for (LauncherActivityInfo info : availableActivities) {
                ComponentName componentName = info.getComponentName();
                String packageName = componentName.getPackageName();
                mEnabledPackages.add(packageName);
                if (shouldAddToLaunchables(packageManager, componentName, appsToHide,
                        customMediaComponents, appTypes, APP_TYPE_LAUNCHABLES)) {
                    boolean isDistractionOptimized =
                            isActivityDistractionOptimized(carPackageManager, packageName,
                                    info.getName());

                    Intent intent = new Intent(Intent.ACTION_MAIN)
                            .setComponent(componentName)
                            .addCategory(Intent.CATEGORY_LAUNCHER)
                            .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                    CharSequence displayName = info.getLabel();
                    boolean isMirroring = packageName.equals(mirroringAppPkgName);
                    AppMetaData appMetaData = new AppMetaData(
                            displayName,
                            componentName,
                            info.getBadgedIcon(0),
                            isDistractionOptimized,
                            isMirroring,
                            contextArg -> {
                                if (packageName.equals(mirroringAppPkgName)) {
                                    Log.d(TAG, "non-media service package name "
                                            + "equals mirroring pkg name");
                                }
                                AppLauncherUtils.launchApp(contextArg,
                                        isMirroring ? mirroringAppRedirect : intent);
                            },
                            buildShortcuts(packageName, displayName, shortcutsListener));
                    launchablesMap.put(componentName, appMetaData);
                }
            }

            List<ResolveInfo> disabledActivities = getDisabledActivities(context, packageManager,
                    mEnabledPackages);
            for (ResolveInfo info : disabledActivities) {
                String packageName = info.activityInfo.packageName;
                String className = info.activityInfo.name;
                ComponentName componentName = new ComponentName(packageName, className);
                if (!shouldAddToLaunchables(packageManager, componentName, appsToHide,
                        customMediaComponents, appTypes, APP_TYPE_LAUNCHABLES)) {
                    continue;
                }
                boolean isDistractionOptimized =
                        isActivityDistractionOptimized(carPackageManager, packageName, className);

                Intent intent = new Intent(Intent.ACTION_MAIN)
                        .setComponent(componentName)
                        .addCategory(Intent.CATEGORY_LAUNCHER)
                        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

                CharSequence displayName = info.activityInfo.loadLabel(packageManager);
                AppMetaData appMetaData = new AppMetaData(
                        displayName,
                        componentName,
                        info.activityInfo.loadIcon(packageManager),
                        isDistractionOptimized,
                        /* isMirroring = */ false,
                        contextArg -> {
                            packageManager.setApplicationEnabledSetting(packageName,
                                    PackageManager.COMPONENT_ENABLED_STATE_ENABLED, 0);
                            /* Fetch the current enabled setting to make sure the setting is synced
                             * before launching the activity. Otherwise, the activity may not
                             * launch.
                             */
                            if (packageManager.getApplicationEnabledSetting(packageName)
                                    != PackageManager.COMPONENT_ENABLED_STATE_ENABLED) {
                                throw new IllegalStateException(
                                        "Failed to enable the disabled package [" + packageName
                                                + "]");
                            }
                            Log.i(TAG, "Successfully enabled package [" + packageName + "]");
                            AppLauncherUtils.launchApp(contextArg, intent);
                        },
                        buildShortcuts(packageName, displayName, shortcutsListener));
                launchablesMap.put(componentName, appMetaData);
            }
        }

        return new LauncherAppsInfo(launchablesMap, mediaServicesMap);
    }

    /**
     * Determine if the given media browse service is supported through media templates
     */
    public static boolean isMediaTemplate(PackageManager pm, ComponentName mbsComponentName) {
        Bundle metaData = null;
        try {
            metaData = pm.getServiceInfo(
                    mbsComponentName,
                    PackageManager.GET_META_DATA)
                    .metaData;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Service for Component " + mbsComponentName + " was not found");
            return false;
        }

        if (metaData != null && metaData.containsKey(ANDROIDX_CAR_APP_LAUNCHABLE)) {
            boolean launchable = metaData.getBoolean(ANDROIDX_CAR_APP_LAUNCHABLE);
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "MBS for " + mbsComponentName
                        + " is opted " + (launchable ? "in" : "out"));
            }
            return launchable;
        }

        // No explicit declaration. For backward compatibility, keep MBS only for media apps
        String packageName = mbsComponentName.getPackageName();
        try {
            if (isLegacyMediaApp(pm, packageName)) {
                Log.d(TAG, "Including " + mbsComponentName + "  for media app " + packageName);
                return true;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Package " + packageName + " was not found");
        }
        Log.d(TAG, "Skipping MBS for " + mbsComponentName
                + " belonging to non media app " + packageName);
        return false;
    }

    /** Determine if it's a legacy media app that doesn't have a launcher activity*/
    private static boolean isLegacyMediaApp(
            PackageManager pm, String packageName) throws PackageManager.NameNotFoundException {
        // a media app doesn't have a launcher activity
        return pm.getLaunchIntentForPackage(packageName) == null;
    }

    private static Consumer<Pair<Context, View>> buildShortcuts(String packageName,
            CharSequence displayName, ShortcutsListener shortcutsListener) {
        return pair -> {
            CarUiShortcutsPopup carUiShortcutsPopup = new CarUiShortcutsPopup.Builder()
                    .addShortcut(
                            buildForceStopShortcut(packageName, displayName, pair.first,
                                    shortcutsListener)
                    )
                    .addShortcut(buildAppInfoShortcut(packageName, pair.first))
                    .build(pair.first,
                            pair.second
                    );

            carUiShortcutsPopup.show();
            shortcutsListener.onShortcutsShow(carUiShortcutsPopup);
        };
    }

    private static CarUiShortcutsPopup.ShortcutItem buildForceStopShortcut(String packageName,
            CharSequence displayName,
            Context context,
            ShortcutsListener shortcutsListener) {
        return new CarUiShortcutsPopup.ShortcutItem() {
            @Override
            public CarUiShortcutsPopup.ItemData data() {
                return new CarUiShortcutsPopup.ItemData(
                        R.drawable.ic_force_stop_caution_icon,
                        context.getResources().getString(
                                R.string.app_launcher_stop_app_action));
            }

            @Override
            public boolean onClick() {
                shortcutsListener.onShortcutsItemClick(packageName, displayName,
                        /* allowStopApp= */ true);
                return true;
            }

            @Override
            public boolean isEnabled() {
                return shouldAllowStopApp(packageName, context);
            }
        };
    }

    private static CarUiShortcutsPopup.ShortcutItem buildAppInfoShortcut(String packageName,
            Context context) {
        return new CarUiShortcutsPopup.ShortcutItem() {
            @Override
            public CarUiShortcutsPopup.ItemData data() {
                return new CarUiShortcutsPopup.ItemData(
                        R.drawable.ic_app_info,
                        context.getResources().getString(
                                R.string.app_launcher_app_info_action));
            }

            @Override
            public boolean onClick() {
                Uri packageURI = Uri.parse(PACKAGE_URI_PREFIX + packageName);
                Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                        packageURI);
                context.startActivity(intent);
                return true;
            }

            @Override
            public boolean isEnabled() {
                return true;
            }
        };
    }

    /**
     * Force stops an app
     * <p>Note: Uses hidden apis<p/>
     */
    public static void forceStop(String packageName, Context context, CharSequence displayName,
            CarMediaManager carMediaManager, Map<ComponentName, ResolveInfo> mediaServices,
            ShortcutsListener listener) {
        ActivityManager activityManager = context.getSystemService(ActivityManager.class);
        if (activityManager != null) {
            maybeReplaceMediaSource(carMediaManager, packageName, mediaServices,
                    CarMediaManager.MEDIA_SOURCE_MODE_BROWSE);
            maybeReplaceMediaSource(carMediaManager, packageName, mediaServices,
                    CarMediaManager.MEDIA_SOURCE_MODE_PLAYBACK);
            activityManager.forceStopPackage(packageName);
            String message = context.getResources()
                    .getString(R.string.app_launcher_stop_app_success_toast_text, displayName);
            listener.onStopAppSuccess(message);
        }
    }

    private static boolean isCurrentMediaSource(CarMediaManager carMediaManager,
            String packageName, @CarMediaManager.MediaSourceMode int mode) {
        ComponentName componentName = carMediaManager.getMediaSource(mode);
        if (componentName == null) {
            //There is no current media source.
            return false;
        }
        return Objects.equals(componentName.getPackageName(), packageName);
    }

    /***
     * Updates the MediaSource to second most recent if {@code  packageName} is current media source
     * Sets to MediaSource to null if no previous MediaSource exists.
     */
    private static void maybeReplaceMediaSource(CarMediaManager carMediaManager, String packageName,
            Map<ComponentName, ResolveInfo> allMediaServices,
            @CarMediaManager.MediaSourceMode int mode) {
        if (!isCurrentMediaSource(carMediaManager, packageName, mode)) {
            return;
        }
        //find the most recent source from history not equal to force-stopping package.
        List<ComponentName> mediaSources = carMediaManager.getLastMediaSources(mode);
        ComponentName componentName = mediaSources.stream().filter(c-> (!c.getPackageName()
                .equals(packageName))).findFirst().orElse(null);
        if (componentName == null) {
            //no recent package found, find from all available media services.
            componentName = allMediaServices.keySet().stream().filter(
                    c -> (!c.getPackageName().equals(packageName))).findFirst().orElse(null);
            if (componentName == null) {
                Log.e(TAG, "Stop-app, no alternative media service found");
            }
        }
        carMediaManager.setMediaSource(componentName, mode);
    }

    /**
     * <p>Note: Uses hidden apis<p/>
     * @return true if the user has restrictions to force stop an app with {@code appInfo}
     */
    private static boolean hasUserRestriction(ApplicationInfo appInfo, Context context) {
        String restriction = UserManager.DISALLOW_APPS_CONTROL;
        UserManager userManager = context.getSystemService(UserManager.class);
        if (userManager == null) {
            Log.e(TAG, " Disabled because , UserManager is null");
            return true;
        }
        if (!userManager.hasUserRestriction(restriction)) {
            return false;
        }
        UserHandle user = UserHandle.getUserHandleForUid(appInfo.uid);
        if (userManager.hasBaseUserRestriction(restriction, user)) {
            Log.d(TAG, " Disabled because " + user + " has " + restriction
                    + " restriction");
            return true;
        }
        // Not disabled for this User
        return false;
    }

    /**
     * <p>Note: uses hidden apis</p>
     *
     * @param packageName name of the package to stop the app
     * @param context     app context
     * @return true if an app should show the Stop app action
     */
    private static boolean shouldAllowStopApp(String packageName, Context context) {
        DevicePolicyManager dm = context.getSystemService(DevicePolicyManager.class);
        if (dm == null || dm.packageHasActiveAdmins(packageName)) {
            return false;
        }
        try {
            ApplicationInfo appInfo = context.getPackageManager().getApplicationInfo(packageName,
                    PackageManager.ApplicationInfoFlags.of(PackageManager.GET_META_DATA));
            // Show only if the User has no restrictions to force stop this app
            if (hasUserRestriction(appInfo, context)) {
                return false;
            }
            // Show only if the app is running
            if ((appInfo.flags & ApplicationInfo.FLAG_STOPPED) == 0) {
                return true;
            }
        } catch (PackageManager.NameNotFoundException e) {
            Log.d(TAG, "shouldAllowStopApp() Package " + packageName + " was not found");
        }
        return false;
    }

    private static List<ResolveInfo> getDisabledActivities(Context context,
            PackageManager packageManager, Set<String> enabledPackages) {
        ContentResolver contentResolverForUser = context.createContextAsUser(
                        UserHandle.getUserHandleForUid(Process.myUid()), /* flags= */ 0)
                .getContentResolver();
        String settingsValue = Settings.Secure.getString(contentResolverForUser,
                KEY_PACKAGES_DISABLED_ON_RESOURCE_OVERUSE);
        Set<String> disabledPackages = TextUtils.isEmpty(settingsValue) ? new ArraySet<>()
                : new ArraySet<>(Arrays.asList(settingsValue.split(
                        PACKAGES_DISABLED_ON_RESOURCE_OVERUSE_SEPARATOR)));
        if (disabledPackages.isEmpty()) {
            return Collections.emptyList();
        }

        List<ResolveInfo> allActivities = packageManager.queryIntentActivities(
                new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER),
                PackageManager.ResolveInfoFlags.of(PackageManager.GET_RESOLVED_FILTER
                        | PackageManager.MATCH_DISABLED_UNTIL_USED_COMPONENTS));

        List<ResolveInfo> disabledActivities = new ArrayList<>();
        for (int i = 0; i < allActivities.size(); ++i) {
            ResolveInfo info = allActivities.get(i);
            if (!enabledPackages.contains(info.activityInfo.packageName)
                    && disabledPackages.contains(info.activityInfo.packageName)) {
                disabledActivities.add(info);
            }
        }
        return disabledActivities;
    }

    private static boolean shouldAddToLaunchables(PackageManager packageManager,
            @NonNull ComponentName componentName,
            @NonNull Set<String> appsToHide,
            @NonNull Set<String> customMediaComponents,
            @AppTypes int appTypesToShow,
            @AppTypes int componentAppType) {
        if (appsToHide.contains(componentName.getPackageName())) {
            return false;
        }
        switch (componentAppType) {
            // Process media services
            case APP_TYPE_MEDIA_SERVICES:
                // For a media service in customMediaComponents, if its application's launcher
                // activity will be shown in the Launcher, don't show the service's icon in the
                // Launcher.
                if (customMediaComponents.contains(componentName.flattenToString())) {
                    if ((appTypesToShow & APP_TYPE_LAUNCHABLES) != 0) {
                        if (Log.isLoggable(TAG, Log.DEBUG)) {
                            Log.d(TAG, "MBS for custom media app " + componentName
                                    + " is skipped in app launcher");
                        }
                        return false;
                    }
                    // Media switcher use case should still show
                    if (Log.isLoggable(TAG, Log.DEBUG)) {
                        Log.d(TAG, "MBS for custom media app " + componentName
                                + " is included in media switcher");
                    }
                    return true;
                }
                // Only Keep MBS that is a media template
                return isMediaTemplate(packageManager, componentName);
            // Process activities
            case APP_TYPE_LAUNCHABLES:
                return true;
            default:
                Log.e(TAG, "Invalid componentAppType : " + componentAppType);
                return false;
        }
    }

    private static void selectMediaSourceAndFinish(Context context, ComponentName componentName,
            CarMediaManager carMediaManager) {
        try {
            carMediaManager.setMediaSource(componentName, CarMediaManager.MEDIA_SOURCE_MODE_BROWSE);
            if (context instanceof Activity) {
                ((Activity) context).finish();
            }
        } catch (CarNotConnectedException e) {
            Log.e(TAG, "Car not connected", e);
        }
    }

    /**
     * Gets if an activity is distraction optimized.
     *
     * @param carPackageManager The {@link CarPackageManager} system service
     * @param packageName       The package name of the app
     * @param activityName      The requested activity name
     * @return true if the supplied activity is distraction optimized
     */
    static boolean isActivityDistractionOptimized(
            CarPackageManager carPackageManager, String packageName, String activityName) {
        boolean isDistractionOptimized = false;
        // try getting distraction optimization info
        try {
            if (carPackageManager != null) {
                isDistractionOptimized =
                        carPackageManager.isActivityDistractionOptimized(packageName, activityName);
            }
        } catch (CarNotConnectedException e) {
            Log.e(TAG, "Car not connected when getting DO info", e);
        }
        return isDistractionOptimized;
    }

    /**
     * Callback when a ShortcutsPopup View is shown
     */
    protected interface ShortcutsListener {

        void onShortcutsShow(CarUiShortcutsPopup carUiShortcutsPopup);

        void onShortcutsItemClick(String packageName, CharSequence displayName,
                boolean allowStopApp);

        void onStopAppSuccess(String message);
    }
}
