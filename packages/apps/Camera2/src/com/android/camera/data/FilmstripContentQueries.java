/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.camera.data;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;

import com.android.camera.Storage;
import com.android.camera.debug.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * A set of queries for loading data from a content resolver.
 */
public class FilmstripContentQueries {
    private static final Log.Tag TAG = new Log.Tag("LocalDataQuery");
    private static final String PHOTO_PATH = "%" + Environment.DIRECTORY_PICTURES + "%";
    private static final String VIDEO_PATH = "%" + Environment.DIRECTORY_MOVIES + "%";
    private static final String SELECT_BY_PATH = MediaStore.MediaColumns.DATA + " LIKE ?";
    private static final String CAMERA2_PACKAGENAME = "com.android.camera2";
    private static final String OWNER_PACKAGE_NAME = "owner_package_name";
    private static final boolean isDebugOn = false;
    private static int mAllImagesCount = 0;
    private static int mLoadNewImagesCount = 0;
    private static int mVideosCount = 0;
    private static List<PhotoItem> mAllImagesResult = new ArrayList<>();
    private static List<PhotoItem> mLoadNewImagesResult = new ArrayList<>();
    private static List<VideoItem> mVideosResult = new ArrayList<>();

    public interface CursorToFilmstripItemFactory<I extends FilmstripItem> {

        /**
         * Convert a cursor at a given location to a Local Data object.
         *
         * @param cursor the current cursor state.
         * @return a LocalData object that represents the current cursor state.
         */
        public I get(Cursor cursor);
    }

    public static List<PhotoItem> forAllCameraPathPhoto(ContentResolver contentResolver,
          Uri contentUri, String[] projection, long minimumId, String orderBy,
          CursorToFilmstripItemFactory<PhotoItem> factory) {
        if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto contentUri:" + contentUri + ",minimumId:" + minimumId);
        String selection = SELECT_BY_PATH + " AND " + MediaStore.MediaColumns._ID + " > ?" +
                " AND " + OWNER_PACKAGE_NAME + " = ?";
        String[] selectionArgs = new String[] { PHOTO_PATH, Long.toString(minimumId), CAMERA2_PACKAGENAME };

        Cursor cursor = contentResolver.query(contentUri, projection,
              selection, selectionArgs, orderBy);

        if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto getCount:" + cursor.getCount() + ",mAllImagesCount:" + mAllImagesCount);
        if (cursor.getCount() != mAllImagesCount) {
            if (cursor != null) {
                if (mAllImagesCount ==0) {
                    while (cursor.moveToNext()) {
                        int size = cursor.getInt(PhotoDataQuery.COL_SIZE);
                        if (isDebugOn)Log.e(TAG, "---zc forAllCameraPathPhoto size1:" + size);
                        if (size > 0) {
                            PhotoItem item = factory.get(cursor);
                            if (item != null) {
                                if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto item:" + item.toString());
                                mAllImagesResult.add(item);
                            } else {
                                final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                            }
                        }
                    }
                } else {
                    if (cursor.getCount() != 0) {
                        if (cursor.getCount() > mAllImagesCount) {
                            cursor.moveToFirst();
                            int size = cursor.getInt(PhotoDataQuery.COL_SIZE);
                            if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto size2:" + size);
                            if (size > 0) {
                                PhotoItem item = factory.get(cursor);
                                if (item != null) {
                                    if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto item:" + item.toString());
                                    mAllImagesResult.add(item);
                                } else {
                                    final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                    Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                                }
                            }
                        } else {
                            mAllImagesResult.clear();
                            while (cursor.moveToNext()) {
                                int size = cursor.getInt(PhotoDataQuery.COL_SIZE);
                                if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto size3:" + size);
                                if (size > 0) {
                                    PhotoItem item = factory.get(cursor);
                                    if (item != null) {
                                        if (isDebugOn) Log.e(TAG, "---zc forAllCameraPathPhoto item:" + item.toString());
                                        mAllImagesResult.add(item);
                                    } else {
                                        final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                        Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                                    }
                                }
                            }
                        }
                    } else {
                        mAllImagesResult = new ArrayList<>();
                    }
                }
                mAllImagesCount = mAllImagesResult.size();
                cursor.close();
            } else {
                Log.e(TAG, "images cursor == null");
            }
        } else {
            if (isDebugOn) Log.e(TAG, "---zc cursor.getCount() == mAllImagesCount");
        }
        return mAllImagesResult;
    }

