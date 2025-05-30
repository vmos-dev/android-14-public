/*
 * Copyright (C) 2012 The Android Open Source Project
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


package com.android.camera;

import android.Manifest;
import android.animation.Animator;
import android.app.ActionBar;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.Dialog;
import android.app.KeyguardManager.KeyguardDismissCallback;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ComponentCallbacks;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.hardware.Camera;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.StrictMode;
import android.os.storage.DiskInfo;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.os.storage.VolumeInfo;
import android.provider.MediaStore;
import android.provider.Settings;
import android.text.TextUtils;
import android.util.CameraPerformanceTracker;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.OrientationEventListener;
import android.view.View;
import android.view.View.OnSystemUiVisibilityChangeListener;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ShareActionProvider;
import android.widget.Toast;

import com.android.camera.app.AppController;
import com.android.camera.app.CameraAppUI;
import com.android.camera.app.CameraController;
import com.android.camera.app.CameraProvider;
import com.android.camera.app.CameraServices;
import com.android.camera.app.CameraServicesImpl;
import com.android.camera.app.FirstRunDialog;
import com.android.camera.app.LocationManager;
import com.android.camera.app.MemoryManager;
import com.android.camera.app.MemoryQuery;
import com.android.camera.app.ModuleManager;
import com.android.camera.app.ModuleManager.ModuleAgent;
import com.android.camera.app.OrientationManager.DeviceOrientation;
import com.android.camera.app.ModuleManagerImpl;
import com.android.camera.app.MotionManager;
import com.android.camera.app.OrientationManager;
import com.android.camera.app.OrientationManagerImpl;
import com.android.camera.data.CameraFilmstripDataAdapter;
import com.android.camera.data.FilmstripContentObserver;
import com.android.camera.data.FilmstripItem;
import com.android.camera.data.FilmstripItemData;
import com.android.camera.data.FilmstripItemType;
import com.android.camera.data.FilmstripItemUtils;
import com.android.camera.data.FixedLastProxyAdapter;
import com.android.camera.data.GlideFilmstripManager;
import com.android.camera.data.LocalFilmstripDataAdapter;
import com.android.camera.data.LocalFilmstripDataAdapter.FilmstripItemListener;
import com.android.camera.data.Location;
import com.android.camera.data.MediaDetails;
import com.android.camera.data.MetadataLoader;
import com.android.camera.data.PhotoDataFactory;
import com.android.camera.data.PhotoItem;
import com.android.camera.data.PhotoItemFactory;
import com.android.camera.data.PlaceholderItem;
import com.android.camera.data.SessionItem;
import com.android.camera.data.VideoDataFactory;
import com.android.camera.data.VideoItemFactory;
import com.android.camera.debug.DebugPropertyHelper;
import com.android.camera.debug.Log;
import com.android.camera.device.ActiveCameraDeviceTracker;
import com.android.camera.device.CameraId;
import com.android.camera.filmstrip.FilmstripContentPanel;
import com.android.camera.filmstrip.FilmstripController;
import com.android.camera.module.ModuleController;
import com.android.camera.module.ModulesInfo;
import com.android.camera.one.OneCameraException;
import com.android.camera.one.OneCameraManager;
import com.android.camera.one.OneCameraModule;
import com.android.camera.one.OneCameraOpener;
import com.android.camera.one.config.OneCameraFeatureConfig;
import com.android.camera.one.config.OneCameraFeatureConfigCreator;
import com.android.camera.session.CaptureSession;
import com.android.camera.session.CaptureSessionManager;
import com.android.camera.session.CaptureSessionManager.SessionListener;
import com.android.camera.settings.AppUpgrader;
import com.android.camera.settings.CameraSettingsActivity;
import com.android.camera.settings.Keys;
import com.android.camera.settings.PictureSizeLoader;
import com.android.camera.settings.ResolutionSetting;
import com.android.camera.settings.ResolutionUtil;
import com.android.camera.settings.SettingsManager;
import com.android.camera.stats.UsageStatistics;
import com.android.camera.stats.profiler.Profile;
import com.android.camera.stats.profiler.Profiler;
import com.android.camera.stats.profiler.Profilers;
import com.android.camera.tinyplanet.TinyPlanetFragment;
import com.android.camera.ui.AbstractTutorialOverlay;
import com.android.camera.ui.DetailsDialog;
import com.android.camera.ui.MainActivityLayout;
import com.android.camera.ui.ModeListView;
import com.android.camera.ui.ModeListView.ModeListVisibilityChangedListener;
import com.android.camera.ui.PreviewStatusListener;
import com.android.camera.util.ApiHelper;
import com.android.camera.util.Callback;
import com.android.camera.util.CameraUtil;
import com.android.camera.util.AndroidContext;
import com.android.camera.util.GalleryHelper;
import com.android.camera.util.GcamHelper;
import com.android.camera.util.GoogleHelpHelper;
import com.android.camera.util.IntentHelper;
import com.android.camera.util.PhotoSphereHelper.PanoramaViewHelper;
import com.android.camera.util.QuickActivity;
import com.android.camera.util.ReleaseHelper;
import com.android.camera.widget.FilmstripView;
import com.android.camera.widget.Preloader;
import com.android.camera2.R;
import com.android.ex.camera2.portability.CameraAgent;
import com.android.ex.camera2.portability.CameraAgentFactory;
import com.android.ex.camera2.portability.CameraExceptionHandler;
import com.android.ex.camera2.portability.CameraSettings;
import com.android.ex.camera2.portability.CameraCapabilities;
import com.bumptech.glide.Glide;
import com.bumptech.glide.GlideBuilder;
import com.bumptech.glide.MemoryCategory;
import com.bumptech.glide.load.DecodeFormat;
import com.bumptech.glide.load.engine.executor.FifoPriorityThreadPoolExecutor;
import com.android.camera.exif.ExifInterface;

import com.google.common.base.Optional;
import com.google.common.logging.eventprotos;
import com.google.common.logging.eventprotos.ForegroundEvent.ForegroundSource;
import com.google.common.logging.eventprotos.MediaInteraction;
import com.google.common.logging.eventprotos.NavigationChange;

import java.io.File;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

public class CameraActivity extends QuickActivity
        implements AppController, CameraAgent.CameraOpenCallback,
        ShareActionProvider.OnShareTargetSelectedListener,
        OrientationManager.OnOrientationChangeListener {

    private static final Log.Tag TAG = new Log.Tag("CameraActivity");

    private static final String INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE =
            "android.media.action.STILL_IMAGE_CAMERA_SECURE";
    public static final String ACTION_IMAGE_CAPTURE_SECURE =
            "android.media.action.IMAGE_CAPTURE_SECURE";

    // The intent extra for camera from secure lock screen. True if the gallery
    // should only show newly captured pictures. sSecureAlbumId does not
    // increment. This is used when switching between camera, camcorder, and
    // panorama. If the extra is not set, it is in the normal camera mode.
    public static final String SECURE_CAMERA_EXTRA = "secure_camera";

    private static final int MSG_CLEAR_SCREEN_ON_FLAG = 2;
    private static final long SCREEN_DELAY_MS = 2 * 60 * 1000; // 2 mins.
    /** Load metadata for 10 items ahead of our current. */
    private static final int FILMSTRIP_PRELOAD_AHEAD_ITEMS = 10;

    /** Should be used wherever a context is needed. */
    private Context mAppContext;

    /**
     * Camera fatal error handling:
     * 1) Present error dialog to guide users to exit the app.
     * 2) If users hit home button, onPause should just call finish() to exit the app.
     */
    private boolean mCameraFatalError = false;

    /**
     * Whether onResume should reset the view to the preview.
     */
    private boolean mResetToPreviewOnResume = true;

    /**
     * This data adapter is used by FilmStripView.
     */
    private VideoItemFactory mVideoItemFactory;
    private PhotoItemFactory mPhotoItemFactory;
    private LocalFilmstripDataAdapter mDataAdapter;

    private ActiveCameraDeviceTracker mActiveCameraDeviceTracker;
    private OneCameraOpener mOneCameraOpener;
    private OneCameraManager mOneCameraManager;
    private SettingsManager mSettingsManager;
    private ResolutionSetting mResolutionSetting;
    private ModeListView mModeListView;
    private boolean mModeListVisible = false;
    private int mCurrentModeIndex;
    private CameraModule mCurrentModule;
    private ModuleManagerImpl mModuleManager;
    private FrameLayout mAboveFilmstripControlLayout;
    private FilmstripController mFilmstripController;
    private boolean mFilmstripVisible;
    /** Whether the filmstrip fully covers the preview. */
    private boolean mFilmstripCoversPreview = false;
    private int mResultCodeForTesting;
    private Intent mResultDataForTesting;
    private OnScreenHint mStorageHint;
    private final Object mStorageSpaceLock = new Object();
    private long mStorageSpaceBytes = Storage.LOW_STORAGE_THRESHOLD_BYTES;
    private long mOtherStorageSpaceBytes = Storage.LOW_STORAGE_THRESHOLD_BYTES;
    private boolean mAutoRotateScreen;
    private boolean mSecureCamera;
    private int mLastRawOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;
    private OrientationManagerImpl mOrientationManager;
    private LocationManager mLocationManager;
    private ButtonManager mButtonManager;
    private Handler mMainHandler;
    private PanoramaViewHelper mPanoramaViewHelper;
    private ActionBar mActionBar;
    private ViewGroup mUndoDeletionBar;
    private boolean mIsUndoingDeletion = false;
    private boolean mIsActivityRunning = false;
    private FatalErrorHandler mFatalErrorHandler;
    private boolean mHasCriticalPermissions;

    private boolean mIsNeedReLoadImage = false;

    private FilmstripContentObserver mLocalImagesObserver;
    private FilmstripContentObserver mLocalVideosObserver;

    private boolean mPendingDeletion = false;

    private CameraController mCameraController;
    private boolean mPaused;
    private CameraAppUI mCameraAppUI;

    private CameraCapabilities mCameraCapabilities;

    private Intent mGalleryIntent;
    private long mOnCreateTime;

    private Menu mActionBarMenu;
    private Preloader<Integer, AsyncTask> mPreloader;
    private AsyncTask<Void, Void, Long> mUpdateStorageTask;

    /** Can be used to play custom sounds. */
    private SoundPlayer mSoundPlayer;

    /** Holds configuration for various OneCamera features. */
    private OneCameraFeatureConfig mFeatureConfig;

    private Toast mBackToast;

    private static final int LIGHTS_OUT_DELAY_MS = 4000;
    private final int BASE_SYS_UI_VISIBILITY =
            View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
    private final Runnable mLightsOutRunnable = new Runnable() {
        @Override
        public void run() {
            getWindow().getDecorView().setSystemUiVisibility(
                    BASE_SYS_UI_VISIBILITY | View.SYSTEM_UI_FLAG_LOW_PROFILE);
        }
    };
    private MemoryManager mMemoryManager;
    private MotionManager mMotionManager;
    private final Profiler mProfiler = Profilers.instance().guard();

    /** First run dialog */
    private FirstRunDialog mFirstRunDialog;

    @Override
    public CameraAppUI getCameraAppUI() {
        return mCameraAppUI;
    }

    @Override
    public ModuleManager getModuleManager() {
        return mModuleManager;
    }

    /**
     * Close activity when secure app passes lock screen or screen turns
     * off.
     */
    private final BroadcastReceiver mShutdownReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            finish();
        }
    };

    /**
     * Whether the screen is kept turned on.
     */
    private boolean mKeepScreenOn;
    private int mLastLayoutOrientation;
    private final CameraAppUI.BottomPanel.Listener mMyFilmstripBottomControlListener =
            new CameraAppUI.BottomPanel.Listener() {

                /**
                 * If the current photo is a photo sphere, this will launch the
                 * Photo Sphere panorama viewer.
                 */
                @Override
                public void onExternalViewer() {
                    if (mPanoramaViewHelper == null) {
                        return;
                    }
                    final FilmstripItem data = getCurrentLocalData();
                    if (data == null) {
                        Log.w(TAG, "Cannot open null data.");
                        return;
                    }
                    final Uri contentUri = data.getData().getUri();
                    if (contentUri == Uri.EMPTY) {
                        Log.w(TAG, "Cannot open empty URL.");
                        return;
                    }

                    if (data.getMetadata().isUsePanoramaViewer()) {
                        mPanoramaViewHelper.showPanorama(CameraActivity.this, contentUri);
                    } else if (data.getMetadata().isHasRgbzData()) {
                        mPanoramaViewHelper.showRgbz(contentUri);
                        if (mSettingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                                Keys.KEY_SHOULD_SHOW_REFOCUS_VIEWER_CLING)) {
                            mSettingsManager.set(SettingsManager.SCOPE_GLOBAL,
                                    Keys.KEY_SHOULD_SHOW_REFOCUS_VIEWER_CLING, false);
                            mCameraAppUI.clearClingForViewer(
                                    CameraAppUI.BottomPanel.VIEWER_REFOCUS);
                        }
                    }
                }

                @Override
                public void onEdit() {
                    FilmstripItem data = getCurrentLocalData();
                    if (data == null) {
                        Log.w(TAG, "Cannot edit null data.");
                        return;
                    }
                    final int currentDataId = getCurrentDataId();
                    UsageStatistics.instance().mediaInteraction(fileNameFromAdapterAtIndex(
                                currentDataId),
                            MediaInteraction.InteractionType.EDIT,
                            NavigationChange.InteractionCause.BUTTON,
                            fileAgeFromAdapterAtIndex(currentDataId));
                    launchEditor(data);
                }

                @Override
                public void onTinyPlanet() {
                    FilmstripItem data = getCurrentLocalData();
                    if (data == null) {
                        Log.w(TAG, "Cannot edit tiny planet on null data.");
                        return;
                    }
                    launchTinyPlanetEditor(data);
                }

                @Override
                public void onDelete() {
                    final int currentDataId = getCurrentDataId();
                    UsageStatistics.instance().mediaInteraction(fileNameFromAdapterAtIndex(
                                currentDataId),
                            MediaInteraction.InteractionType.DELETE,
                            NavigationChange.InteractionCause.BUTTON,
                            fileAgeFromAdapterAtIndex(currentDataId));
                    removeItemAt(currentDataId);
                }

                @Override
                public void onShare() {
                    final FilmstripItem data = getCurrentLocalData();
                    if (data == null) {
                        Log.w(TAG, "Cannot share null data.");
                        return;
                    }

                    final int currentDataId = getCurrentDataId();
                    UsageStatistics.instance().mediaInteraction(fileNameFromAdapterAtIndex(
                                currentDataId),
                            MediaInteraction.InteractionType.SHARE,
                            NavigationChange.InteractionCause.BUTTON,
                            fileAgeFromAdapterAtIndex(currentDataId));
                    // If applicable, show release information before this item
                    // is shared.
                    if (ReleaseHelper.shouldShowReleaseInfoDialogOnShare(data)) {
                        ReleaseHelper.showReleaseInfoDialog(CameraActivity.this,
                                new Callback<Void>() {
                                    @Override
                                    public void onCallback(Void result) {
                                        share(data);
                                    }
                                });
                    } else {
                        share(data);
                    }
                }

                private void share(FilmstripItem data) {
                    Intent shareIntent = getShareIntentByData(data);
                    if (shareIntent != null) {
                        try {
                            launchActivityByIntent(shareIntent);
                            mCameraAppUI.getFilmstripBottomControls().setShareEnabled(false);
                        } catch (ActivityNotFoundException ex) {
                            // Nothing.
                        }
                    }
                }

                private int getCurrentDataId() {
                    return mFilmstripController.getCurrentAdapterIndex();
                }

                private FilmstripItem getCurrentLocalData() {
                    return mDataAdapter.getItemAt(getCurrentDataId());
                }

                /**
                 * Sets up the share intent and NFC properly according to the
                 * data.
                 *
                 * @param item The data to be shared.
                 */
                private Intent getShareIntentByData(final FilmstripItem item) {
                    Intent intent = null;
                    final Uri contentUri = item.getData().getUri();
                    final String msgShareTo = getResources().getString(R.string.share_to);

                    if (item.getMetadata().isPanorama360() &&
                          item.getData().getUri() != Uri.EMPTY) {
                        intent = new Intent(Intent.ACTION_SEND);
                        intent.setType(FilmstripItemData.MIME_TYPE_PHOTOSPHERE);
                        intent.putExtra(Intent.EXTRA_STREAM, contentUri);
                    } else if (item.getAttributes().canShare()) {
                        final String mimeType = item.getData().getMimeType();
                        intent = getShareIntentFromType(mimeType);
                        if (intent != null) {
                            intent.putExtra(Intent.EXTRA_STREAM, contentUri);
                            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                        }
                        intent = Intent.createChooser(intent, msgShareTo);
                    }
                    return intent;
                }

                /**
                 * Get the share intent according to the mimeType
                 *
                 * @param mimeType The mimeType of current data.
                 * @return the video/image's ShareIntent or null if mimeType is
                 *         invalid.
                 */
                private Intent getShareIntentFromType(String mimeType) {
                    // Lazily create the intent object.
                    Intent intent = new Intent(Intent.ACTION_SEND);
                    if (mimeType.startsWith("video/")) {
                        intent.setType("video/*");
                    } else {
                        if (mimeType.startsWith("image/")) {
                            intent.setType("image/*");
                        } else {
                            Log.w(TAG, "unsupported mimeType " + mimeType);
                        }
                    }
                    return intent;
                }

                @Override
                public void onProgressErrorClicked() {
                    FilmstripItem data = getCurrentLocalData();
                    getServices().getCaptureSessionManager().removeErrorMessage(
                            data.getData().getUri());
                    updateBottomControlsByData(data);
                }
            };

    @Override
    public void onCameraOpened(CameraAgent.CameraProxy camera) {
        Log.v(TAG, "onCameraOpened activity:" + this);
        if (mPaused) {
            // We've paused, but just asynchronously opened the camera. Close it
            // because we should be releasing the camera when paused to allow
            // other apps to access it.
            Log.v(TAG, "received onCameraOpened but activity is paused, closing Camera");
            mCameraController.closeCamera(true);
            return;
        }

        if (!mModuleManager.getModuleAgent(mCurrentModeIndex).requestAppForCamera()) {
            // We shouldn't be here. Just close the camera and leave.
            mCameraController.closeCamera(false);
            Log.e(TAG,"Camera opened but the module shouldn't be " +
                    "requesting");
            return;
        }

        Camera cam = camera.getCamera();
        // If the camera device is null, the camera proxy is stale
        // should be ignored.
        if (cam != null) {
            Log.w(TAG, "null camera within proxy, maybe api2");
            if (cam.isReleased()) {
                Log.d(TAG, "camera has released!!!!");
                return;
            }
        }
		
		mCameraCapabilities = camera.getCapabilities();

        if (mCurrentModule != null) {
		    if (cam != null) {
            boolean result = false;
            result = resetExposureCompensationToDefault(camera);
            if (!result) return;
            result = resetWhiteBalanceToDefault(camera);
            if (!result) return;
            result = resetSceneToDefault(camera);
            if (!result) return;
            result = resetColorEffectToDefault(camera);
            if (!result) return;
            result = resetSaturationToDefault(camera);
            if (!result) return;
            result = resetContrastToDefault(camera);
            if (!result) return;
            result = resetSharpnessToDefault(camera);
            if (!result) return;
            result = resetBrightnessToDefault(camera);
            if (!result) return;
            result = resetHueToDefault(camera);
            if (!result) return;
			}
            try {
                mCurrentModule.onCameraAvailable(camera);
            } catch (RuntimeException ex) {
                Log.e(TAG, "Error connecting to camera", ex);
                mFatalErrorHandler.onCameraOpenFailure();
            }
        } else {
            Log.v(TAG, "mCurrentModule null, not invoking onCameraAvailable");
        }
        Log.v(TAG, "invoking onChangeCamera");
        mCameraAppUI.onChangeCamera();
        syncCameraSoundSetting(camera);
    }

    private boolean resetExposureCompensationToDefault(CameraAgent.CameraProxy camera) {
        // Reset the exposure compensation before handing the camera to module.
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setExposureCompensationIndex(0);
		if (mCameraCapabilities.supports(cameraSettings))
        	return camera.applySettings(cameraSettings);
		else
		    return true;
    }

    private boolean resetWhiteBalanceToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setWhiteBalance(CameraCapabilities.WhiteBalance.AUTO);
		if (mCameraCapabilities.supports(CameraCapabilities.WhiteBalance.AUTO))
        	return camera.applySettings(cameraSettings);
		else
		    return true;
    }

    private boolean resetSceneToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setSceneMode(CameraCapabilities.SceneMode.AUTO);
        if (mCameraCapabilities.supports(CameraCapabilities.SceneMode.AUTO))
        	return camera.applySettings(cameraSettings);
		else
		    return true;
    }

    private boolean resetColorEffectToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setColorEffect(Camera.Parameters.EFFECT_NONE);
        return camera.applySettings(cameraSettings);
    }

    private boolean resetSaturationToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setSaturation("normal");
        return camera.applySettings(cameraSettings);
    }

    private boolean resetContrastToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setContrast("normal");
        return camera.applySettings(cameraSettings);
    }

    private boolean resetSharpnessToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setSharpness("normal");
        return camera.applySettings(cameraSettings);
    }

    private boolean resetBrightnessToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setBrightness("normal");
        return camera.applySettings(cameraSettings);
    }

    private boolean resetHueToDefault(CameraAgent.CameraProxy camera) {
        CameraSettings cameraSettings = camera.getSettings();
        cameraSettings.setHue("normal");
        return camera.applySettings(cameraSettings);
    }

    @Override
    public void onCameraDisabled(int cameraId) {
        Log.w(TAG, "Camera disabled: " + cameraId);
        mFatalErrorHandler.onCameraDisabledFailure();
    }

    @Override
    public void onDeviceOpenFailure(int cameraId, String info) {
        Log.w(TAG, "Camera open failure: " + info);
        mFatalErrorHandler.onCameraOpenFailure();
    }

    @Override
    public void onDeviceOpenedAlready(int cameraId, String info) {
        Log.w(TAG, "Camera open already: " + cameraId + "," + info);
        mFatalErrorHandler.onGenericCameraAccessFailure();
    }

    @Override
    public void onReconnectionFailure(CameraAgent mgr, String info) {
        Log.w(TAG, "Camera reconnection failure:" + info);
        mFatalErrorHandler.onCameraReconnectFailure();
    }

    private static class MainHandler extends Handler {
        final WeakReference<CameraActivity> mActivity;

        public MainHandler(CameraActivity activity, Looper looper) {
            super(looper);
            mActivity = new WeakReference<CameraActivity>(activity);
        }

        @Override
        public void handleMessage(Message msg) {
            CameraActivity activity = mActivity.get();
            if (activity == null) {
                return;
            }
            switch (msg.what) {

                case MSG_CLEAR_SCREEN_ON_FLAG: {
                    if (!activity.mPaused) {
                        activity.getWindow().clearFlags(
                                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    }
                    break;
                }
            }
        }
    }

    private String fileNameFromAdapterAtIndex(int index) {
        final FilmstripItem filmstripItem = mDataAdapter.getItemAt(index);
        if (filmstripItem == null) {
            return "";
        }

        File localFile = new File(filmstripItem.getData().getFilePath());
        return localFile.getName();
    }

    private float fileAgeFromAdapterAtIndex(int index) {
        final FilmstripItem filmstripItem = mDataAdapter.getItemAt(index);
        if (filmstripItem == null) {
            return 0;
        }

        File localFile = new File(filmstripItem.getData().getFilePath());
        return 0.001f * (System.currentTimeMillis() - localFile.lastModified());
    }

    private final FilmstripContentPanel.Listener mFilmstripListener =
            new FilmstripContentPanel.Listener() {

                @Override
                public void onSwipeOut() {
                }

                @Override
                public void onSwipeOutBegin() {
                    mActionBar.hide();
                    mCameraAppUI.hideBottomControls();
                    mFilmstripCoversPreview = false;
                    updatePreviewVisibility();
                }

                @Override
                public void onFilmstripHidden() {
                    mFilmstripVisible = false;
                    UsageStatistics.instance().changeScreen(currentUserInterfaceMode(),
                            NavigationChange.InteractionCause.SWIPE_RIGHT);
                    CameraActivity.this.setFilmstripUiVisibility(false);
                    // When the user hide the filmstrip (either swipe out or
                    // tap on back key) we move to the first item so next time
                    // when the user swipe in the filmstrip, the most recent
                    // one is shown.
                    mFilmstripController.goToFirstItem();
                }

                @Override
                public void onFilmstripShown() {
                    Log.i(TAG, "onFilmstripShown()");
                    if (mCameraAppUI != null && !mCameraAppUI.isModeCoverHide()) {
                        Log.w(TAG, "=====mode cover not hide, not show filmstrip=====");
                        return;
                    }
                    mFilmstripVisible = true;
                    mCameraAppUI.hideCaptureIndicator();
                    UsageStatistics.instance().changeScreen(currentUserInterfaceMode(),
                            NavigationChange.InteractionCause.SWIPE_LEFT);
                    updateUiByData(mFilmstripController.getCurrentAdapterIndex());
                }

                @Override
                public void onFocusedDataLongPressed(int adapterIndex) {
                    // Do nothing.
                }

                @Override
                public void onFocusedDataPromoted(int adapterIndex) {
                    UsageStatistics.instance().mediaInteraction(fileNameFromAdapterAtIndex(
                                adapterIndex),
                            MediaInteraction.InteractionType.DELETE,
                            NavigationChange.InteractionCause.SWIPE_UP, fileAgeFromAdapterAtIndex(
                                adapterIndex));
                    removeItemAt(adapterIndex);
                }

                @Override
                public void onFocusedDataDemoted(int adapterIndex) {
                    UsageStatistics.instance().mediaInteraction(fileNameFromAdapterAtIndex(
                                adapterIndex),
                            MediaInteraction.InteractionType.DELETE,
                            NavigationChange.InteractionCause.SWIPE_DOWN,
                            fileAgeFromAdapterAtIndex(adapterIndex));
                    removeItemAt(adapterIndex);
                }

                @Override
                public void onEnterFullScreenUiShown(int adapterIndex) {
                    if (mFilmstripVisible) {
                        CameraActivity.this.setFilmstripUiVisibility(true);
                    }
                }

                @Override
                public void onLeaveFullScreenUiShown(int adapterIndex) {
                    // Do nothing.
                }

                @Override
                public void onEnterFullScreenUiHidden(int adapterIndex) {
                    if (mFilmstripVisible) {
                        CameraActivity.this.setFilmstripUiVisibility(false);
                    }
                }

                @Override
                public void onLeaveFullScreenUiHidden(int adapterIndex) {
                    // Do nothing.
                }

                @Override
                public void onEnterFilmstrip(int adapterIndex) {
                    if (mFilmstripVisible) {
                        CameraActivity.this.setFilmstripUiVisibility(true);
                    }
                }

                @Override
                public void onLeaveFilmstrip(int adapterIndex) {
                    // Do nothing.
                }

                @Override
                public void onDataReloaded() {
                    if (!mFilmstripVisible) {
                        return;
                    }
                    updateUiByData(mFilmstripController.getCurrentAdapterIndex());
                }

                @Override
                public void onDataUpdated(int adapterIndex) {
                    if (!mFilmstripVisible) {
                        return;
                    }
                    updateUiByData(mFilmstripController.getCurrentAdapterIndex());
                }

                @Override
                public void onEnterZoomView(int adapterIndex) {
                    if (mFilmstripVisible) {
                        CameraActivity.this.setFilmstripUiVisibility(false);
                    }
                }

                @Override
                public void onZoomAtIndexChanged(int adapterIndex, float zoom) {
                    final FilmstripItem filmstripItem = mDataAdapter.getItemAt(adapterIndex);
                    long ageMillis = System.currentTimeMillis()
                          - filmstripItem.getData().getLastModifiedDate().getTime();

                    // Do not log if items is to old or does not have a path (which is
                    // being used as a key).
                    if (TextUtils.isEmpty(filmstripItem.getData().getFilePath()) ||
                            ageMillis > UsageStatistics.VIEW_TIMEOUT_MILLIS) {
                        return;
                    }
                    File localFile = new File(filmstripItem.getData().getFilePath());
                    UsageStatistics.instance().mediaView(localFile.getName(),
                          filmstripItem.getData().getLastModifiedDate().getTime(), zoom);
               }

                @Override
                public void onDataFocusChanged(final int prevIndex, final int newIndex) {
                    if (!mFilmstripVisible) {
                        return;
                    }
                    // TODO: This callback is UI event callback, should always
                    // happen on UI thread. Find the reason for this
                    // runOnUiThread() and fix it.
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            updateUiByData(newIndex);
                        }
                    });
                }

                @Override
                public void onScroll(int firstVisiblePosition, int visibleItemCount, int totalItemCount) {
                    mPreloader.onScroll(null /*absListView*/, firstVisiblePosition, visibleItemCount, totalItemCount);
                }
            };

    private final FilmstripItemListener mFilmstripItemListener =
            new FilmstripItemListener() {
                @Override
                public void onMetadataUpdated(List<Integer> indexes) {
                    if (mPaused) {
                        // Callback after the activity is paused.
                        return;
                    }
                    int currentIndex = mFilmstripController.getCurrentAdapterIndex();
                    for (Integer index : indexes) {
                        if (index == currentIndex) {
                            updateUiByData(index);
                            // Currently we have only 1 data can be matched.
                            // No need to look for more, break.
                            break;
                        }
                    }
                }
            };

    public void gotoGallery() {
        UsageStatistics.instance().changeScreen(NavigationChange.Mode.FILMSTRIP,
                NavigationChange.InteractionCause.BUTTON);

        mFilmstripController.goToNextItem();
    }

    /**
     * If 'visible' is false, this hides the action bar. Also maintains
     * lights-out at all times.
     *
     * @param visible is false, this hides the action bar and filmstrip bottom
     *            controls.
     */
    private void setFilmstripUiVisibility(boolean visible) {
        mLightsOutRunnable.run();
        mCameraAppUI.getFilmstripBottomControls().setVisible(visible);
        if (visible != mActionBar.isShowing()) {
            if (visible) {
                mActionBar.show();
                mCameraAppUI.showBottomControls();
            } else {
                mActionBar.hide();
                mCameraAppUI.hideBottomControls();
            }
        }
        mFilmstripCoversPreview = visible;
        updatePreviewVisibility();
    }

    private void hideSessionProgress() {
        mCameraAppUI.getFilmstripBottomControls().hideProgress();
    }

    private void showSessionProgress(int messageId) {
        CameraAppUI.BottomPanel controls = mCameraAppUI.getFilmstripBottomControls();
        controls.setProgressText(messageId > 0 ? getString(messageId) : "");
        controls.hideControls();
        controls.hideProgressError();
        controls.showProgress();
    }

    private void showProcessError(int messageId) {
        mCameraAppUI.getFilmstripBottomControls().showProgressError(
                messageId > 0 ? getString(messageId) : "");
    }

    private void updateSessionProgress(int progress) {
        mCameraAppUI.getFilmstripBottomControls().setProgress(progress);
    }

    private void updateSessionProgressText(int messageId) {
        mCameraAppUI.getFilmstripBottomControls().setProgressText(
                messageId > 0 ? getString(messageId) : "");
    }

    @Override
    public boolean onShareTargetSelected(ShareActionProvider shareActionProvider, Intent intent) {
        int currentIndex = mFilmstripController.getCurrentAdapterIndex();
        if (currentIndex < 0) {
            return false;
        }
        UsageStatistics.instance().mediaInteraction(fileNameFromAdapterAtIndex(currentIndex),
                MediaInteraction.InteractionType.SHARE,
                NavigationChange.InteractionCause.BUTTON, fileAgeFromAdapterAtIndex(currentIndex));
        // TODO add intent.getComponent().getPackageName()
        return true;
    }

    // Note: All callbacks come back on the main thread.
    private final SessionListener mSessionListener =
            new SessionListener() {
                @Override
                public void onSessionQueued(final Uri uri) {
                    Log.v(TAG, "onSessionQueued: " + uri);
                    if (!Storage.instance().isSessionUri(uri)) {
                        return;
                    }
                    Optional<SessionItem> newData = SessionItem.create(getApplicationContext(), uri);
                    if (newData.isPresent()) {
                        if (mDataAdapter != null)
                            mDataAdapter.addOrUpdate(newData.get());
                    }
                }

                @Override
                public void onSessionUpdated(Uri uri) {
                    Log.v(TAG, "onSessionUpdated: " + uri);
                    mDataAdapter.refresh(uri);
                }

                @Override
                public void onSessionDone(final Uri sessionUri) {
                    Log.v(TAG, "onSessionDone:" + sessionUri);
                    Uri contentUri = Storage.instance().getContentUriForSessionUri(sessionUri);
                    if (contentUri == null) {
                        mDataAdapter.refresh(sessionUri);
                        return;
                    }
                    PhotoItem newData = null;/*mPhotoItemFactory.queryContentUri(contentUri);*/

                    // This can be null if e.g. a session is canceled (e.g.
                    // through discard panorama). It might be worth adding
                    // onSessionCanceled or the like this interface.
                    if (newData == null) {
                        Log.i(TAG, "onSessionDone: Could not find LocalData for URI: " + contentUri);
                        return;
                    }

                    final int pos = mDataAdapter.findByContentUri(sessionUri);
                    if (pos == -1) {
                        // We do not have a placeholder for this image, perhaps
                        // due to the activity crashing or being killed.
                        mDataAdapter.addOrUpdate(newData);
                    } else {
                        // Make the PhotoItem aware of the session placeholder, to
                        // allow it to make a smooth transition to its content if it
                        // the session item is currently visible.
                        FilmstripItem oldSessionData = mDataAdapter.getFilmstripItemAt(pos);
                        if (mCameraAppUI.getFilmstripVisibility() == View.VISIBLE
                                && mFilmstripController.isVisible(oldSessionData)) {
                            Log.v(TAG, "session item visible, setting transition placeholder");
                            newData.setSessionPlaceholderBitmap(
                                    Storage.instance().getPlaceholderForSession(sessionUri));
                        }
                        mDataAdapter.updateItemAt(pos, newData);
                    }
                }

                @Override
                public void onSessionProgress(final Uri uri, final int progress) {
                    if (progress < 0) {
                        // Do nothing, there is no task for this URI.
                        return;
                    }
                    int currentIndex = mFilmstripController.getCurrentAdapterIndex();
                    if (currentIndex == -1) {
                        return;
                    }
                    if (uri.equals(
                            mDataAdapter.getItemAt(currentIndex).getData().getUri())) {
                        updateSessionProgress(progress);
                    }
                }

                @Override
                public void onSessionProgressText(final Uri uri, final int messageId) {
                    int currentIndex = mFilmstripController.getCurrentAdapterIndex();
                    if (currentIndex == -1) {
                        return;
                    }
                    if (uri.equals(
                            mDataAdapter.getItemAt(currentIndex).getData().getUri())) {
                        updateSessionProgressText(messageId);
                    }
                }

                @Override
                public void onSessionCaptureIndicatorUpdate(Bitmap indicator, int rotationDegrees) {
                    // Don't show capture indicator in Photo Sphere.
                    final int photosphereModuleId = getApplicationContext().getResources()
                            .getInteger(
                                    R.integer.camera_mode_photosphere);
                    if (mCurrentModeIndex == photosphereModuleId) {
                        return;
                    }
                    indicateCapture(indicator, rotationDegrees);
                }

                @Override
                public void onSessionFailed(Uri uri, int failureMessageId,
                        boolean removeFromFilmstrip) {
                    Log.v(TAG, "onSessionFailed:" + uri);

                    int failedIndex = mDataAdapter.findByContentUri(uri);
                    int currentIndex = mFilmstripController.getCurrentAdapterIndex();

                    if (currentIndex == failedIndex) {
                        updateSessionProgress(0);
                        showProcessError(failureMessageId);
                        mDataAdapter.refresh(uri);
                    }
                    if (removeFromFilmstrip) {
                        mFatalErrorHandler.onMediaStorageFailure();
                        if (failedIndex > -1)
                            mDataAdapter.removeAt(failedIndex);
                    }
                }

                @Override
                public void onSessionCanceled(Uri uri) {
                    Log.v(TAG, "onSessionCanceled:" + uri);
                    int failedIndex = mDataAdapter.findByContentUri(uri);
                    if (failedIndex > -1)
                        mDataAdapter.removeAt(failedIndex);
                }

                @Override
                public void onSessionThumbnailUpdate(Bitmap bitmap) {
                }

                @Override
                public void onSessionPictureDataUpdate(byte[] pictureData, int orientation) {
                }
            };

    @Override
    public Context getAndroidContext() {
        return mAppContext;
    }

    @Override
    public OneCameraFeatureConfig getCameraFeatureConfig() {
        return mFeatureConfig;
    }

    @Override
    public Dialog createDialog() {
        return new Dialog(this, android.R.style.Theme_Black_NoTitleBar_Fullscreen);
    }

    @Override
    public void launchActivityByIntent(Intent intent) {
        // Starting from L, we prefer not to start edit activity within camera's task.
        mResetToPreviewOnResume = false;
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);

        startActivity(intent);
    }

    @Override
    public int getCurrentModuleIndex() {
        return mCurrentModeIndex;
    }

    @Override
    public String getModuleScope() {
        ModuleAgent agent = mModuleManager.getModuleAgent(mCurrentModeIndex);
        return SettingsManager.getModuleSettingScope(agent.getScopeNamespace());
    }

    @Override
    public String getCameraScope() {
        // if an unopen camera i.e. negative ID is returned, which we've observed in
        // some automated scenarios, just return it as a valid separate scope
        // this could cause user issues, so log a stack trace noting the call path
        // which resulted in this scenario.

        CameraId cameraId =  mCameraController.getCurrentCameraId();

        if(cameraId == null) {
            Log.e(TAG,  "Retrieving Camera Setting Scope with -1");
            return SettingsManager.getCameraSettingScope("-1");
        }

        return SettingsManager.getCameraSettingScope(cameraId.getValue());
    }

    @Override
    public ModuleController getCurrentModuleController() {
        return mCurrentModule;
    }

    @Override
    public int getQuickSwitchToModuleId(int currentModuleIndex) {
        return mModuleManager.getQuickSwitchToModuleId(currentModuleIndex, mSettingsManager,
                mAppContext);
    }

    @Override
    public SurfaceTexture getPreviewBuffer() {
        // TODO: implement this
        return null;
    }

    @Override
    public void onPreviewReadyToStart() {
        mCameraAppUI.onPreviewReadyToStart();
    }

    @Override
    public void onPreviewStarted() {
        mCameraAppUI.onPreviewStarted();
    }

    @Override
    public void addPreviewAreaSizeChangedListener(
            PreviewStatusListener.PreviewAreaChangedListener listener) {
        mCameraAppUI.addPreviewAreaChangedListener(listener);
    }

    @Override
    public void removePreviewAreaSizeChangedListener(
            PreviewStatusListener.PreviewAreaChangedListener listener) {
        mCameraAppUI.removePreviewAreaChangedListener(listener);
    }

    @Override
    public void setupOneShotPreviewListener() {
        mCameraController.setOneShotPreviewCallback(mMainHandler,
                new CameraAgent.CameraPreviewDataCallback() {
                    @Override
                    public void onPreviewFrame(byte[] data, CameraAgent.CameraProxy camera) {
                        mCurrentModule.onPreviewInitialDataReceived();
                        mCameraAppUI.onNewPreviewFrame();
                    }
                }
        );
    }

    @Override
    public void updatePreviewAspectRatio(float aspectRatio) {
        mCameraAppUI.updatePreviewAspectRatio(aspectRatio);
    }

    @Override
    public void updatePreviewTransformFullscreen(Matrix matrix, float aspectRatio) {
        mCameraAppUI.updatePreviewTransformFullscreen(matrix, aspectRatio);
    }

    @Override
    public RectF getFullscreenRect() {
        return mCameraAppUI.getFullscreenRect();
    }

    @Override
    public void updatePreviewTransform(Matrix matrix) {
        mCameraAppUI.updatePreviewTransform(matrix);
    }

    @Override
    public void setPreviewStatusListener(PreviewStatusListener previewStatusListener) {
        mCameraAppUI.setPreviewStatusListener(previewStatusListener);
    }

    @Override
    public FrameLayout getModuleLayoutRoot() {
        return mCameraAppUI.getModuleRootView();
    }

    @Override
    public void setShutterEventsListener(ShutterEventsListener listener) {
        // TODO: implement this
    }

    @Override
    public void setShutterEnabled(boolean enabled) {
        mCameraAppUI.setShutterButtonEnabled(enabled);
    }

    @Override
    public void setShutterLongClickEnabled(boolean enabled) {
        // TODO Auto-generated method stub
        mCameraAppUI.setShutterButtonLongClickEnabled(enabled);
    }

    @Override
    public boolean isShutterEnabled() {
        return mCameraAppUI.isShutterButtonEnabled();
    }

    @Override
    public void startFlashAnimation(boolean shortFlash) {
        mCameraAppUI.startFlashAnimation(shortFlash);
    }

    @Override
    public void startPreCaptureAnimation() {
        // TODO: implement this
    }

    @Override
    public void cancelPreCaptureAnimation() {
        // TODO: implement this
    }

    @Override
    public void startPostCaptureAnimation() {
        // TODO: implement this
    }

    @Override
    public void startPostCaptureAnimation(Bitmap thumbnail) {
        // TODO: implement this
    }

    @Override
    public void cancelPostCaptureAnimation() {
        // TODO: implement this
    }

    @Override
    public OrientationManager getOrientationManager() {
        return mOrientationManager;
    }

    @Override
    public LocationManager getLocationManager() {
        return mLocationManager;
    }

    @Override
    public void lockOrientation() {
        if (mOrientationManager != null) {
            mOrientationManager.lockOrientation();
        }
    }

    @Override
    public void unlockOrientation() {
        if (mOrientationManager != null) {
            mOrientationManager.unlockOrientation();
        }
    }

    /**
     * If not in filmstrip, this shows the capture indicator.
     */
    private void indicateCapture(final Bitmap indicator, final int rotationDegrees) {
        if (mFilmstripVisible) {
            return;
        }

        // Don't show capture indicator in Photo Sphere.
        // TODO: Don't reach into resources to figure out the current mode.
        final int photosphereModuleId = getApplicationContext().getResources().getInteger(
                R.integer.camera_mode_photosphere);
        if (mCurrentModeIndex == photosphereModuleId) {
            return;
        }

        mMainHandler.post(new Runnable() {
            @Override
            public void run() {
                mCameraAppUI.startCaptureIndicatorRevealAnimation(mCurrentModule
                        .getPeekAccessibilityString());
                mCameraAppUI.updateCaptureIndicatorThumbnail(indicator, rotationDegrees);
            }
        });
    }

    @Override
    public void notifyNewMedia(Uri uri) {
        /*if (mPaused) {
            return;
        }*/
        // TODO: This method is running on the main thread. Also we should get
        // rid of that AsyncTask.

        updateStorageSpaceAndHint(null);
        ContentResolver cr = getContentResolver();
        String mimeType = cr.getType(uri);
        Log.v(TAG,"===============notifyNewMedia===================");
        if(mimeType == null) {
            Log.e(TAG, "Can't find video data in content resolver:" + uri);
            return;
        }
        FilmstripItem newData = null;
        if (FilmstripItemUtils.isMimeTypeVideo(mimeType)) {
            sendBroadcast(new Intent(CameraUtil.ACTION_NEW_VIDEO, uri).setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));
            newData = mVideoItemFactory.queryContentUri(uri);
            if (newData == null) {
                Log.e(TAG, "Can't find video data in content resolver:" + uri);
                return;
            }
        } else if (FilmstripItemUtils.isMimeTypeImage(mimeType)) {
            CameraUtil.broadcastNewPicture(mAppContext, uri);
            newData = mPhotoItemFactory.queryContentUri(uri);
            if (newData == null) {
                Log.e(TAG, "Can't find photo data in content resolver:" + uri);
                return;
            }
        } else {
            Log.w(TAG, "Unknown new media with MIME type:" + mimeType + ", uri:" + uri);
            return;
        }

        // We are preloading the metadata for new video since we need the
        // rotation info for the thumbnail.
        new AsyncTask<FilmstripItem, Void, FilmstripItem>() {
            @Override
            protected FilmstripItem doInBackground(FilmstripItem... params) {
                FilmstripItem data = params[0];
                MetadataLoader.loadMetadata(getAndroidContext(), data);
                return data;
            }

            @Override
            protected void onPostExecute(final FilmstripItem data) {
                // TODO: Figure out why sometimes the data is aleady there.
                if (mDataAdapter != null && data != null)
                    mDataAdapter.addOrUpdate(data);

                // Legacy modules don't use CaptureSession, so we show the capture indicator when
                // the item was safed.
                if (mCurrentModule instanceof PhotoModule ||
                        mCurrentModule instanceof VideoModule) {
                    AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
                        @Override
                        public void run() {
                            Log.d(TAG, "generateThumbnail onPostExecute");
                            final Optional<Bitmap> bitmap = data.generateThumbnail(
                                    mAboveFilmstripControlLayout.getWidth(),
                                    mAboveFilmstripControlLayout.getMeasuredHeight());
                            if (bitmap.isPresent()) {
                                indicateCapture(bitmap.get(), 0);
                            }
                            Log.d(TAG, "generateThumbnail onPostExecute end");
                        }
                    });
                }
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR, newData);
    }

    @Override
    public void enableKeepScreenOn(boolean enabled) {
        if (mPaused) {
            return;
        }

        mKeepScreenOn = enabled;
        if (mKeepScreenOn) {
            mMainHandler.removeMessages(MSG_CLEAR_SCREEN_ON_FLAG);
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        } else {
            keepScreenOnForAWhile();
        }
    }

    @Override
    public CameraProvider getCameraProvider() {
        return mCameraController;
    }

    @Override
    public OneCameraOpener getCameraOpener() {
        return mOneCameraOpener;
    }

    private void removeItemAt(int index) {
        if (index > -1)
            mDataAdapter.removeAt(index);
        if (mDataAdapter.getTotalNumber() >= 1) {
            showUndoDeletionBar();
        } else {
            // If camera preview is the only view left in filmstrip,
            // no need to show undo bar.
            mPendingDeletion = true;
            performDeletion();
            if (mFilmstripVisible) {
                mCameraAppUI.getFilmstripContentPanel().animateHide();
            }
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle presses on the action bar items
        switch (item.getItemId()) {
            case android.R.id.home:
                onBackPressed();
                return true;
            case R.id.action_details:
                showDetailsDialog(mFilmstripController.getCurrentAdapterIndex());
                return true;
            case R.id.action_show_on_map:
                int currentDataId = mFilmstripController.getCurrentAdapterIndex();
                if (currentDataId < 0) {
                    return false;
                }
                final FilmstripItem filmstripitem = mDataAdapter.getItemAt(currentDataId);
                if (filmstripitem != null) {
                    final FilmstripItemData data = filmstripitem.getData();
                    if (data != null) {
                        Location location = data.getLocation();
                        if (location != null && location.isValid())
                            CameraUtil.showOnMap(this, location);
                    }
                }
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private boolean isCaptureIntent() {
        if (MediaStore.ACTION_VIDEO_CAPTURE.equals(getIntent().getAction())
                || MediaStore.ACTION_IMAGE_CAPTURE.equals(getIntent().getAction())
                || MediaStore.ACTION_IMAGE_CAPTURE_SECURE.equals(getIntent().getAction())) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * Note: Make sure this callback is unregistered properly when the activity
     * is destroyed since we're otherwise leaking the Activity reference.
     */
    private final CameraExceptionHandler.CameraExceptionCallback mCameraExceptionCallback
        = new CameraExceptionHandler.CameraExceptionCallback() {
                @Override
                public void onCameraError(int errorCode) {
                    // Not a fatal error. only do Log.e().
                    Log.e(TAG, "Camera error callback. error=" + errorCode);
                    onFatalError();
                }
                @Override
                public void onCameraException(
                        RuntimeException ex, String commandHistory, int action, int state) {
                    Log.e(TAG, "Camera Exception", ex);
                    UsageStatistics.instance().cameraFailure(
                            eventprotos.CameraFailure.FailureReason.API_RUNTIME_EXCEPTION,
                            commandHistory, action, state);
                    onFatalError();
                }
                @Override
                public void onDispatchThreadException(RuntimeException ex) {
                    Log.e(TAG, "DispatchThread Exception", ex);
                    UsageStatistics.instance().cameraFailure(
                            eventprotos.CameraFailure.FailureReason.API_TIMEOUT,
                            null, UsageStatistics.NONE, UsageStatistics.NONE);
                    onFatalError();
                }
                private void onFatalError() {
                    if (mCameraFatalError) {
                        return;
                    }
                    mCameraFatalError = true;

                    // If the activity receives exception during onPause, just exit the app.
                    if (mPaused && !isFinishing()) {
                        Log.e(TAG, "Fatal error during onPause, call Activity.finish()");
                        finish();
                    } else {
                        mFatalErrorHandler.handleFatalError(FatalErrorHandler.Reason.CANNOT_CONNECT_TO_CAMERA);
                    }
                }
            };

    @Override
    public void onNewIntentTasks(Intent intent) {
        if (mCameraController.getNumberOfCameras() > 0)
            onModeSelected(getModeIndex());
    }

    @Override
    public void onCreateTasks(Bundle state) {
        AndroidContext.initialize(this);
        Profile profile = mProfiler.create("CameraActivity.onCreateTasks").start();
        CameraPerformanceTracker.onEvent(CameraPerformanceTracker.ACTIVITY_START);
        mBackToast = Toast.makeText(this, R.string.back_to_exit_msg, Toast.LENGTH_SHORT);
        mOnCreateTime = System.currentTimeMillis();
        mAppContext = getApplicationContext();
        mMainHandler = new MainHandler(this, getMainLooper());
        mLocationManager = new LocationManager(mAppContext, shouldUseNoOpLocation());
        mOrientationManager = new OrientationManagerImpl(this, mMainHandler);
        mOrientationManager.addOnOrientationChangeListener(this);
        mSettingsManager = getServices().getSettingsManager();
        mSoundPlayer = new SoundPlayer(mAppContext);
        mFeatureConfig = OneCameraFeatureConfigCreator.createDefault(getContentResolver(),
                getServices().getMemoryManager());
        mFatalErrorHandler = new FatalErrorHandlerImpl(this);
        checkPermissions();
        if (!mHasCriticalPermissions) {
            Log.v(TAG, "onCreate: Missing critical permissions.");
            finish();
            return;
        }
        profile.mark();
        if (!Glide.isSetup()) {
            Context context = getAndroidContext();
            Glide.setup(new GlideBuilder(context)
                .setDecodeFormat(DecodeFormat.ALWAYS_ARGB_8888)
                .setResizeService(new FifoPriorityThreadPoolExecutor(2)));

            Glide glide = Glide.get(context);

            // As a camera we will use a large amount of memory
            // for displaying images.
            glide.setMemoryCategory(MemoryCategory.HIGH);
        }
        profile.mark("Glide.setup");

        mActiveCameraDeviceTracker = ActiveCameraDeviceTracker.instance();
        try {
            mOneCameraOpener = OneCameraModule.provideOneCameraOpener(
                    mFeatureConfig,
                    mAppContext,
                    mActiveCameraDeviceTracker,
                    ResolutionUtil.getDisplayMetrics(this));
            mOneCameraManager = OneCameraModule.provideOneCameraManager();
        } catch (OneCameraException e) {
            // Log error and continue start process while showing error dialog..
            Log.e(TAG, "Creating camera manager failed.", e);
            mFatalErrorHandler.onGenericCameraAccessFailure();
        }
        profile.mark("OneCameraManager.get");

        try {
            mCameraController = new CameraController(mAppContext, this, mMainHandler,
                    CameraAgentFactory.getAndroidCameraAgent(mAppContext,
                            CameraAgentFactory.CameraApi.API_1),
                    CameraAgentFactory.getAndroidCameraAgent(mAppContext,
                            CameraAgentFactory.CameraApi.AUTO),
                    mActiveCameraDeviceTracker);
            mCameraController.setCameraExceptionHandler(
                    new CameraExceptionHandler(mCameraExceptionCallback, mMainHandler));
        } catch (AssertionError e) {
            Log.e(TAG, "Creating camera controller failed.", e);
            mFatalErrorHandler.onGenericCameraAccessFailure();
        }

        // TODO: Try to move all the resources allocation to happen as soon as
        // possible so we can call module.init() at the earliest time.
        mModuleManager = new ModuleManagerImpl();

        ModulesInfo.setupModules(mAppContext, mModuleManager, mFeatureConfig);

        AppUpgrader appUpgrader = new AppUpgrader(this);
        appUpgrader.upgrade(mSettingsManager);

        // Make sure the picture sizes are correctly cached for the current OS
        // version.
        profile.mark();
        try {
            PictureSizeLoader pictureSizeLoader = new PictureSizeLoader(mAppContext);
            pictureSizeLoader.computePictureSizes();
            pictureSizeLoader.release();
        } catch (AssertionError e) {
            Log.e(TAG, "Creating camera controller failed.", e);
            mFatalErrorHandler.onGenericCameraAccessFailure();
        }
        profile.mark("computePictureSizes");
        Keys.setDefaults(mSettingsManager, mAppContext);

        mResolutionSetting = new ResolutionSetting(mSettingsManager, mOneCameraManager,
                getContentResolver());

        getWindow().requestFeature(Window.FEATURE_ACTION_BAR);
        // We suppress this flag via theme when drawing the system preview
        // background, but once we create activity here, reactivate to the
        // default value. The default is important for L, we don't want to
        // change app behavior, just starting background drawable layout.
        if (ApiHelper.isLOrHigher()) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        }

        profile.mark();
        setContentView(R.layout.activity_main);
        profile.mark("setContentView()");
        // A window background is set in styles.xml for the system to show a
        // drawable background with gray color and camera icon before the
        // activity is created. We set the background to null here to prevent
        // overdraw, all views must take care of drawing backgrounds if
        // necessary. This call to setBackgroundDrawable must occur after
        // setContentView, otherwise a background may be set again from the
        // style.
        getWindow().setBackgroundDrawable(null);

        mActionBar = getActionBar();
        // set actionbar background to 100% or 50% transparent
        if (ApiHelper.isLOrHigher()) {
            mActionBar.setBackgroundDrawable(new ColorDrawable(0x00000000));
        } else {
            mActionBar.setBackgroundDrawable(new ColorDrawable(0x80000000));
        }

        mModeListView = (ModeListView) findViewById(R.id.mode_list_layout);
        mModeListView.init(mModuleManager.getSupportedModeIndexList());
        if (ApiHelper.HAS_ROTATION_ANIMATION) {
            setRotationAnimation();
        }
        mModeListView.setVisibilityChangedListener(new ModeListVisibilityChangedListener() {
            @Override
            public void onVisibilityChanged(boolean visible) {
                mModeListVisible = visible;
                mCameraAppUI.setShutterButtonImportantToA11y(!visible);
                updatePreviewVisibility();
            }
        });

        // Check if this is in the secure camera mode.
        Intent intent = getIntent();
        String action = intent.getAction();
        if (INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE.equals(action)
                || ACTION_IMAGE_CAPTURE_SECURE.equals(action)) {
            mSecureCamera = true;
        } else {
            mSecureCamera = intent.getBooleanExtra(SECURE_CAMERA_EXTRA, false);
        }
        mModeListView.setSecureCamera(mSecureCamera);

        if (mSecureCamera) {
            // Change the window flags so that secure camera can show when
            // locked
            Window win = getWindow();
            WindowManager.LayoutParams params = win.getAttributes();
            params.flags |= WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED;
            win.setAttributes(params);

            // Filter for screen off so that we can finish secure camera
            // activity when screen is off.
            IntentFilter filter_screen_off = new IntentFilter(Intent.ACTION_SCREEN_OFF);
            registerReceiver(mShutdownReceiver, filter_screen_off);

            // Filter for phone unlock so that we can finish secure camera
            // via this UI path:
            //    1. from secure lock screen, user starts secure camera
            //    2. user presses home button
            //    3. user unlocks phone
            IntentFilter filter_user_unlock = new IntentFilter(Intent.ACTION_USER_PRESENT);
            registerReceiver(mShutdownReceiver, filter_user_unlock);
        }
        mCameraAppUI = new CameraAppUI(this,
                (MainActivityLayout) findViewById(R.id.activity_root_view), isCaptureIntent());
        CaptureLayoutHelper captureLayoutHelper = mCameraAppUI.getCaptureLayoutHelper();
        if (captureLayoutHelper != null)
            captureLayoutHelper.setActivity(this);

        mCameraAppUI.setFilmstripBottomControlsListener(mMyFilmstripBottomControlListener);

        mAboveFilmstripControlLayout =
                (FrameLayout) findViewById(R.id.camera_filmstrip_content_layout);

        // Add the session listener so we can track the session progress
        // updates.
        getServices().getCaptureSessionManager().addSessionListener(mSessionListener);
        mFilmstripController = ((FilmstripView) findViewById(R.id.filmstrip_view)).getController();
        mFilmstripController.setImageGap(
                getResources().getDimensionPixelSize(R.dimen.camera_film_strip_gap));
        profile.mark("Configure Camera UI");

        mPanoramaViewHelper = new PanoramaViewHelper(this);
        mPanoramaViewHelper.onCreate();

        ContentResolver appContentResolver = mAppContext.getContentResolver();
        GlideFilmstripManager glideManager = new GlideFilmstripManager(mAppContext);
        mPhotoItemFactory = new PhotoItemFactory(CameraActivity.this, glideManager, appContentResolver,
                new PhotoDataFactory());
        mVideoItemFactory = new VideoItemFactory(CameraActivity.this, glideManager, appContentResolver,
                new VideoDataFactory());
        mCameraAppUI.getFilmstripContentPanel().setFilmstripListener(mFilmstripListener);
        if (mSettingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                                        Keys.KEY_SHOULD_SHOW_REFOCUS_VIEWER_CLING)) {
            mCameraAppUI.setupClingForViewer(CameraAppUI.BottomPanel.VIEWER_REFOCUS);
        }

        setModuleFromModeIndex(getModeIndex());

        profile.mark();
        mCameraAppUI.prepareModuleUI();
        profile.mark("Init Current Module UI");
        mCurrentModule.init(this, isSecureCamera(), isCaptureIntent());
        profile.mark("Init CurrentModule");

        preloadFilmstripItems();

        mLocalImagesObserver = new FilmstripContentObserver();
        mLocalVideosObserver = new FilmstripContentObserver();

        getContentResolver().registerContentObserver(
                MediaStore.Images.Media.EXTERNAL_CONTENT_URI, true,
                mLocalImagesObserver);
        getContentResolver().registerContentObserver(
              MediaStore.Video.Media.EXTERNAL_CONTENT_URI, true,
              mLocalVideosObserver);

        mMemoryManager = getServices().getMemoryManager();

        AsyncTask.THREAD_POOL_EXECUTOR.execute(new Runnable() {
            @Override
            public void run() {
                HashMap memoryData = mMemoryManager.queryMemory();
                UsageStatistics.instance().reportMemoryConsumed(memoryData,
                      MemoryQuery.REPORT_LABEL_LAUNCH);
            }
        });

        mMotionManager = getServices().getMotionManager();

        mFirstRunDialog = new FirstRunDialog(this,
              this /* as context */,
              mResolutionSetting,
              mSettingsManager,
              mOneCameraManager,
              new FirstRunDialog.FirstRunDialogListener() {
            @Override
            public void onFirstRunStateReady() {
                // Run normal resume tasks.
                resume();
            }

            @Override
            public void onFirstRunDialogCancelled() {
                // App isn't functional until users finish first run dialog.
                // We need to finish here since users hit back button during
                // first run dialog (b/19593942).
                finish();
            }

            @Override
            public void onCameraAccessException() {
                mFatalErrorHandler.onGenericCameraAccessFailure();
            }
        });
        profile.stop();
    }

    /**
     * Get the current mode index from the Intent or from persistent
     * settings.
     */
    private int getModeIndex() {
        int modeIndex = -1;
        int photoIndex = getResources().getInteger(R.integer.camera_mode_photo);
        int videoIndex = getResources().getInteger(R.integer.camera_mode_video);
        int gcamIndex = getResources().getInteger(R.integer.camera_mode_gcam);
        int captureIntentIndex =
                getResources().getInteger(R.integer.camera_mode_capture_intent);
        String intentAction = getIntent().getAction();
        if (MediaStore.INTENT_ACTION_VIDEO_CAMERA.equals(intentAction)
                || MediaStore.ACTION_VIDEO_CAPTURE.equals(intentAction)) {
            modeIndex = videoIndex;
        } else if (MediaStore.ACTION_IMAGE_CAPTURE.equals(intentAction)
                || MediaStore.ACTION_IMAGE_CAPTURE_SECURE.equals(intentAction)) {
            // Capture intent.
            modeIndex = captureIntentIndex;
        } else if (MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA.equals(intentAction)
                ||MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE.equals(intentAction)
                || MediaStore.ACTION_IMAGE_CAPTURE_SECURE.equals(intentAction)) {
            modeIndex = mSettingsManager.getInteger(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_CAMERA_MODULE_LAST_USED);

            // For upgraders who have not seen the aspect ratio selection screen,
            // we need to drop them back in the photo module and have them select
            // aspect ratio.
            // TODO: Move this to SettingsManager as an upgrade procedure.
            if (!mSettingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_USER_SELECTED_ASPECT_RATIO)) {
                modeIndex = photoIndex;
            }
        } else {
            // If the activity has not been started using an explicit intent,
            // read the module index from the last time the user changed modes
            modeIndex = mSettingsManager.getInteger(SettingsManager.SCOPE_GLOBAL,
                                                    Keys.KEY_STARTUP_MODULE_INDEX);
            if ((modeIndex == gcamIndex &&
                    !GcamHelper.hasGcamAsSeparateModule(mFeatureConfig)) || modeIndex < 0) {
                modeIndex = photoIndex;
            }
        }
        return modeIndex;
    }

    /**
     * Incase the calling package doesn't have ACCESS_FINE_LOCATION permissions, we should not pass
     * it valid location information in exif.
     */
    private boolean shouldUseNoOpLocation () {
        String callingPackage = getCallingPackage();
        if (callingPackage == null) {
            if (isCaptureIntent()) {
                // Activity not started through startActivityForResult.
                return true;
            } else {
                callingPackage = mAppContext.getPackageName();
            }
        }
        PackageInfo packageInfo = null;
        try {
            packageInfo = getPackageManager().getPackageInfo(callingPackage,
                    PackageManager.GET_PERMISSIONS);
        } catch (Exception e) {
            Log.w(TAG, "Unable to get PackageInfo for callingPackage " + callingPackage);
        }
        if (packageInfo != null) {
            if (packageInfo.requestedPermissions == null) {
                // No-permissions at all, were requested by the calling app.
                return true;
            }
            for (int i = 0; i < packageInfo.requestedPermissions.length; i++) {
                if (packageInfo.requestedPermissions[i].equals(
                        Manifest.permission.ACCESS_FINE_LOCATION) &&
                        (packageInfo.requestedPermissionsFlags[i] &
                        PackageInfo.REQUESTED_PERMISSION_GRANTED) != 0) {
                  return false;
                }
            }
        }
        return true;
    }
    /**
     * Call this whenever the mode drawer or filmstrip change the visibility
     * state.
     */
    private void updatePreviewVisibility() {
        if (mCurrentModule == null) {
            return;
        }

        int visibility = getPreviewVisibility();
        mCameraAppUI.onPreviewVisiblityChanged(visibility);
        updatePreviewRendering(visibility);
        mCurrentModule.onPreviewVisibilityChanged(visibility);
    }

    private void updatePreviewRendering(int visibility) {
        if (visibility == ModuleController.VISIBILITY_HIDDEN) {
            mCameraAppUI.pausePreviewRendering();
        } else {
            mCameraAppUI.resumePreviewRendering();
        }
    }

    public int getPreviewVisibility() {
        if (mFilmstripVisible) {
            return ModuleController.VISIBILITY_HIDDEN;
        } else if (mModeListVisible){
            return ModuleController.VISIBILITY_COVERED;
        } else {
            return ModuleController.VISIBILITY_VISIBLE;
        }
    }

    private void setRotationAnimation() {
        int rotationAnimation = WindowManager.LayoutParams.ROTATION_ANIMATION_ROTATE;
        rotationAnimation = WindowManager.LayoutParams.ROTATION_ANIMATION_CROSSFADE;
        Window win = getWindow();
        WindowManager.LayoutParams winParams = win.getAttributes();
        winParams.rotationAnimation = rotationAnimation;
        win.setAttributes(winParams);
    }

    private void setMaxScreenBright() {
        Window win = getWindow();
        WindowManager.LayoutParams winParams = win.getAttributes();
        Log.i(TAG, "screenBrightness = " + winParams.screenBrightness);
        winParams.screenBrightness = WindowManager.LayoutParams.BRIGHTNESS_OVERRIDE_FULL;
        win.setAttributes(winParams);
    }

    private int mOrignalScreenBrightMode = -1;
    private int mOrignalScreenBrightness = -1;
    private void setScreenBrightAdjust() {
        mOrignalScreenBrightMode = getScreenMode();
        mOrignalScreenBrightness = getScreenBrightness();
        Log.i(TAG, "mOrignalScreenBrightMode = " + mOrignalScreenBrightMode
                + ",mOrignalScreenBrightness = " + mOrignalScreenBrightness);
        if (mOrignalScreenBrightMode == Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC)
            setScreenMode(Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);
        if (mOrignalScreenBrightness < ((int) (255 * 0.9f)))
            saveScreenBrightness((int) (255 * 0.9f));
    }

    private Runnable mScreenBrightAdjust = new Runnable() {
        
        @Override
        public void run() {
            // TODO Auto-generated method stub
            setScreenBrightAdjust();
        }
    };

    private void restoreScreenBright() {
        mMainHandler.removeCallbacks(mScreenBrightAdjust);
        setScreenMode(mOrignalScreenBrightMode);
        saveScreenBrightness(mOrignalScreenBrightness);
    }

    /** 
     * getCurrentScreenBrightMode
     * SCREEN_BRIGHTNESS_MODE_AUTOMATIC=1 auto
     * SCREEN_BRIGHTNESS_MODE_MANUAL=0  manual
     */  
    private int getScreenMode() {
        int screenMode=0;
        try {
            screenMode = Settings.System.getInt(getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS_MODE);
        }
        catch (Exception localException){
            Log.e(TAG, "getScreenMode error:" + localException);
        }
        return screenMode;  
    }  

    /** 
     * getScreenBrightness  0--255
     */  
    private int getScreenBrightness() {
        int screenBrightness = 255;
        try {
            screenBrightness = Settings.System.getInt(getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS);
        }
        catch (Exception localException){
            Log.e(TAG, "getScreenBrightness error:" + localException);
        }
        return screenBrightness;  
    }

    /** 
     * setScreenMode
     * SCREEN_BRIGHTNESS_MODE_AUTOMATIC=1 auto 
     * SCREEN_BRIGHTNESS_MODE_MANUAL=0  manual 
     */
    private void setScreenMode(int paramInt){
        if (paramInt < 0) return;
        try{  
            Settings.System.putInt(getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS_MODE, paramInt);
        }catch (Exception localException){
            Log.e(TAG, "setScreenMode error:" + localException);
        }
    }

    /** 
     * saveScreenBrightness  0--255
     */  
    private void saveScreenBrightness(int paramInt){
        if (paramInt < 0) return;
        try{  
            Settings.System.putInt(getContentResolver(),
                    Settings.System.SCREEN_BRIGHTNESS, paramInt);
        }
        catch (Exception localException) {
            Log.e(TAG, "saveScreenBrightness error:" + localException);
        }
    }

    @Override
    public void onUserInteraction() {
        super.onUserInteraction();
        if (!isFinishing()) {
            keepScreenOnForAWhile();
        }
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        boolean result = super.dispatchTouchEvent(ev);
        if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) {
            // Real deletion is postponed until the next user interaction after
            // the gesture that triggers deletion. Until real deletion is
            // performed, users can click the undo button to bring back the
            // image that they chose to delete.
            if (mPendingDeletion && !mIsUndoingDeletion) {
                performDeletion();
            }
        }
        return result;
    }

    @Override
    public void onPauseTasks() {
        CameraPerformanceTracker.onEvent(CameraPerformanceTracker.ACTIVITY_PAUSE);
        Profile profile = mProfiler.create("CameraActivity.onPause").start();
        restoreScreenBright();

        /*
         * Save the last module index after all secure camera and icon launches,
         * not just on mode switches.
         *
         * Right now we exclude capture intents from this logic, because we also
         * ignore the cross-Activity recovery logic in onStart for capture intents.
         */
        if (!isCaptureIntent()) {
            mSettingsManager.set(SettingsManager.SCOPE_GLOBAL,
                                 Keys.KEY_STARTUP_MODULE_INDEX,
                mCurrentModeIndex);
        }

        mPaused = true;
        mCameraAppUI.hideCaptureIndicator();
        mFirstRunDialog.dismiss();

        // Delete photos that are pending deletion
        performDeletion();
        mCurrentModule.pause();
        mOrientationManager.pause();
        mPanoramaViewHelper.onPause();

        mLocalImagesObserver.setForegroundChangeListener(null);
        mLocalImagesObserver.setActivityPaused(true);
        mLocalVideosObserver.setActivityPaused(true);
        if (mPreloader != null) {
            mPreloader.cancelAllLoads();
        }
        resetScreenOn();

        mMotionManager.stop();

        // Always stop recording location when paused. Resume will start
        // location recording again if the location setting is on.
        mLocationManager.recordLocation(false);

        UsageStatistics.instance().backgrounded();

        // Camera is in fatal state. A fatal dialog is presented to users, but users just hit home
        // button. Let's just kill the process.
        if (mCameraFatalError && !isFinishing()) {
            Log.v(TAG, "onPause when camera is in fatal state, call Activity.finish()");
            mCameraFatalError = false;
            CameraUtil.dismissErrorDialog();
            mCameraController.closeCamera(true);
            finish();
        } else {
            // Close the camera and wait for the operation done.
            Log.v(TAG, "onPause closing camera");
            if (mCameraController != null) {
                mCameraController.closeCamera(true);
            }
        }

        profile.stop();
    }

    @Override
    public void onResumeTasks() {
        mPaused = false;
        checkPermissions();
        if (!mHasCriticalPermissions) {
            Log.v(TAG, "onResume: Missing critical permissions.");
            finish();
            return;
        }
        if (!isSecureCamera() && !isCaptureIntent()) {
            // Show the dialog if necessary. The rest resume logic will be invoked
            // at the onFirstRunStateReady() callback.
            try {
                mFirstRunDialog.showIfNecessary();
            } catch (AssertionError e) {
                Log.e(TAG, "Creating camera controller failed.", e);
                mFatalErrorHandler.onGenericCameraAccessFailure();
            }
        } else {
            // In secure mode from lockscreen, we go straight to camera and will
            // show first run dialog next time user enters launcher.
            Log.v(TAG, "in secure mode, skipping first run dialog check");
            resume();
        }
    }

    /**
     * Checks if any of the needed Android runtime permissions are missing.
     * If they are, then launch the permissions activity under one of the following conditions:
     * a) The permissions dialogs have not run yet. We will ask for permission only once.
     * b) If the missing permissions are critical to the app running, we will display a fatal error dialog.
     * Critical permissions are: camera, microphone and storage. The app cannot run without them.
     * Non-critical permission is location.
     */
    private void checkPermissions() {
        if (!ApiHelper.isMOrHigher()) {
            Log.v(TAG, "not running on M, skipping permission checks");
            mHasCriticalPermissions = true;
            return;
        }

        if (checkSelfPermission(Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED &&
                checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED &&
                checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED) {
            mHasCriticalPermissions = true;
        } else {
            mHasCriticalPermissions = false;
        }
        if (!mHasCriticalPermissions || (mSettingsManager.getBoolean(
                SettingsManager.SCOPE_GLOBAL, Keys.KEY_RECORD_LOCATION) &&
                (checkSelfPermission(Manifest.permission.ACCESS_COARSE_LOCATION)
                   != PackageManager.PERMISSION_GRANTED) &&
                !mSettingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                    Keys.KEY_HAS_SEEN_PERMISSIONS_DIALOGS))) {
            // TODO: Convert PermissionsActivity into a dialog so we
            // don't lose the state of CameraActivity.
            Intent intent = new Intent(this, PermissionsActivity.class);
            intent.setAction(getIntent().getAction());
            startActivity(intent);
            finish();
        }
    }

    private void preloadFilmstripItems() {
        if (mDataAdapter == null) {
            mDataAdapter = new CameraFilmstripDataAdapter(mAppContext,
                    mPhotoItemFactory, mVideoItemFactory);
            mDataAdapter.setLocalDataListener(mFilmstripItemListener);
            mPreloader = new Preloader<Integer, AsyncTask>(FILMSTRIP_PRELOAD_AHEAD_ITEMS, mDataAdapter,
                    mDataAdapter);
            if (!mSecureCamera) {
                mFilmstripController.setDataAdapter(mDataAdapter);
                if (!isCaptureIntent()) {
                    mDataAdapter.requestLoad(new Callback<Void>() {
                        @Override
                        public void onCallback(Void result) {
                            fillTemporarySessions();
                        }
                    });
                }
            } else {
                // Put a lock placeholder as the last image by setting its date to
                // 0.
                ImageView v = (ImageView) getLayoutInflater().inflate(
                        R.layout.secure_album_placeholder, null);
                v.setTag(R.id.mediadata_tag_viewtype, FilmstripItemType.SECURE_ALBUM_PLACEHOLDER.ordinal());
                v.setOnClickListener(new View.OnClickListener() {
                    @Override
                    public void onClick(View view) {
                        UsageStatistics.instance().changeScreen(NavigationChange.Mode.GALLERY,
                                NavigationChange.InteractionCause.BUTTON);
                        startGallery();
                        finish();
                    }
                });
                v.setContentDescription(getString(R.string.accessibility_unlock_to_camera));
                mDataAdapter = new FixedLastProxyAdapter(
                        mAppContext,
                        mDataAdapter,
                        new PlaceholderItem(
                                v,
                                FilmstripItemType.SECURE_ALBUM_PLACEHOLDER,
                                v.getDrawable().getIntrinsicWidth(),
                                v.getDrawable().getIntrinsicHeight()));
                // Flush out all the original data.
                mDataAdapter.clear();
                mFilmstripController.setDataAdapter(mDataAdapter);
            }
        }
    }

    private void resume() {
        Profile profile = mProfiler.create("CameraActivity.resume").start();
        CameraPerformanceTracker.onEvent(CameraPerformanceTracker.ACTIVITY_RESUME);
        Log.v(TAG, "Build info: " + Build.DISPLAY);
        updateExternalSD(CameraActivity.this);
        registerMediaMountListener();
        updateStorageSpaceAndHint(null);
        initExposure();

        mLastLayoutOrientation = getResources().getConfiguration().orientation;

        // TODO: Handle this in OrientationManager.
        // Auto-rotate off
//        if (Settings.System.getInt(getContentResolver(),
//                Settings.System.ACCELEROMETER_ROTATION, 0) == 0) {
//            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
//            mAutoRotateScreen = false;
//        } else {
//            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
//            mAutoRotateScreen = true;
//        }
        updateActivityOrientation();

        // Foreground event logging.  ACTION_STILL_IMAGE_CAMERA and
        // INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE are double logged due to
        // lockscreen onResume->onPause->onResume sequence.
        int source;
        String action = getIntent().getAction();
        if (action == null) {
            source = ForegroundSource.UNKNOWN_SOURCE;
        } else {
            switch (action) {
                case MediaStore.ACTION_IMAGE_CAPTURE:
                    source = ForegroundSource.ACTION_IMAGE_CAPTURE;
                    break;
                case MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA:
                    // was UNKNOWN_SOURCE in Fishlake.
                    source = ForegroundSource.ACTION_STILL_IMAGE_CAMERA;
                    break;
                case MediaStore.INTENT_ACTION_VIDEO_CAMERA:
                    // was UNKNOWN_SOURCE in Fishlake.
                    source = ForegroundSource.ACTION_VIDEO_CAMERA;
                    break;
                case MediaStore.ACTION_VIDEO_CAPTURE:
                    source = ForegroundSource.ACTION_VIDEO_CAPTURE;
                    break;
                case MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE:
                    // was ACTION_IMAGE_CAPTURE_SECURE in Fishlake.
                    source = ForegroundSource.ACTION_STILL_IMAGE_CAMERA_SECURE;
                    break;
                case MediaStore.ACTION_IMAGE_CAPTURE_SECURE:
                    source = ForegroundSource.ACTION_IMAGE_CAPTURE_SECURE;
                    break;
                case Intent.ACTION_MAIN:
                    source = ForegroundSource.ACTION_MAIN;
                    break;
                default:
                    source = ForegroundSource.UNKNOWN_SOURCE;
                    break;
            }
        }
        UsageStatistics.instance().foregrounded(source, currentUserInterfaceMode(),
                isKeyguardSecure(), isKeyguardLocked(),
                mStartupOnCreate, mExecutionStartNanoTime);

        mGalleryIntent = IntentHelper.getGalleryIntent(mAppContext);
        if (ApiHelper.isLOrHigher()) {
            // hide the up affordance for L devices, it's not very Materially
            mActionBar.setDisplayShowHomeEnabled(false);
        }

        mOrientationManager.resume();

        mCurrentModule.hardResetSettings(mSettingsManager);

        profile.mark();
        if (mCameraController.getNumberOfCameras() > 0)
            mCurrentModule.resume();
        UsageStatistics.instance().changeScreen(currentUserInterfaceMode(),
                NavigationChange.InteractionCause.BUTTON);
        setSwipingEnabled(true);
        profile.mark("mCurrentModule.resume");

        if (!mResetToPreviewOnResume) {
            FilmstripItem item = mDataAdapter.getItemAt(
                  mFilmstripController.getCurrentAdapterIndex());
            if (item != null) {
                mDataAdapter.refresh(item.getData().getUri());
            }
        }

        // The share button might be disabled to avoid double tapping.
        mCameraAppUI.getFilmstripBottomControls().setShareEnabled(true);
        // Default is showing the preview, unless disabled by explicitly
        // starting an activity we want to return from to the filmstrip rather
        // than the preview.
        mResetToPreviewOnResume = true;

        if (mLocalVideosObserver.isMediaDataChangedDuringPause()
                || mLocalImagesObserver.isMediaDataChangedDuringPause()) {
            if (!mSecureCamera) {
                // If it's secure camera, requestLoad() should not be called
                // as it will load all the data.
                if (!mFilmstripVisible) {
                    mDataAdapter.requestLoad(new Callback<Void>() {
                        @Override
                        public void onCallback(Void result) {
                            fillTemporarySessions();
                        }
                    });
                } else {
                    //mDataAdapter.requestLoadNewPhotos();
                }
            }
        }
        mLocalImagesObserver.setActivityPaused(false);
        mLocalVideosObserver.setActivityPaused(false);
        if (!mSecureCamera) {
            mLocalImagesObserver.setForegroundChangeListener(
                    new FilmstripContentObserver.ChangeListener() {
                @Override
                public void onChange() {
                    if (!mSecureCamera && !isCaptureIntent()) {
                        Log.d(TAG, "LocalImagesObserver changed mDataAdapter.requestLoad");
                        mIsNeedReLoadImage = false;
                        mDataAdapter.requestLoad(new Callback<Void>() {
                            @Override
                            public void onCallback(Void result) {
                                fillTemporarySessions();
                            }
                        });
                    } else {
                        //Log.d(TAG, "LocalImagesObserver changed mDataAdapter.requestLoadNewPhotos");
                        //mDataAdapter.requestLoadNewPhotos();
                    }
                    updateStorageSpaceAndHint(null);
                }
            });
            mLocalVideosObserver.setForegroundChangeListener(
                    new FilmstripContentObserver.ChangeListener() {
                @Override
                public void onChange() {
                    if (!mSecureCamera && !isCaptureIntent() && !mFilmstripVisible) {
                        Log.d(TAG, "LocalVideosObserver changed mDataAdapter.requestLoad");
                        mDataAdapter.requestLoad(new Callback<Void>() {
                            @Override
                            public void onCallback(Void result) {
                                fillTemporarySessions();
                            }
                        });
                    } else {
                        //Log.d(TAG, "LocalImagesObserver changed mDataAdapter.requestLoadNewPhotos");
                        //mDataAdapter.requestLoadNewPhotos();
                    }
                    updateStorageSpaceAndHint(null);
                }
            });
        }

        keepScreenOnForAWhile();
        mMainHandler.postDelayed(mScreenBrightAdjust, 100);

        // Lights-out mode at all times.
        final View rootView = findViewById(R.id.activity_root_view);
        mLightsOutRunnable.run();
        getWindow().getDecorView().setOnSystemUiVisibilityChangeListener(
              new OnSystemUiVisibilityChangeListener() {
                  @Override
                  public void onSystemUiVisibilityChange(int visibility) {
                      mMainHandler.removeCallbacks(mLightsOutRunnable);
                      mMainHandler.postDelayed(mLightsOutRunnable, LIGHTS_OUT_DELAY_MS);
                  }
              });

        profile.mark();
        mPanoramaViewHelper.onResume();
        profile.mark("mPanoramaViewHelper.onResume()");

        ReleaseHelper.showReleaseInfoDialogOnStart(this, mSettingsManager);
        // Enable location recording if the setting is on.
        final boolean locationRecordingEnabled =
                mSettingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL, Keys.KEY_RECORD_LOCATION);
        mLocationManager.recordLocation(locationRecordingEnabled);

        final int previewVisibility = getPreviewVisibility();
        updatePreviewRendering(previewVisibility);

        mMotionManager.start();
        mCameraAppUI.checkOrientation();
        profile.stop();
    }

    private void fillTemporarySessions() {
        if (mSecureCamera) {
            return;
        }
        // There might be sessions still in flight (processed by our service).
        // Make sure they're added to the filmstrip.
        getServices().getCaptureSessionManager().fillTemporarySession(mSessionListener);
    }

    @Override
    public void onStartTasks() {
        mIsActivityRunning = true;
        mPanoramaViewHelper.onStart();

        /*
         * If we're starting after launching a different Activity (lockscreen),
         * we need to use the last mode used in the other Activity, and
         * not the old one from this Activity.
         *
         * This needs to happen before CameraAppUI.resume() in order to set the
         * mode cover icon to the actual last mode used.
         *
         * Right now we exclude capture intents from this logic.
         */
        int modeIndex = getModeIndex();
        if (!isCaptureIntent() && mCurrentModeIndex != modeIndex
                && mCameraController.getNumberOfCameras() > 0) {
            onModeSelected(modeIndex);
        }

        if (mResetToPreviewOnResume) {
            mCameraAppUI.resume();
            mResetToPreviewOnResume = false;
        }
    }

    @Override
    protected void onStopTasks() {
        mIsActivityRunning = false;
        mPanoramaViewHelper.onStop();

        mLocationManager.disconnect();
        CameraUtil.cleanOrientationParameters();

        unregisterMediaMountListener();
    }

    @Override
    public void onDestroyTasks() {
        if (mSecureCamera) {
            unregisterReceiver(mShutdownReceiver);
        }

        // Ensure anything that checks for "isPaused" returns true.
        mPaused = true;
        mBackToast = null;
        if (mFilmstripController != null)
            mFilmstripController.setDataAdapter(null);
        if (mDataAdapter != null) {
            mDataAdapter.setListener(null);
            mDataAdapter.setLocalDataListener(null);
            mDataAdapter = null;
        }
        if (mOrientationManager != null){
            mOrientationManager.removeOnOrientationChangeListener(this);
        }
        if (mCameraAppUI != null) {
            mCameraAppUI.getFilmstripContentPanel().setFilmstripListener(null);
            mCameraAppUI.setFilmstripBottomControlsListener(null);
        }
        getApplicationContext().unregisterComponentCallbacks((ComponentCallbacks) mMemoryManager);

        mSettingsManager.removeAllListeners();
        if (mCameraController != null) {
            mCameraController.removeCallbackReceiver();
            mCameraController.setCameraExceptionHandler(null);
        }
        if (mLocalImagesObserver != null) {
            getContentResolver().unregisterContentObserver(mLocalImagesObserver);
        }
        if (mLocalVideosObserver != null) {
            getContentResolver().unregisterContentObserver(mLocalVideosObserver);
        }
        getServices().getCaptureSessionManager().removeSessionListener(mSessionListener);
        if (mCameraAppUI != null) {
            mCameraAppUI.onDestroy();
        }
        if (mModeListView != null) {
            mModeListView.setVisibilityChangedListener(null);
        }
        mCameraController = null;
        mSettingsManager = null;
        mOrientationManager = null;
        mButtonManager = null;
        if (mSoundPlayer != null) {
          mSoundPlayer.release();
        }
        if (mFirstRunDialog != null) {
            mFirstRunDialog.dismiss();
        }
		if (mCurrentModule != null) {
            mCurrentModule.destroy();
        }
        CameraAgentFactory.recycle(CameraAgentFactory.CameraApi.API_1);
        CameraAgentFactory.recycle(CameraAgentFactory.CameraApi.AUTO);
    }

    @Override
    public void onConfigurationChanged(Configuration config) {
        super.onConfigurationChanged(config);
        Log.v(TAG, "onConfigurationChanged");
        if (config.orientation == Configuration.ORIENTATION_UNDEFINED) {
            return;
        }

        if (mLastLayoutOrientation != config.orientation) {
            mLastLayoutOrientation = config.orientation;
            mCurrentModule.onLayoutOrientationChanged(
                    mLastLayoutOrientation == Configuration.ORIENTATION_LANDSCAPE);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (ActivityManager.isUserAMonkey())
            return super.onKeyDown(keyCode, event);
        if (!mFilmstripVisible) {
            if (mCurrentModule.onKeyDown(keyCode, event)) {
                return true;
            }
            // Prevent software keyboard or voice search from showing up.
            if (keyCode == KeyEvent.KEYCODE_SEARCH
                    || keyCode == KeyEvent.KEYCODE_MENU) {
                if (event.isLongPress()) {
                    return true;
                }
            }
        }

        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (ActivityManager.isUserAMonkey())
            return super.onKeyUp(keyCode, event);
        if (!mFilmstripVisible) {
            // If a module is in the middle of capture, it should
            // consume the key event.
            if (mCurrentModule.onKeyUp(keyCode, event)) {
                Log.i(TAG, "onKeyUp->mCurrentModule->onKeyup->true");
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_MENU
                    || keyCode == KeyEvent.KEYCODE_DPAD_LEFT) {
                // Let the mode list view consume the event.
                mCameraAppUI.openModeList();
                return true;
            } else if(keyCode == KeyEvent.KEYCODE_DPAD_DOWN){
                Log.i(TAG, "onKeyUp->keycode_dpad_down");
                if(mModeListView.getVisibility() == View.VISIBLE && mCurrentModeIndex == 0){
                    mModeListView.getListView().getChildAt(1).performClick();
                }
            } else if(keyCode == KeyEvent.KEYCODE_DPAD_UP){
                Log.i(TAG, "onKeyUp->keycode_dpad_up");
                if(mModeListView.getVisibility() == View.VISIBLE && mCurrentModeIndex == 1){
                    mModeListView.getListView().getChildAt(0).performClick();
                }
            } else if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) {
                if (mDataAdapter.getTotalNumber() > 0)
                    mCameraAppUI.showFilmstrip();
                return true;
            }
        } else {
            if (keyCode == KeyEvent.KEYCODE_DPAD_RIGHT) {
                mFilmstripController.goToNextItem();
                return true;
            } else if (keyCode == KeyEvent.KEYCODE_DPAD_LEFT) {
                boolean wentToPrevious = mFilmstripController.goToPreviousItem();
                if (!wentToPrevious) {
                  // at beginning of filmstrip, hide and go back to preview
                  mCameraAppUI.hideFilmstrip();
                }
                return true;
            }
        }
        return super.onKeyUp(keyCode, event);
    }

    private int mBackKeyPressTimes = 0;
    @Override
    public void onBackPressed() {
        if (!mCameraAppUI.onBackPressed()) {
            if (!mCurrentModule.onBackPressed()) {
                if (ActivityManager.isUserAMonkey()) {
                    super.onBackPressed();
                    return;
                }
                if (mBackKeyPressTimes == 0) {
                    if (mBackToast != null) {
                        mBackToast.show();
                    }
                    mBackKeyPressTimes++;
                    mMainHandler.postDelayed(new Runnable() {
                        
                        @Override
                        public void run() {
                            // TODO Auto-generated method stub
                            mBackKeyPressTimes = 0;
                            if (mBackToast != null) {
                                mBackToast.cancel();
                            }
                        }
                    }, 2000);
                    return;
                }
                if (mCameraAppUI != null) {
                    mCameraAppUI.pauseTextViewHelper();
                }
                if (mBackKeyPressTimes >= 1) {
                    if (mBackToast != null) {
                        mBackToast.cancel();
                    }
                    super.onBackPressed();
                }
            }
        }
    }

    @Override
    public boolean isAutoRotateScreen() {
        // TODO: Move to OrientationManager.
        return mAutoRotateScreen;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.filmstrip_menu, menu);
        mActionBarMenu = menu;

        // add a button for launching the gallery
        if (mGalleryIntent != null) {
            CharSequence appName =  IntentHelper.getGalleryAppName(mAppContext, mGalleryIntent);
            if (appName != null) {
                if(appName.equals("Gallery")){
                    appName = getString(R.string.gallery_name);
                }
                MenuItem menuItem = menu.add(appName);
                menuItem.setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
                menuItem.setIntent(mGalleryIntent);

                Drawable galleryLogo = IntentHelper.getGalleryIcon(mAppContext, mGalleryIntent);
                if (galleryLogo != null) {
                    menuItem.setIcon(galleryLogo);
                }
            }
        }

        return super.onCreateOptionsMenu(menu);
    }

    protected long getStorageSpaceBytes() {
        synchronized (mStorageSpaceLock) {
            return mStorageSpaceBytes;
        }
    }

    protected interface OnStorageUpdateDoneListener {
        public void onStorageUpdateDone(long bytes);
    }

    protected void updateStorageSpaceAndHint(final OnStorageUpdateDoneListener callback) {
        /*
         * We execute disk operations on a background thread in order to
         * free up the UI thread.  Synchronizing on the lock below ensures
         * that when getStorageSpaceBytes is called, the main thread waits
         * until this method has completed.
         *
         * However, .execute() does not ensure this execution block will be
         * run right away (.execute() schedules this AsyncTask for sometime
         * in the future. executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR)
         * tries to execute the task in parellel with other AsyncTasks, but
         * there's still no guarantee).
         * e.g. don't call this then immediately call getStorageSpaceBytes().
         * Instead, pass in an OnStorageUpdateDoneListener.
         */
        if (mUpdateStorageTask != null) 
            mUpdateStorageTask.cancel(true);
        mUpdateStorageTask = (new AsyncTask<Void, Void, Long>() {
            @Override
            protected Long doInBackground(Void ... arg) {
                synchronized (mStorageSpaceLock) {
                    mStorageSpaceBytes = Storage.instance().getAvailableSpace();
                    //mOtherStorageSpaceBytes = Storage.instance().getOtherAvailableSpace();
                    return mStorageSpaceBytes;
                }
            }

            @Override
            protected void onPostExecute(Long bytes) {
                updateStorageHint(bytes);
                // This callback returns after I/O to check disk, so we could be
                // pausing and shutting down. If so, don't bother invoking.
                if (callback != null && !mPaused) {
                    callback.onStorageUpdateDone(bytes);
                } else {
                    Log.v(TAG, "ignoring storage callback after activity pause");
                }
            }
        }).executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        Log.d(TAG, "mUpdateStorageTask = " + mUpdateStorageTask);
    }

    protected void updateStorageHint(long storageSpace) {
        if (!mIsActivityRunning) {
            return;
        }

        String message = null;
        boolean changeStorage = false;
        if (storageSpace == Storage.UNAVAILABLE) {
            message = getString(R.string.no_storage);
        } else if (storageSpace == Storage.PREPARING) {
            message = getString(R.string.preparing_sd);
        } else if (storageSpace == Storage.UNKNOWN_SIZE) {
            message = getString(R.string.access_sd_fail);
        } else if (storageSpace <= Storage.LOW_STORAGE_THRESHOLD_BYTES) {
            /*if (mOtherStorageSpaceBytes > Storage.LOW_STORAGE_THRESHOLD_BYTES) {
                if (Storage.DEFAULT_DIRECTORY.equals(Storage.DIRECTORY))
                    message = getString(R.string.flash_spaceIsLow_content);
                else
                    message = getString(R.string.exsd_spaceIsLow_content);
                changeStorage = true;
            } else*/
                message = getString(R.string.spaceIsLow_content);
        }

        if (message != null) {
            Log.w(TAG, "Storage warning: " + message);
            if (mStorageHint == null) {
                mStorageHint = OnScreenHint.makeText(CameraActivity.this, message);
            } else {
                mStorageHint.setText(message);
            }
            mStorageHint.show();
            UsageStatistics.instance().storageWarning(storageSpace);

            if (changeStorage) {
                mCameraAppUI.enableModeOptions();
                mCameraAppUI.setShutterButtonEnabled(false);
                mCameraAppUI.setSwipeEnabled(true);
                mCameraAppUI.hideModeListView();
            } else 
                // Disable all user interactions,
                mCameraAppUI.setDisableAllUserInteractions(true);
        } else if (mStorageHint != null) {
            mStorageHint.cancel();
            mStorageHint = null;

            // Re-enable all user interactions.
            mCameraAppUI.setDisableAllUserInteractions(false);
        }
    }

    protected void setResultEx(int resultCode) {
        mResultCodeForTesting = resultCode;
        setResult(resultCode);
    }

    protected void setResultEx(int resultCode, Intent data) {
        mResultCodeForTesting = resultCode;
        mResultDataForTesting = data;
        setResult(resultCode, data);
    }

    public int getResultCode() {
        return mResultCodeForTesting;
    }

    public Intent getResultData() {
        return mResultDataForTesting;
    }

    public boolean isSecureCamera() {
        return mSecureCamera;
    }

    @Override
    public boolean isPaused() {
        return mPaused;
    }

    @Override
    public int getPreferredChildModeIndex(int modeIndex) {
        if (modeIndex == getResources().getInteger(R.integer.camera_mode_photo)) {
            boolean hdrPlusOn = Keys.isHdrPlusOn(mSettingsManager);
            if (hdrPlusOn && GcamHelper.hasGcamAsSeparateModule(mFeatureConfig)) {
                modeIndex = getResources().getInteger(R.integer.camera_mode_gcam);
            }
        }
        return modeIndex;
    }

    @Override
    public void onModeSelected(int modeIndex) {
        Log.i(TAG,"onModeSelected = " + modeIndex);
        if (mCurrentModeIndex == modeIndex) {
            return;
        }

        CameraPerformanceTracker.onEvent(CameraPerformanceTracker.MODE_SWITCH_START);
        // Record last used camera mode for quick switching
        if (modeIndex == getResources().getInteger(R.integer.camera_mode_photo)
                || modeIndex == getResources().getInteger(R.integer.camera_mode_gcam)) {
            mSettingsManager.set(SettingsManager.SCOPE_GLOBAL,
                                 Keys.KEY_CAMERA_MODULE_LAST_USED,
                                 modeIndex);
        }

        closeModule(mCurrentModule);

        // Select the correct module index from the mode switcher index.
        modeIndex = getPreferredChildModeIndex(modeIndex);
        setModuleFromModeIndex(modeIndex);

        mCameraAppUI.resetBottomControls(mCurrentModule, modeIndex);
        mCameraAppUI.addShutterListener(mCurrentModule);
        openModule(mCurrentModule);
        mCurrentModule.onOrientationChanged(mLastRawOrientation);
        // Store the module index so we can use it the next time the Camera
        // starts up.
        mSettingsManager.set(SettingsManager.SCOPE_GLOBAL,
                             Keys.KEY_STARTUP_MODULE_INDEX, modeIndex);
    }

    /**
     * Shows the settings dialog.
     */
    @Override
    public void onSettingsSelected() {
        UsageStatistics.instance().controlUsed(
                eventprotos.ControlEvent.ControlType.OVERALL_SETTINGS);
        Intent intent = new Intent(this, CameraSettingsActivity.class);
        if (!isKeyguardLocked()) {
            startActivity(intent);
        } else {
            /* Need to explicitly request keyguard dismissal for PIN/pattern
             * entry to show up directly. */
            requestDismissKeyguard(
                /* requesting Activity: */ CameraActivity.this,
                new KeyguardDismissCallback() {
                    @Override
                    public void onDismissSucceeded() {
                        /* Need to use launchActivityByIntent() so that going
                         * back from settings after unlock leads to main
                         * activity instead of dismissing camera entirely. */
                        launchActivityByIntent(intent);
                    }
                    @Override
                    public void onDismissError() {
                        Log.e(TAG, "Keyguard dismissal failed.");
                    }
                    @Override
                    public void onDismissCancelled() {
                        Log.d(TAG, "Keyguard dismissal canceled.");
                    }
                }
            );
        }
    }

    @Override
    public void freezeScreenUntilPreviewReady() {
        mCameraAppUI.freezeScreenUntilPreviewReady();
    }

    @Override
    public int getModuleId(int modeIndex) {
        ModuleManagerImpl.ModuleAgent agent = mModuleManager.getModuleAgent(modeIndex);
        if (agent == null) {
            return -1;
        }
        return agent.getModuleId();
    }

    /**
     * Sets the mCurrentModuleIndex, creates a new module instance for the given
     * index an sets it as mCurrentModule.
     */
    private void setModuleFromModeIndex(int modeIndex) {
        ModuleManagerImpl.ModuleAgent agent = mModuleManager.getModuleAgent(modeIndex);
        if (agent == null) {
            return;
        }
        if (!agent.requestAppForCamera()) {
            mCameraController.closeCamera(true);
        }
        mCurrentModeIndex = agent.getModuleId();
        mCurrentModule = (CameraModule) agent.createModule(this, getIntent());
    }

    @Override
    public SettingsManager getSettingsManager() {
        if (mSettingsManager == null) {
            Log.i(TAG, "reget mSettingsManager");
            mSettingsManager = getServices().getSettingsManager();
        }
        return mSettingsManager;
    }

    @Override
    public ResolutionSetting getResolutionSetting() {
        return mResolutionSetting;
    }

    @Override
    public CameraServices getServices() {
        return CameraServicesImpl.instance();
    }

    @Override
    public FatalErrorHandler getFatalErrorHandler() {
        return mFatalErrorHandler;
    }

    public List<String> getSupportedModeNames() {
        List<Integer> indices = mModuleManager.getSupportedModeIndexList();
        List<String> supported = new ArrayList<String>();

        for (Integer modeIndex : indices) {
            String name = CameraUtil.getCameraModeText(modeIndex, mAppContext);
            if (name != null && !name.equals("")) {
                supported.add(name);
            }
        }
        return supported;
    }

    @Override
    public ButtonManager getButtonManager() {
        if (mButtonManager == null) {
            mButtonManager = new ButtonManager(this);
        }
        return mButtonManager;
    }

    @Override
    public SoundPlayer getSoundPlayer() {
        return mSoundPlayer;
    }

    /**
     * Launches an ACTION_EDIT intent for the given local data item. If
     * 'withTinyPlanet' is set, this will show a disambig dialog first to let
     * the user start either the tiny planet editor or another photo editor.
     *
     * @param data The data item to edit.
     */
    public void launchEditor(FilmstripItem data) {
        Intent intent = new Intent(Intent.ACTION_EDIT)
                .setDataAndType(data.getData().getUri(), data.getData().getMimeType())
                .setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            launchActivityByIntent(intent);
        } catch (ActivityNotFoundException e) {
            final String msgEditWith = getResources().getString(R.string.edit_with);
            launchActivityByIntent(Intent.createChooser(intent, msgEditWith));
        }
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);

        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.filmstrip_context_menu, menu);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case R.id.tiny_planet_editor:
                mMyFilmstripBottomControlListener.onTinyPlanet();
                return true;
            case R.id.photo_editor:
                mMyFilmstripBottomControlListener.onEdit();
                return true;
        }
        return false;
    }

    /**
     * Launch the tiny planet editor.
     *
     * @param data The data must be a 360 degree stereographically mapped
     *            panoramic image. It will not be modified, instead a new item
     *            with the result will be added to the filmstrip.
     */
    public void launchTinyPlanetEditor(FilmstripItem data) {
        TinyPlanetFragment fragment = new TinyPlanetFragment();
        Bundle bundle = new Bundle();
        bundle.putString(TinyPlanetFragment.ARGUMENT_URI, data.getData().getUri().toString());
        bundle.putString(TinyPlanetFragment.ARGUMENT_TITLE, data.getData().getTitle());
        fragment.setArguments(bundle);
        fragment.show(getFragmentManager(), "tiny_planet");
    }

    /**
     * Returns what UI mode (capture mode or filmstrip) we are in.
     * Returned number one of {@link com.google.common.logging.eventprotos.NavigationChange.Mode}
     */
    private int currentUserInterfaceMode() {
        int mode = NavigationChange.Mode.UNKNOWN_MODE;
        if (mCurrentModeIndex == getResources().getInteger(R.integer.camera_mode_photo)) {
            mode = NavigationChange.Mode.PHOTO_CAPTURE;
        }
        if (mCurrentModeIndex == getResources().getInteger(R.integer.camera_mode_video)) {
            mode = NavigationChange.Mode.VIDEO_CAPTURE;
        }
        if (mCurrentModeIndex == getResources().getInteger(R.integer.camera_mode_refocus)) {
            mode = NavigationChange.Mode.LENS_BLUR;
        }
        if (mCurrentModeIndex == getResources().getInteger(R.integer.camera_mode_gcam)) {
            mode = NavigationChange.Mode.HDR_PLUS;
        }
        if (mCurrentModeIndex == getResources().getInteger(R.integer.camera_mode_photosphere)) {
            mode = NavigationChange.Mode.PHOTO_SPHERE;
        }
        if (mCurrentModeIndex == getResources().getInteger(R.integer.camera_mode_panorama)) {
            mode = NavigationChange.Mode.PANORAMA;
        }
        if (mFilmstripVisible) {
            mode = NavigationChange.Mode.FILMSTRIP;
        }
        return mode;
    }

    private void openModule(CameraModule module) {
        module.init(this, isSecureCamera(), isCaptureIntent());
        module.hardResetSettings(mSettingsManager);
        // Hide accessibility zoom UI by default. Modules will enable it themselves if required.
        getCameraAppUI().hideAccessibilityZoomUI();
        if (!mPaused) {
            module.resume();
            UsageStatistics.instance().changeScreen(currentUserInterfaceMode(),
                    NavigationChange.InteractionCause.BUTTON);
            updatePreviewVisibility();
        }
    }

    private void closeModule(CameraModule module) {
        module.pause();
        mCameraAppUI.clearModuleUI();
    }

    private void performDeletion() {
        if (!mPendingDeletion) {
            return;
        }
        hideUndoDeletionBar(false);
        mDataAdapter.executeDeletion();
    }

    public void showUndoDeletionBar() {
        if (mPendingDeletion) {
            performDeletion();
        }
        Log.v(TAG, "showing undo bar");
        mPendingDeletion = true;
        if (mUndoDeletionBar == null) {
            ViewGroup v = (ViewGroup) getLayoutInflater().inflate(R.layout.undo_bar,
                    mAboveFilmstripControlLayout, true);
            mUndoDeletionBar = (ViewGroup) v.findViewById(R.id.camera_undo_deletion_bar);
            View button = mUndoDeletionBar.findViewById(R.id.camera_undo_deletion_button);
            button.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View view) {
                    mDataAdapter.undoDeletion();
                    // Fix for b/21666018: When undoing a delete in Fullscreen
                    // mode, just flip
                    // back to the filmstrip to force a refresh.
                    if (mFilmstripController.inFullScreen()) {
                        mFilmstripController.goToFilmstrip();
                    }
                    hideUndoDeletionBar(true);
                }
            });
            // Setting undo bar clickable to avoid touch events going through
            // the bar to the buttons (eg. edit button, etc) underneath the bar.
            mUndoDeletionBar.setClickable(true);
            // When there is user interaction going on with the undo button, we
            // do not want to hide the undo bar.
            button.setOnTouchListener(new View.OnTouchListener() {
                @Override
                public boolean onTouch(View v, MotionEvent event) {
                    if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                        mIsUndoingDeletion = true;
                    } else if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                        mIsUndoingDeletion = false;
                    }
                    return false;
                }
            });
        }
        mUndoDeletionBar.setAlpha(0f);
        mUndoDeletionBar.setVisibility(View.VISIBLE);
        mUndoDeletionBar.animate().setDuration(200).alpha(1f).setListener(null).start();
    }

    private void hideUndoDeletionBar(boolean withAnimation) {
        Log.v(TAG, "Hiding undo deletion bar");
        mPendingDeletion = false;
        if (mUndoDeletionBar != null) {
            if (withAnimation) {
                mUndoDeletionBar.animate().setDuration(200).alpha(0f)
                        .setListener(new Animator.AnimatorListener() {
                            @Override
                            public void onAnimationStart(Animator animation) {
                                // Do nothing.
                            }

                            @Override
                            public void onAnimationEnd(Animator animation) {
                                mUndoDeletionBar.setVisibility(View.GONE);
                            }

                            @Override
                            public void onAnimationCancel(Animator animation) {
                                // Do nothing.
                            }

                            @Override
                            public void onAnimationRepeat(Animator animation) {
                                // Do nothing.
                            }
                        }).start();
            } else {
                mUndoDeletionBar.setVisibility(View.GONE);
            }
        }
    }

    @Override
    public void onOrientationChanged(OrientationManager orientationManager,
            DeviceOrientation orientation) {
        // TODO Auto-generated method stub
        if (orientation.getDegrees() != mLastRawOrientation) {
            Log.v(TAG, "orientation changed (from:to) " + mLastRawOrientation +
                    ":" + orientation.getDegrees());
        }

        // We keep the last known orientation. So if the user first orient
        // the camera then point the camera to floor or sky, we still have
        // the correct orientation.
        if (orientation.getDegrees() == OrientationEventListener.ORIENTATION_UNKNOWN) {
            return;
        }
        mLastRawOrientation = orientation.getDegrees();
        if (mCurrentModule != null) {
            mCurrentModule.onOrientationChanged(orientation.getDegrees());
        }
    }

    /**
     * Enable/disable swipe-to-filmstrip. Will always disable swipe if in
     * capture intent.
     *
     * @param enable {@code true} to enable swipe.
     */
    public void setSwipingEnabled(boolean enable) {
        // TODO: Bring back the functionality.
        if (isCaptureIntent()) {
            // lockPreview(true);
        } else {
            // lockPreview(!enable);
        }
    }

    // Accessor methods for getting latency times used in performance testing
    public long getFirstPreviewTime() {
        if (mCurrentModule instanceof PhotoModule) {
            long coverHiddenTime = getCameraAppUI().getCoverHiddenTime();
            if (coverHiddenTime != -1) {
                return coverHiddenTime - mOnCreateTime;
            }
        }
        return -1;
    }

    public long getAutoFocusTime() {
        return (mCurrentModule instanceof PhotoModule) ?
                ((PhotoModule) mCurrentModule).mAutoFocusTime : -1;
    }

    public long getShutterLag() {
        return (mCurrentModule instanceof PhotoModule) ?
                ((PhotoModule) mCurrentModule).mShutterLag : -1;
    }

    public long getShutterToPictureDisplayedTime() {
        return (mCurrentModule instanceof PhotoModule) ?
                ((PhotoModule) mCurrentModule).mShutterToPictureDisplayedTime : -1;
    }

    public long getPictureDisplayedToJpegCallbackTime() {
        return (mCurrentModule instanceof PhotoModule) ?
                ((PhotoModule) mCurrentModule).mPictureDisplayedToJpegCallbackTime : -1;
    }

    public long getJpegCallbackFinishTime() {
        return (mCurrentModule instanceof PhotoModule) ?
                ((PhotoModule) mCurrentModule).mJpegCallbackFinishTime : -1;
    }

    public long getCaptureStartTime() {
        return (mCurrentModule instanceof PhotoModule) ?
                ((PhotoModule) mCurrentModule).mCaptureStartTime : -1;
    }

    public boolean isRecording() {
        return (mCurrentModule instanceof VideoModule) ?
                ((VideoModule) mCurrentModule).isRecording() : false;
    }

    public CameraAgent.CameraOpenCallback getCameraOpenErrorCallback() {
        return mCameraController;
    }

    // For debugging purposes only.
    public CameraModule getCurrentModule() {
        return mCurrentModule;
    }

    @Override
    public void showTutorial(AbstractTutorialOverlay tutorial) {
        mCameraAppUI.showTutorial(tutorial, getLayoutInflater());
    }

    @Override
    public void finishActivityWithIntentCompleted(Intent resultIntent) {
        finishActivityWithIntentResult(Activity.RESULT_OK, resultIntent);
    }

    @Override
    public void finishActivityWithIntentCanceled() {
        finishActivityWithIntentResult(Activity.RESULT_CANCELED, new Intent());
    }

    public void syncCameraSoundSetting(CameraAgent.CameraProxy camera) {
        boolean soundOn = mSettingsManager.getBoolean(SettingsManager.SCOPE_GLOBAL,
                Keys.KEY_CAMERA_SOUND);
        Log.i(TAG,"camera sound = " + soundOn);
        if (camera != null)
            camera.enableShutterSound(soundOn);
    }

    private void finishActivityWithIntentResult(int resultCode, Intent resultIntent) {
        setResultEx(resultCode, resultIntent);
        finish();
    }

    private void keepScreenOnForAWhile() {
        if (mKeepScreenOn) {
            return;
        }
        mMainHandler.removeMessages(MSG_CLEAR_SCREEN_ON_FLAG);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        mMainHandler.sendEmptyMessageDelayed(MSG_CLEAR_SCREEN_ON_FLAG, SCREEN_DELAY_MS);
    }

    private void resetScreenOn() {
        mKeepScreenOn = false;
        mMainHandler.removeMessages(MSG_CLEAR_SCREEN_ON_FLAG);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    /**
     * @return {@code true} if the Gallery is launched successfully.
     */
    private boolean startGallery() {
        if (mGalleryIntent == null) {
            return false;
        }
        try {
            UsageStatistics.instance().changeScreen(NavigationChange.Mode.GALLERY,
                    NavigationChange.InteractionCause.BUTTON);
            Intent startGalleryIntent = new Intent(mGalleryIntent);
            int currentIndex = mFilmstripController.getCurrentAdapterIndex();
            FilmstripItem currentFilmstripItem = mDataAdapter.getItemAt(currentIndex);
            if (currentFilmstripItem != null) {
                GalleryHelper.setContentUri(startGalleryIntent,
                      currentFilmstripItem.getData().getUri());
            }
            launchActivityByIntent(startGalleryIntent);
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Failed to launch gallery activity, closing");
        }
        return false;
    }

    /**
     * Updates the visibility of the filmstrip bottom controls and action bar.
     */
    private void updateUiByData(final int index) {
        final FilmstripItem currentData = mDataAdapter.getItemAt(index);
        if (currentData == null) {
            Log.w(TAG, "Current data ID not found.");
            hideSessionProgress();
            return;
        }
        updateActionBarMenu(currentData);

        /* Bottom controls. */
        updateBottomControlsByData(currentData);

        if (isSecureCamera()) {
            // We cannot show buttons in secure camera since go to other
            // activities might create a security hole.
            mCameraAppUI.getFilmstripBottomControls().hideControls();
            return;
        }

        if (!mDataAdapter.isMetadataUpdatedAt(index)) {
            mDataAdapter.updateMetadataAt(index);
        }
    }

    /**
     * Updates the bottom controls based on the data.
     */
    private void updateBottomControlsByData(final FilmstripItem currentData) {

        final CameraAppUI.BottomPanel filmstripBottomPanel =
                mCameraAppUI.getFilmstripBottomControls();
        filmstripBottomPanel.showControls();
        filmstripBottomPanel.setEditButtonVisibility(
                currentData.getAttributes().canEdit());
        filmstripBottomPanel.setShareButtonVisibility(
              currentData.getAttributes().canShare());
        filmstripBottomPanel.setDeleteButtonVisibility(
                currentData.getAttributes().canDelete());

        /* Progress bar */

        Uri contentUri = currentData.getData().getUri();
        CaptureSessionManager sessionManager = getServices()
                .getCaptureSessionManager();

        if (sessionManager.hasErrorMessage(contentUri)) {
            showProcessError(sessionManager.getErrorMessageId(contentUri));
        } else {
            filmstripBottomPanel.hideProgressError();
            CaptureSession session = sessionManager.getSession(contentUri);

            if (session != null) {
                int sessionProgress = session.getProgress();

                if (sessionProgress < 0) {
                    hideSessionProgress();
                } else {
                    int progressMessageId = session.getProgressMessageId();
                    showSessionProgress(progressMessageId);
                    updateSessionProgress(sessionProgress);
                }
            } else {
                hideSessionProgress();
            }
        }

        /* View button */

        // We need to add this to a separate DB.
        final int viewButtonVisibility;
        if (currentData.getMetadata().isUsePanoramaViewer()) {
            viewButtonVisibility = CameraAppUI.BottomPanel.VIEWER_PHOTO_SPHERE;
        } else if (currentData.getMetadata().isHasRgbzData()) {
            viewButtonVisibility = CameraAppUI.BottomPanel.VIEWER_REFOCUS;
        } else {
            viewButtonVisibility = CameraAppUI.BottomPanel.VIEWER_NONE;
        }

        filmstripBottomPanel.setTinyPlanetEnabled(
                currentData.getMetadata().isPanorama360());
        filmstripBottomPanel.setViewerButtonVisibility(viewButtonVisibility);
    }

    private Dialog mDetailDialog = null;
    private void showDetailsDialog(int index) {
        final FilmstripItem data = mDataAdapter.getItemAt(index);
        if (data == null) {
            return;
        }
        Optional<MediaDetails> details = data.getMediaDetails();
        if (!details.isPresent()) {
            return;
        }
        if (mDetailDialog != null && mDetailDialog.isShowing()) {
            mDetailDialog.dismiss();
            mDetailDialog = null;
        }
        mDetailDialog = DetailsDialog.create(CameraActivity.this, details.get());
        mDetailDialog.show();
        if (mDetailDialog.isShowing())
        UsageStatistics.instance().mediaInteraction(
                fileNameFromAdapterAtIndex(index), MediaInteraction.InteractionType.DETAILS,
                NavigationChange.InteractionCause.BUTTON, fileAgeFromAdapterAtIndex(index));
    }

    /**
     * Show or hide action bar items depending on current data type.
     */
    private void updateActionBarMenu(FilmstripItem data) {
        if (mActionBarMenu == null) {
            return;
        }

        MenuItem detailsMenuItem = mActionBarMenu.findItem(R.id.action_details);
        if (detailsMenuItem == null) {
            return;
        }

        boolean showDetails = data.getAttributes().hasDetailedCaptureInfo();
        detailsMenuItem.setVisible(showDetails);

        FilmstripItemData itemdata = data.getData();
        if (itemdata != null) {
            Location location = itemdata.getLocation();
            String type = itemdata.getMimeType();
            MenuItem showOnMapMenuItem = mActionBarMenu.findItem(R.id.action_show_on_map);
            if (showOnMapMenuItem == null) {
                return;
            }
            boolean itemHasLocation = (location != null && location.isValid());
            boolean showOnMap = FilmstripItemData.MIME_TYPE_JPEG.equals(type);
            showOnMapMenuItem.setVisible(showOnMap && itemHasLocation);
        }
    }

    private void updateActivityOrientation() {
        if (CameraUtil.AUTO_ROTATE_SENSOR) {
            // TODO: Handle this in OrientationManager.
            // Auto-rotate off
            if (Settings.System.getInt(getContentResolver(),
                    Settings.System.ACCELEROMETER_ROTATION, 0) == 0) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
                mAutoRotateScreen = false;
            } else {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR);
                mAutoRotateScreen = true;
            }
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            mAutoRotateScreen = false;
        }
    }

    private void initExposure() {
        String[] cameraId = getResources().getStringArray(R.array.camera_id_entryvalues);
        if (cameraId != null) {
            for (int i = 0; i < cameraId.length; i++) {
                if (Integer.valueOf(cameraId[i]) < 0) {
                    // if an unopen camera i.e. negative ID is returned, which we've observed in
                    // some automated scenarios, just return it as a valid separate scope
                    // this could cause user issues, so log a stack trace noting the call path
                    // which resulted in this scenario.
                    continue;
                }
                String cameraScope = SettingsManager.CAMERA_SCOPE_PREFIX + cameraId[i];
                if (mSettingsManager.isSet(cameraScope, Keys.KEY_EXPOSURE)) {
                    mSettingsManager.set(cameraScope, Keys.KEY_EXPOSURE, 0);
                }
            }
        }
    }

    public void onNonDecorWindowSizeChanged() {
        if (mCurrentModule != null)
            mCurrentModule.onNonDecorWindowSizeChanged();
    }

    private void registerMediaMountListener() {
        IntentFilter filter = new IntentFilter();
        filter.addAction(Intent.ACTION_MEDIA_REMOVED);
        filter.addAction(Intent.ACTION_MEDIA_EJECT);
        filter.addAction(Intent.ACTION_MEDIA_MOUNTED);
        filter.addAction(Intent.ACTION_MEDIA_UNMOUNTED);
        filter.addDataScheme("file");
        registerReceiver(mMediaStateChangedReceiver, filter);
        updateMediaSavePath();
    }

    private void unregisterMediaMountListener() {
        try {
            unregisterReceiver(mMediaStateChangedReceiver);
        } catch (Exception e) {
            Log.e(TAG, "unregisterReceiver MediaStateChangedReceiver error:" + e);
        }
    }

    private BroadcastReceiver mMediaStateChangedReceiver = new BroadcastReceiver(){
        public void onReceive(Context context, Intent intent) {
            /*String action = intent.getAction();
            if(action.equals(Intent.ACTION_MEDIA_UNMOUNTED) && Storage.EXTENAL_SD.equals(intent.getData().getPath())){
                Toast.makeText(context, R.string.external_sd_pull_out, Toast.LENGTH_SHORT).show();
                finish();
            }else{
                String mountPath = intent.getData().getPath();
                Log.i(TAG, "receiver broadcast action = " + action
                        + ", data = " + mountPath);
                updateExternalSD(CameraActivity.this);
                updateMediaSavePath();
                updateStorageSpaceAndHint(null);
               if (mountPath != null && (mountPath.equals(Storage.EXTENAL_SD)
                        || mountPath.equals("/sdcard")
                        || mountPath.equals(Environment.getExternalStorageDirectory().getPath())))
                    mIsNeedReLoadImage = true;
            }*/
        }
    };

    private void updateMediaSavePath() {
        /*final String SDCARD = getResources().getString(R.string.pref_media_save_path_external_sd);
        String value = mSettingsManager.getString(SettingsManager.SCOPE_GLOBAL, Keys.KEY_MEDIA_SAVE_PATH);
        Log.i(TAG, "get mediapath value = " + value);
        if (SDCARD.equals(value)) {
            String state = Environment.getStorageState(new File(Storage.EXTENAL_SD));
            Log.i(TAG,"getSecondVolumeStorageState = " + state);
            if (!Environment.MEDIA_MOUNTED.equalsIgnoreCase(state)) {
                Storage.DIRECTORY = Storage.DEFAULT_DIRECTORY;
                mSettingsManager.setToDefault(SettingsManager.SCOPE_GLOBAL, Keys.KEY_MEDIA_SAVE_PATH);
            }
        }*/
    }

    private void updateExternalSD(Context context) {
        StorageManager sm = (StorageManager) getSystemService(Context.STORAGE_SERVICE);
        if (sm != null) {
            final List<VolumeInfo> volumes = sm.getVolumes();
            Collections.sort(volumes, VolumeInfo.getDescriptionComparator());
            for (VolumeInfo vol : volumes) {
                if (vol.getType() == VolumeInfo.TYPE_PUBLIC) {
                    Log.d(TAG, "Volume path for user:" + vol.getInternalPathForUser(context.getUserId()));
                    DiskInfo disk = vol.getDisk();
                    if(disk != null) {
                        if(disk.isSd()) {
                            //sdcard dir
                            StorageVolume sv = vol.buildStorageVolume(context, context.getUserId(), false);
                            String sdcard_dir = sv.getPath();
                            Log.d(TAG, "external sd = " + sdcard_dir);
                            /*Storage.EXTENAL_SD = sdcard_dir;
                            Storage.EXTERNAL_DIRECTORY = Storage.EXTENAL_SD
                                    + "/" + Environment.DIRECTORY_DCIM
                                    + "/Camera";*/
                            break;
                        }
                    }
                }
            }
        }
    }
}
