/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.ext.services.storage;

import android.app.usage.CacheQuotaHint;
import android.app.usage.CacheQuotaService;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.text.TextUtils;
import android.util.ArrayMap;

import androidx.core.util.Preconditions;

import com.android.modules.utils.build.SdkLevel;

import java.io.File;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

/**
 * CacheQuotaServiceImpl implements the CacheQuotaService with a strategy for populating the quota
 * of {@link CacheQuotaHint}.
 */
public class CacheQuotaServiceImpl extends CacheQuotaService {

    @Override
    public List<CacheQuotaHint> onComputeCacheQuotaHints(List<CacheQuotaHint> requests) {
        ArrayMap<String, List<CacheQuotaHintExtend>> byUuid = new ArrayMap<>();
        final int requestCount = requests.size();
        for (int i = 0; i < requestCount; i++) {
            CacheQuotaHint request = requests.get(i);
            String uuid = request.getVolumeUuid();
            List<CacheQuotaHintExtend> listForUuid = byUuid.get(uuid);
            if (listForUuid == null) {
                listForUuid = new ArrayList<>();
                byUuid.put(uuid, listForUuid);
            }
            listForUuid.add(convertToCacheQuotaHintExtend(request));
        }

        List<CacheQuotaHint> processed = new ArrayList<>();
        byUuid.forEach((uuid, uuidGroupedList) -> {
            // Collapse all usage stats to the same uid.
            Map<Integer, List<CacheQuotaHintExtend>> byUid = uuidGroupedList
                    .stream()
                    .collect(Collectors.groupingBy(CacheQuotaHintExtend::getUid));
            byUid.values().forEach(uidGroupedList -> {
                int size = uidGroupedList.size();
                if (size < 2) {
                    return;
                }
                CacheQuotaHintExtend first = uidGroupedList.get(0);
                for (int i = 1; i < size; i++) {
                    /* Note: We can't use the UsageStats built-in addition function because
                             UIDs may span multiple packages and usage stats adding has
                             matching package names as a precondition. */
                    first.mTotalTimeInForeground +=
                            uidGroupedList.get(i).mTotalTimeInForeground;
                }
            });

            // Because the foreground stats have been added to the first element, we need
            // a list of only the first values (which contain the merged foreground time).
            List<CacheQuotaHintExtend> flattenedRequests =
                    byUid.values()
                            .stream()
                            .map(entryList -> entryList.get(0))
                            .filter(entry -> entry.mTotalTimeInForeground != 0)
                            .sorted(sCacheQuotaRequestComparator)
                            .collect(Collectors.toList());

            // Because the elements are sorted, we can use the index to also be the sorted
            // index for cache quota calculation.
            double sum = getSumOfFairShares(flattenedRequests.size());
            long reservedSize = getReservedCacheSize(uuid);
            final int flattenedRequestsSize = flattenedRequests.size();
            for (int count = 0; count < flattenedRequestsSize; count++) {
                double share = getFairShareForPosition(count) / sum;
                CacheQuotaHint entry = flattenedRequests.get(count).mCacheQuotaHint;
                CacheQuotaHint.Builder builder = new CacheQuotaHint.Builder(entry);
                builder.setQuota(Math.round(share * reservedSize));
                processed.add(builder.build());
            }

            // Calculate the median of quotas of uids with >0 foreground time
            int midPoint = flattenedRequests.size() / 2;
            double medianValue, share;
            long medianQuota;
            if (flattenedRequests.size() % 2 == 0) {
                medianValue = (getFairShareForPosition(midPoint - 1)
                        + getFairShareForPosition(midPoint)) / 2;
            } else {
                medianValue = getFairShareForPosition(midPoint);
            }
            share = medianValue / sum;
            medianQuota = Math.round(share * reservedSize);
            // Allot median quota to uids with foreground time =0
            List<CacheQuotaHintExtend> flattenedRequestsForegroundZero =
                    byUid.values()
                            .stream()
                            .map(entryList -> entryList.get(0))
                            .filter(entry -> entry.mTotalTimeInForeground == 0)
                            .sorted(sCacheQuotaRequestComparator)
                            .collect(Collectors.toList());
            final int flattenedRequestsForegroundZeroSize = flattenedRequestsForegroundZero.size();
            for (int count = 0; count < flattenedRequestsForegroundZeroSize; count++) {
                CacheQuotaHint entry = flattenedRequestsForegroundZero.get(count)
                        .mCacheQuotaHint;
                CacheQuotaHint.Builder builder = new CacheQuotaHint.Builder(entry);
                builder.setQuota(medianQuota);
                processed.add(builder.build());
            }
        });

        return processed.stream()
                .filter(request -> request.getQuota() > 0).collect(Collectors.toList());
    }