    public static List<PhotoItem> forLoadNewCameraPathPhoto(ContentResolver contentResolver,
          Uri contentUri, String[] projection, long minimumId, String orderBy,
          CursorToFilmstripItemFactory<PhotoItem> factory) {
        if (isDebugOn) Log.e(TAG, "---zc forLoadNewCameraPathPhoto contentUri:" + contentUri + ",minimumId:" + minimumId);
        String selection = SELECT_BY_PATH + " AND " + MediaStore.MediaColumns._ID + " > ?" +
                " AND " + OWNER_PACKAGE_NAME + " = ?";
        String[] selectionArgs = new String[] { PHOTO_PATH, Long.toString(minimumId), CAMERA2_PACKAGENAME };

        Cursor cursor = contentResolver.query(contentUri, projection,
              selection, selectionArgs, orderBy);

        if (isDebugOn) Log.e(TAG, "---zc forLoadNewCameraPathPhoto getCount:" + cursor.getCount() + ",mLoadNewImagesCount:" + mLoadNewImagesCount);
        if (cursor.getCount() != mLoadNewImagesCount) {
            if (cursor != null) {
                if (mLoadNewImagesCount ==0) {
                    while (cursor.moveToNext()) {
                        PhotoItem item = factory.get(cursor);
                        if (item != null) {
                            if (isDebugOn) Log.e(TAG, "---zc forLoadNewCameraPathPhoto item:" + item.toString());
                            mLoadNewImagesResult.add(item);
                        } else {
                            final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                            Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                        }
                    }
                } else {
                    if (cursor.getCount() != 0) {
                        if (cursor.getCount() > mLoadNewImagesCount) {
                            cursor.moveToFirst();
                            PhotoItem item = factory.get(cursor);
                            if (item != null) {
                                if (isDebugOn) Log.e(TAG, "---zc forLoadNewCameraPathPhoto item:" + item.toString());
                                mLoadNewImagesResult.add(item);
                            } else {
                                final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                            }
                        } else {
                            mLoadNewImagesResult.clear();
                            while (cursor.moveToNext()) {
                                PhotoItem item = factory.get(cursor);
                                if (item != null) {
                                    if (isDebugOn) Log.e(TAG, "---zc forLoadNewCameraPathPhoto item:" + item.toString());
                                    mLoadNewImagesResult.add(item);
                                } else {
                                    final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                    Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                                }
                            }
                        }
                    } else {
                        mLoadNewImagesResult = new ArrayList<>();
                    }
                }
                mLoadNewImagesCount = mLoadNewImagesResult.size();
                cursor.close();
            } else {
                Log.e(TAG, "images cursor == null");
            }
        } else {
            if (isDebugOn) Log.e(TAG, "---zc cursor.getCount() == mLoadNewImagesCount");
        }
        return mLoadNewImagesResult;
    }

    public static List<VideoItem> forCameraPathVideo(ContentResolver contentResolver,
          Uri contentUri, String[] projection, long minimumId, String orderBy,
          CursorToFilmstripItemFactory<VideoItem> factory) {
        if (isDebugOn) Log.e(TAG, "---zc forCameraPathVideo contentUri:" + contentUri + ",minimumId:" + minimumId);
        String selection = SELECT_BY_PATH + " AND " + MediaStore.MediaColumns._ID + " > ?" +
                " AND " + OWNER_PACKAGE_NAME + " = ?";
        String[] selectionArgs = new String[] { VIDEO_PATH, Long.toString(minimumId), CAMERA2_PACKAGENAME };

        Cursor cursor = contentResolver.query(contentUri, projection,
              selection, selectionArgs, orderBy);

        if (isDebugOn) Log.e(TAG, "---zc forCameraPathVideo getCount:" + cursor.getCount() + ",mVideosCount:" + mVideosCount);
        if (cursor.getCount() != mVideosCount) {
            if (cursor != null) {
                if (mVideosCount ==0) {
                    while (cursor.moveToNext()) {
                        VideoItem item = factory.get(cursor);
                        if (item != null) {
                            if (isDebugOn) Log.e(TAG, "---zc forCameraPathVideo item:" + item.toString());
                            mVideosResult.add(item);
                        } else {
                            final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                            Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                        }
                    }
                } else {
                    if (cursor.getCount() != 0) {
                        if (cursor.getCount() > mVideosCount) {
                            cursor.moveToFirst();
                            VideoItem item = factory.get(cursor);
                            if (item != null) {
                                if (isDebugOn) Log.e(TAG, "---zc forCameraPathVideo item:" + item.toString());
                                mVideosResult.add(item);
                            } else {
                                final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                            }
                        } else {
                            mVideosResult.clear();
                            while (cursor.moveToNext()) {
                                VideoItem item = factory.get(cursor);
                                if (item != null) {
                                    if (isDebugOn) Log.e(TAG, "---zc forCameraPathVideo item:" + item.toString());
                                    mVideosResult.add(item);
                                } else {
                                    final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                                    Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                                }
                            }
                        }
                    } else {
                        mVideosResult = new ArrayList<>();
                    }
                }
                mVideosCount = mVideosResult.size();
                cursor.close();
            } else {
                Log.e(TAG, "video cursor == null");
            }
        } else {
            if (isDebugOn) Log.e(TAG, "---zc cursor.getCount() == mVideosCount");
        }
        return mVideosResult;
    }

    /**
     * Query the camera storage directory and convert it to local data
     * objects.
     *
     * @param contentResolver to resolve content with.
     * @param contentUri to resolve an item at
     * @param projection the columns to extract
     * @param minimumId the lower bound of results
     * @param orderBy the order by clause
     * @param factory an object that can turn a given cursor into a LocalData object.
     * @return A list of LocalData objects that satisfy the query.
     */
    public static <I extends FilmstripItem> List<I> forCameraPath(ContentResolver contentResolver,
          Uri contentUri, String[] projection, long minimumId, String orderBy,
          CursorToFilmstripItemFactory<I> factory) {
        if (isDebugOn) Log.e(TAG, "---zc forCameraPath  contentUri:" + contentUri + ",minimumId:" + minimumId);
        String selection = SELECT_BY_PATH + " AND " + MediaStore.MediaColumns._ID + " > ?";
        String[] selectionArgs = new String[] { PhotoDataQuery.CONTENT_URI.equals(contentUri) ? PHOTO_PATH : VIDEO_PATH, Long.toString(minimumId) };

        Cursor cursor = contentResolver.query(contentUri, projection,
              selection, selectionArgs, orderBy);
        List<I> result = new ArrayList<>();
        if (isDebugOn) Log.e(TAG, "---zc forCameraPath getCount:" + cursor.getCount());
        if (cursor != null) {
            while (cursor.moveToNext()) {
                int owner_package_name_index = cursor.getColumnIndex("owner_package_name");
                if (owner_package_name_index > -1) {
                    String packageName = cursor.getString(owner_package_name_index);
                    if (packageName != null && packageName.equals(CAMERA2_PACKAGENAME)) {
                        I item = factory.get(cursor);
                        if (item != null) {
                            if (isDebugOn) Log.e(TAG, "---zc forCameraPath item:" + item.toString());
                            result.add(item);
                        } else {
                            final int dataIndex = cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA);
                            Log.e(TAG, "Error loading data:" + cursor.getString(dataIndex));
                        }
                    }
                }
            }
            cursor.close();
        }
        return result;
    }

    public static void deleteForAllPhoto(String title) {
        if (isDebugOn) Log.e(TAG, "---zc deleteForAll mAllImagesCount1:" + mAllImagesCount);
        mAllImagesResult.removeIf(e -> (e.getData().getTitle().equals(title)));
        mAllImagesCount = mAllImagesResult.size();
        if (isDebugOn) Log.e(TAG, "---zc deleteForAll mAllImagesCount2:" + mAllImagesCount);
    }

    public static void deleteForAllVideo(String title) {
        if (isDebugOn) Log.e(TAG, "---zc deleteForAll mVideosCount1:" + mVideosCount);
        mVideosResult.removeIf(e -> (e.getData().getTitle().equals(title)));
        mVideosCount = mVideosResult.size();
        if (isDebugOn) Log.e(TAG, "---zc deleteForAll mVideosCount2:" + mVideosCount);
    }
}