    private double getFairShareForPosition(int position) {
        double value = 1.0 / Math.log(position + 3) - 0.285;
        return (value > 0.01) ? value : 0.01;
    }

    private double getSumOfFairShares(int size) {
        double sum = 0;
        for (int i = 0; i < size; i++) {
            sum += getFairShareForPosition(i);
        }
        return sum;
    }

    private long getReservedCacheSize(String uuid) {
        // TODO: Revisit the cache size after running more storage tests.
        // TODO: Figure out how to ensure ExtServices has the permissions to call
        //       StorageStatsManager, because this is ignoring the cache...
        final long cacheReservePercent = 15;
        final StorageManager storageManager = getSystemService(StorageManager.class);
        if (TextUtils.isEmpty(uuid)) { // regular equals because of null
            if (SdkLevel.isAtLeastT()) {
                return storageManager.computeStorageCacheBytes(Environment.getDataDirectory());
            } else {
                return Environment.getDataDirectory().getUsableSpace()
                        * cacheReservePercent / 100;
            }
        } else {
            final List<StorageVolume> storageVolumes = storageManager.getStorageVolumes();
            final int volumeCount = storageVolumes.size();
            for (int i = 0; i < volumeCount; i++) {
                final StorageVolume volume = storageVolumes.get(i);
                if (TextUtils.equals(volume.getUuid(), uuid)) {
                    final File directory = volume.getDirectory();
                    if (SdkLevel.isAtLeastT()) {
                        return storageManager.computeStorageCacheBytes(directory);
                    } else {
                        return ((directory != null) ? directory.getUsableSpace() : 0)
                                * cacheReservePercent / 100;
                    }
                }
            }
        }
        return 0;
    }

    // Compares based upon foreground time.
    private static Comparator<CacheQuotaHintExtend> sCacheQuotaRequestComparator =
            new Comparator<CacheQuotaHintExtend>() {
        @Override
        public int compare(CacheQuotaHintExtend o, CacheQuotaHintExtend t1) {
            return (t1.mTotalTimeInForeground < o.mTotalTimeInForeground) ?
                    -1 : ((t1.mTotalTimeInForeground == o.mTotalTimeInForeground) ? 0 : 1);
        }
    };

    private CacheQuotaHintExtend convertToCacheQuotaHintExtend(CacheQuotaHint cacheQuotaHint) {
        Preconditions.checkNotNull(cacheQuotaHint);
        return new CacheQuotaHintExtend(cacheQuotaHint);
    }

    private final class CacheQuotaHintExtend {
        public final CacheQuotaHint mCacheQuotaHint;
        public long mTotalTimeInForeground;

        public CacheQuotaHintExtend (CacheQuotaHint cacheQuotaHint) {
            mCacheQuotaHint = cacheQuotaHint;
            mTotalTimeInForeground = (cacheQuotaHint.getUsageStats() != null) ?
                    cacheQuotaHint.getUsageStats().getTotalTimeInForeground() : 0;
        }

        public int getUid() {
            return mCacheQuotaHint.getUid();
        }
    }
}
