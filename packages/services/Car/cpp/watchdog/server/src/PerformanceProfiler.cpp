/**
 * Copyright (c) 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "carwatchdogd"

#include "PerformanceProfiler.h"

#include <WatchdogProperties.sysprop.h>
#include <android-base/file.h>
#include <android-base/stringprintf.h>
#include <log/log.h>

#include <inttypes.h>

#include <iomanip>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace android {
namespace automotive {
namespace watchdog {

using ::aidl::android::automotive::watchdog::internal::ResourceStats;
using ::android::wp;
using ::android::base::Error;
using ::android::base::Result;
using ::android::base::StringAppendF;
using ::android::base::StringPrintf;
using ::android::base::WriteStringToFd;

namespace {

constexpr int32_t kDefaultTopNStatsPerCategory = 10;
constexpr int32_t kDefaultTopNStatsPerSubcategory = 5;
constexpr int32_t kDefaultMaxUserSwitchEvents = 5;
constexpr std::chrono::seconds kSystemEventDataCacheDurationSec = 1h;
constexpr const char kBootTimeCollectionTitle[] = "%s\nBoot-time performance report:\n%s\n";
constexpr const char kPeriodicCollectionTitle[] = "%s\nLast N minutes performance report:\n%s\n";
constexpr const char kUserSwitchCollectionTitle[] =
        "%s\nUser-switch events performance report:\n%s\n";
constexpr const char kUserSwitchCollectionSubtitle[] = "Number of user switch events: %zu\n";
constexpr const char kWakeUpCollectionTitle[] = "%s\nWake-up performance report:\n%s\n";
constexpr const char kCustomCollectionTitle[] = "%s\nCustom performance data report:\n%s\n";
constexpr const char kUserSwitchEventTitle[] = "\nEvent %zu: From: %d To: %d\n%s\n";
constexpr const char kCollectionTitle[] =
        "Collection duration: %.f seconds\nNumber of collections: %zu\n";
constexpr const char kRecordTitle[] = "\nCollection %zu: <%s>\n%s\n%s";
constexpr const char kCpuTimeTitle[] = "\nTop N CPU Times:\n%s\n";
constexpr const char kCpuTimeHeader[] = "Android User ID, Package Name, CPU Time (ms), Percentage "
                                        "of total CPU time, CPU Cycles\n\tCommand, CPU Time (ms), "
                                        "Percentage of UID's CPU Time, CPU Cycles\n";
constexpr const char kIoReadsTitle[] = "\nTop N Storage I/O Reads:\n%s\n";
constexpr const char kIoWritesTitle[] = "\nTop N Storage I/O Writes:\n%s\n";
constexpr const char kIoStatsHeader[] =
        "Android User ID, Package Name, Foreground Bytes, Foreground Bytes %%, Foreground Fsync, "
        "Foreground Fsync %%, Background Bytes, Background Bytes %%, Background Fsync, "
        "Background Fsync %%\n";
constexpr const char kIoBlockedTitle[] = "\nTop N I/O waiting UIDs:\n%s\n";
constexpr const char kIoBlockedHeader[] =
        "Android User ID, Package Name, Number of owned tasks waiting for I/O, Percentage of owned "
        "tasks waiting for I/O\n\tCommand, Number of I/O waiting tasks, Percentage of UID's tasks "
        "waiting for I/O\n";
constexpr const char kMajorPageFaultsTitle[] = "\nTop N major page faults:\n%s\n";
constexpr const char kMajorFaultsHeader[] =
        "Android User ID, Package Name, Number of major page faults, Percentage of total major "
        "page faults\n\tCommand, Number of major page faults, Percentage of UID's major page "
        "faults\n";
constexpr const char kMajorFaultsSummary[] =
        "Number of major page faults since last collection: %" PRIu64 "\n"
        "Percentage of change in major page faults since last collection: %.2f%%\n";

double percentage(uint64_t numer, uint64_t denom) {
    return denom == 0 ? 0.0 : (static_cast<double>(numer) / static_cast<double>(denom)) * 100.0;
}

void addUidIoStats(const int64_t entry[][UID_STATES], int64_t total[][UID_STATES]) {
    const auto sum = [](int64_t lhs, int64_t rhs) -> int64_t {
        return std::numeric_limits<int64_t>::max() - lhs > rhs
                ? lhs + rhs
                : std::numeric_limits<int64_t>::max();
    };
    total[READ_BYTES][FOREGROUND] =
            sum(total[READ_BYTES][FOREGROUND], entry[READ_BYTES][FOREGROUND]);
    total[READ_BYTES][BACKGROUND] =
            sum(total[READ_BYTES][BACKGROUND], entry[READ_BYTES][BACKGROUND]);
    total[WRITE_BYTES][FOREGROUND] =
            sum(total[WRITE_BYTES][FOREGROUND], entry[WRITE_BYTES][FOREGROUND]);
    total[WRITE_BYTES][BACKGROUND] =
            sum(total[WRITE_BYTES][BACKGROUND], entry[WRITE_BYTES][BACKGROUND]);
    total[FSYNC_COUNT][FOREGROUND] =
            sum(total[FSYNC_COUNT][FOREGROUND], entry[FSYNC_COUNT][FOREGROUND]);
    total[FSYNC_COUNT][BACKGROUND] =
            sum(total[FSYNC_COUNT][BACKGROUND], entry[FSYNC_COUNT][BACKGROUND]);
    return;
}

bool cacheTopNStats(const UserPackageStats& curUserPackageStats,
                    std::vector<UserPackageStats>* topNStats) {
    uint64_t curValue = curUserPackageStats.getValue();
    if (curValue == 0) {
        return false;
    }
    for (auto it = topNStats->begin(); it != topNStats->end(); ++it) {
        if (curValue > it->getValue()) {
            topNStats->insert(it, curUserPackageStats);
            topNStats->pop_back();
            return true;
        }
    }
    return false;
}

Result<void> checkDataCollectors(const sp<UidStatsCollectorInterface>& uidStatsCollector,
                                 const sp<ProcStatCollectorInterface>& procStatCollector) {
    if (uidStatsCollector != nullptr && procStatCollector != nullptr) {
        return {};
    }
    std::string error;
    if (uidStatsCollector == nullptr) {
        error = "Per-UID stats collector must not be null";
    }
    if (procStatCollector == nullptr) {
        StringAppendF(&error, "%s%s", error.empty() ? "" : ", ",
                      "Proc stats collector must not be null");
    }
    return Error() << "Invalid data collectors: " << error;
}

}  // namespace

UserPackageStats::UserPackageStats(MetricType metricType, const UidStats& uidStats) {
    const UidIoStats& ioStats = uidStats.ioStats;
    uid = uidStats.uid();
    genericPackageName = uidStats.genericPackageName();
    statsView = UserPackageStats::
            IoStatsView{.bytes = {ioStats.metrics[metricType][UidState::FOREGROUND],
                                  ioStats.metrics[metricType][UidState::BACKGROUND]},
                        .fsync = {ioStats.metrics[MetricType::FSYNC_COUNT][UidState::FOREGROUND],
                                  ioStats.metrics[MetricType::FSYNC_COUNT][UidState::BACKGROUND]}};
}

UserPackageStats::UserPackageStats(ProcStatType procStatType, const UidStats& uidStats,
                                   int topNProcessCount) {
    uint64_t value = procStatType == CPU_TIME        ? uidStats.cpuTimeMillis
            : procStatType == IO_BLOCKED_TASKS_COUNT ? uidStats.procStats.ioBlockedTasksCount
                                                     : uidStats.procStats.totalMajorFaults;
    uid = uidStats.uid();
    genericPackageName = uidStats.genericPackageName();
    if (procStatType == CPU_TIME) {
        statsView = UserPackageStats::ProcCpuStatsView{.cpuTime = value,
                                                       .cpuCycles = uidStats.procStats.cpuCycles};
        auto& procCpuStatsView = std::get<UserPackageStats::ProcCpuStatsView>(statsView);
        procCpuStatsView.topNProcesses.resize(topNProcessCount);
        cacheTopNProcessCpuStats(uidStats, topNProcessCount, &procCpuStatsView.topNProcesses);
        return;
    }
    statsView = UserPackageStats::ProcSingleStatsView{.value = value};
    auto& procStatsView = std::get<UserPackageStats::ProcSingleStatsView>(statsView);
    procStatsView.topNProcesses.resize(topNProcessCount);
    cacheTopNProcessSingleStats(procStatType, uidStats, topNProcessCount,
                                &procStatsView.topNProcesses);
}

uint64_t UserPackageStats::getValue() const {
    return std::visit(
            [](auto&& arg) -> uint64_t {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, UserPackageStats::IoStatsView>) {
                    return arg.totalBytes();
                }
                if constexpr (std::is_same_v<T, UserPackageStats::ProcSingleStatsView>) {
                    return arg.value;
                }
                if constexpr (std::is_same_v<T, UserPackageStats::ProcCpuStatsView>) {
                    return arg.cpuTime;
                }
                // Unknown stats view
                return 0;
            },
            statsView);
}

std::string UserPackageStats::toString(MetricType metricsType,
                                       const int64_t totalIoStats[][UID_STATES]) const {
    std::string buffer;
    StringAppendF(&buffer, "%" PRIu32 ", %s", multiuser_get_user_id(uid),
                  genericPackageName.c_str());
    const auto& ioStatsView = std::get<UserPackageStats::IoStatsView>(statsView);
    for (int i = 0; i < UID_STATES; ++i) {
        StringAppendF(&buffer, ", %" PRIi64 ", %.2f%%, %" PRIi64 ", %.2f%%", ioStatsView.bytes[i],
                      percentage(ioStatsView.bytes[i], totalIoStats[metricsType][i]),
                      ioStatsView.fsync[i],
                      percentage(ioStatsView.fsync[i], totalIoStats[FSYNC_COUNT][i]));
    }
    StringAppendF(&buffer, "\n");
    return buffer;
}

std::string UserPackageStats::toString(int64_t totalValue) const {
    std::string buffer;
    auto procCpuStatsView = std::get_if<UserPackageStats::ProcCpuStatsView>(&statsView);
    if (procCpuStatsView != nullptr) {
        StringAppendF(&buffer, "%" PRIu32 ", %s, %" PRIu64 ", %.2f%%, %" PRIu64 "\n",
                      multiuser_get_user_id(uid), genericPackageName.c_str(),
                      procCpuStatsView->cpuTime, percentage(procCpuStatsView->cpuTime, totalValue),
                      procCpuStatsView->cpuCycles);
        for (const auto& processCpuValue : procCpuStatsView->topNProcesses) {
            StringAppendF(&buffer, "\t%s, %" PRIu64 ", %.2f%%, %" PRIu64 "\n",
                          processCpuValue.comm.c_str(), processCpuValue.cpuTime,
                          percentage(processCpuValue.cpuTime, procCpuStatsView->cpuTime),
                          processCpuValue.cpuCycles);
        }
        return buffer;
    }
    const auto& procStatsView = std::get<UserPackageStats::ProcSingleStatsView>(statsView);
    StringAppendF(&buffer, "%" PRIu32 ", %s, %" PRIu64 ", %.2f%%\n", multiuser_get_user_id(uid),
                  genericPackageName.c_str(), procStatsView.value,
                  percentage(procStatsView.value, totalValue));
    for (const auto& processValue : procStatsView.topNProcesses) {
        StringAppendF(&buffer, "\t%s, %" PRIu64 ", %.2f%%\n", processValue.comm.c_str(),
                      processValue.value, percentage(processValue.value, procStatsView.value));
    }
    return buffer;
}

void UserPackageStats::cacheTopNProcessSingleStats(
        ProcStatType procStatType, const UidStats& uidStats, int topNProcessCount,
        std::vector<UserPackageStats::ProcSingleStatsView::ProcessValue>* topNProcesses) {
    int cachedProcessCount = 0;
    for (const auto& [_, processStats] : uidStats.procStats.processStatsByPid) {
        uint64_t value = procStatType == IO_BLOCKED_TASKS_COUNT ? processStats.ioBlockedTasksCount
                                                                : processStats.totalMajorFaults;
        if (value == 0) {
            continue;
        }
        for (auto it = topNProcesses->begin(); it != topNProcesses->end(); ++it) {
            if (value > it->value) {
                topNProcesses->insert(it,
                                      UserPackageStats::ProcSingleStatsView::ProcessValue{
                                              .comm = processStats.comm,
                                              .value = value,
                                      });
                topNProcesses->pop_back();
                ++cachedProcessCount;
                break;
            }
        }
    }
    if (cachedProcessCount < topNProcessCount) {
        topNProcesses->erase(topNProcesses->begin() + cachedProcessCount, topNProcesses->end());
    }
}

void UserPackageStats::cacheTopNProcessCpuStats(
        const UidStats& uidStats, int topNProcessCount,
        std::vector<UserPackageStats::ProcCpuStatsView::ProcessCpuValue>* topNProcesses) {
    int cachedProcessCount = 0;
    for (const auto& [_, processStats] : uidStats.procStats.processStatsByPid) {
        uint64_t cpuTime = processStats.cpuTimeMillis;
        if (cpuTime == 0) {
            continue;
        }
        for (auto it = topNProcesses->begin(); it != topNProcesses->end(); ++it) {
            if (cpuTime > it->cpuTime) {
                topNProcesses->insert(it,
                                      UserPackageStats::ProcCpuStatsView::ProcessCpuValue{
                                              .comm = processStats.comm,
                                              .cpuTime = cpuTime,
                                              .cpuCycles = processStats.totalCpuCycles,
                                      });
                topNProcesses->pop_back();
                ++cachedProcessCount;
                break;
            }
        }
    }
    if (cachedProcessCount < topNProcessCount) {
        topNProcesses->erase(topNProcesses->begin() + cachedProcessCount, topNProcesses->end());
    }
}

std::string UserPackageSummaryStats::toString() const {
    std::string buffer;
    if (!topNCpuTimes.empty()) {
        StringAppendF(&buffer, kCpuTimeTitle, std::string(16, '-').c_str());
        StringAppendF(&buffer, kCpuTimeHeader);
        for (const auto& stats : topNCpuTimes) {
            StringAppendF(&buffer, "%s", stats.toString(totalCpuTimeMillis).c_str());
        }
    }
    if (!topNIoReads.empty()) {
        StringAppendF(&buffer, kIoReadsTitle, std::string(24, '-').c_str());
        StringAppendF(&buffer, kIoStatsHeader);
        for (const auto& stats : topNIoReads) {
            StringAppendF(&buffer, "%s",
                          stats.toString(MetricType::READ_BYTES, totalIoStats).c_str());
        }
    }
    if (!topNIoWrites.empty()) {
        StringAppendF(&buffer, kIoWritesTitle, std::string(25, '-').c_str());
        StringAppendF(&buffer, kIoStatsHeader);
        for (const auto& stats : topNIoWrites) {
            StringAppendF(&buffer, "%s",
                          stats.toString(MetricType::WRITE_BYTES, totalIoStats).c_str());
        }
    }
    if (!topNIoBlocked.empty()) {
        StringAppendF(&buffer, kIoBlockedTitle, std::string(23, '-').c_str());
        StringAppendF(&buffer, kIoBlockedHeader);
        for (const auto& stats : topNIoBlocked) {
            const auto it = taskCountByUid.find(stats.uid);
            if (it == taskCountByUid.end()) {
                continue;
            }
            StringAppendF(&buffer, "%s", stats.toString(it->second).c_str());
        }
    }
    if (!topNMajorFaults.empty()) {
        StringAppendF(&buffer, kMajorPageFaultsTitle, std::string(24, '-').c_str());
        StringAppendF(&buffer, kMajorFaultsHeader);
        for (const auto& stats : topNMajorFaults) {
            StringAppendF(&buffer, "%s", stats.toString(totalMajorFaults).c_str());
        }
        StringAppendF(&buffer, kMajorFaultsSummary, totalMajorFaults, majorFaultsPercentChange);
    }
    return buffer;
}

std::string SystemSummaryStats::toString() const {
    std::string buffer;
    StringAppendF(&buffer, "Total CPU time (ms): %" PRIu64 "\n", totalCpuTimeMillis);
    StringAppendF(&buffer, "Total CPU cycles: %" PRIu64 "\n", totalCpuCycles);
    StringAppendF(&buffer, "Total idle CPU time (ms)/percent: %" PRIu64 " / %.2f%%\n",
                  cpuIdleTimeMillis, percentage(cpuIdleTimeMillis, totalCpuTimeMillis));
    StringAppendF(&buffer, "CPU I/O wait time (ms)/percent: %" PRIu64 " / %.2f%%\n",
                  cpuIoWaitTimeMillis, percentage(cpuIoWaitTimeMillis, totalCpuTimeMillis));
    StringAppendF(&buffer, "Number of context switches: %" PRIu64 "\n", contextSwitchesCount);
    StringAppendF(&buffer, "Number of I/O blocked processes/percent: %" PRIu32 " / %.2f%%\n",
                  ioBlockedProcessCount, percentage(ioBlockedProcessCount, totalProcessCount));
    return buffer;
}

std::string PerfStatsRecord::toString() const {
    std::string buffer;
    StringAppendF(&buffer, "%s%s", systemSummaryStats.toString().c_str(),
                  userPackageSummaryStats.toString().c_str());
    return buffer;
}

std::string CollectionInfo::toString() const {
    if (records.empty()) {
        return kEmptyCollectionMessage;
    }
    std::string buffer;
    double duration = difftime(records.back().time, records.front().time);
    StringAppendF(&buffer, kCollectionTitle, duration, records.size());
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];
        std::stringstream timestamp;
        timestamp << std::put_time(std::localtime(&record.time), "%c %Z");
        StringAppendF(&buffer, kRecordTitle, i, timestamp.str().c_str(),
                      std::string(45, '=').c_str(), record.toString().c_str());
    }
    return buffer;
}

Result<void> PerformanceProfiler::init() {
    Mutex::Autolock lock(mMutex);
    if (mTopNStatsPerCategory != 0 || mTopNStatsPerSubcategory != 0) {
        return Error() << "Cannot initialize " << name() << " more than once";
    }
    mTopNStatsPerCategory = static_cast<int>(
            sysprop::topNStatsPerCategory().value_or(kDefaultTopNStatsPerCategory));
    mTopNStatsPerSubcategory = static_cast<int>(
            sysprop::topNStatsPerSubcategory().value_or(kDefaultTopNStatsPerSubcategory));
    mMaxUserSwitchEvents = static_cast<size_t>(
            sysprop::maxUserSwitchEvents().value_or(kDefaultMaxUserSwitchEvents));
    mSystemEventDataCacheDurationSec =
            std::chrono::seconds(sysprop::systemEventDataCacheDuration().value_or(
                    kSystemEventDataCacheDurationSec.count()));
    size_t periodicCollectionBufferSize = static_cast<size_t>(
            sysprop::periodicCollectionBufferSize().value_or(kDefaultPeriodicCollectionBufferSize));
    mBoottimeCollection = {
            .maxCacheSize = std::numeric_limits<std::size_t>::max(),
            .records = {},
    };
    mPeriodicCollection = {
            .maxCacheSize = periodicCollectionBufferSize,
            .records = {},
    };
    mWakeUpCollection = {
            .maxCacheSize = std::numeric_limits<std::size_t>::max(),
            .records = {},
    };
    mCustomCollection = {
            .maxCacheSize = std::numeric_limits<std::size_t>::max(),
            .records = {},
    };
    return {};
}

void PerformanceProfiler::terminate() {
    Mutex::Autolock lock(mMutex);

    ALOGW("Terminating %s", name().c_str());

    mBoottimeCollection.records.clear();
    mBoottimeCollection = {};

    mPeriodicCollection.records.clear();
    mPeriodicCollection = {};

    mUserSwitchCollections.clear();

    mCustomCollection.records.clear();
    mCustomCollection = {};
}

Result<void> PerformanceProfiler::onDump(int fd) const {
    Mutex::Autolock lock(mMutex);
    if (!WriteStringToFd(StringPrintf(kBootTimeCollectionTitle, std::string(75, '-').c_str(),
                                      std::string(33, '=').c_str()),
                         fd) ||
        !WriteStringToFd(mBoottimeCollection.toString(), fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to dump the boot-time collection report.";
    }
    if (!WriteStringToFd(StringPrintf(kWakeUpCollectionTitle, std::string(75, '-').c_str(),
                                      std::string(27, '=').c_str()),
                         fd) ||
        !WriteStringToFd(mWakeUpCollection.toString(), fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to dump the boot-time collection report.";
    }
    if (const auto& result = onUserSwitchCollectionDump(fd); !result.ok()) {
        return result.error();
    }
    if (!WriteStringToFd(StringPrintf(kPeriodicCollectionTitle, std::string(75, '-').c_str(),
                                      std::string(38, '=').c_str()),
                         fd) ||
        !WriteStringToFd(mPeriodicCollection.toString(), fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to dump the periodic collection report.";
    }
    return {};
}

Result<void> PerformanceProfiler::onCustomCollectionDump(int fd) {
    if (fd == -1) {
        // Custom collection ends so clear the cache.
        mCustomCollection.records.clear();
        mCustomCollection = {
                .maxCacheSize = std::numeric_limits<std::size_t>::max(),
                .records = {},
        };
        return {};
    }

    if (!WriteStringToFd(StringPrintf(kCustomCollectionTitle, std::string(75, '-').c_str(),
                                      std::string(75, '-').c_str()),
                         fd) ||
        !WriteStringToFd(mCustomCollection.toString(), fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to write custom I/O collection report.";
    }

    return {};
}

Result<void> PerformanceProfiler::onSystemStartup() {
    Mutex::Autolock lock(mMutex);
    mBoottimeCollection.records.clear();
    mWakeUpCollection.records.clear();
    return {};
}

void PerformanceProfiler::onCarWatchdogServiceRegistered() {
    Mutex::Autolock lock(mMutex);
    mDoSendResourceUsageStats =
            sysprop::syncResourceUsageStatsWithCarServiceEnabled().value_or(false);
}

Result<void> PerformanceProfiler::onBoottimeCollection(
        time_t time, const wp<UidStatsCollectorInterface>& uidStatsCollector,
        const wp<ProcStatCollectorInterface>& procStatCollector, ResourceStats* resourceStats) {
    const sp<UidStatsCollectorInterface> uidStatsCollectorSp = uidStatsCollector.promote();
    const sp<ProcStatCollectorInterface> procStatCollectorSp = procStatCollector.promote();
    auto result = checkDataCollectors(uidStatsCollectorSp, procStatCollectorSp);
    if (!result.ok()) {
        return result;
    }
    Mutex::Autolock lock(mMutex);
    return processLocked(time, SystemState::NORMAL_MODE, std::unordered_set<std::string>(),
                         uidStatsCollectorSp, procStatCollectorSp, &mBoottimeCollection,
                         resourceStats);
}

Result<void> PerformanceProfiler::onPeriodicCollection(
        time_t time, SystemState systemState,
        const wp<UidStatsCollectorInterface>& uidStatsCollector,
        const wp<ProcStatCollectorInterface>& procStatCollector, ResourceStats* resourceStats) {
    const sp<UidStatsCollectorInterface> uidStatsCollectorSp = uidStatsCollector.promote();
    const sp<ProcStatCollectorInterface> procStatCollectorSp = procStatCollector.promote();
    clearExpiredSystemEventCollections(time);
    auto result = checkDataCollectors(uidStatsCollectorSp, procStatCollectorSp);
    if (!result.ok()) {
        return result;
    }
    Mutex::Autolock lock(mMutex);
    return processLocked(time, systemState, std::unordered_set<std::string>(), uidStatsCollectorSp,
                         procStatCollectorSp, &mPeriodicCollection, resourceStats);
}

Result<void> PerformanceProfiler::onUserSwitchCollection(
        time_t time, userid_t from, userid_t to,
        const android::wp<UidStatsCollectorInterface>& uidStatsCollector,
        const android::wp<ProcStatCollectorInterface>& procStatCollector) {
    const sp<UidStatsCollectorInterface> uidStatsCollectorSp = uidStatsCollector.promote();
    const sp<ProcStatCollectorInterface> procStatCollectorSp = procStatCollector.promote();
    auto result = checkDataCollectors(uidStatsCollectorSp, procStatCollectorSp);
    if (!result.ok()) {
        return result;
    }
    Mutex::Autolock lock(mMutex);
    if (mUserSwitchCollections.empty() || mUserSwitchCollections.back().from != from ||
        mUserSwitchCollections.back().to != to) {
        UserSwitchCollectionInfo userSwitchCollection = {
                {
                        .maxCacheSize = std::numeric_limits<std::size_t>::max(),
                        .records = {},
                },
                .from = from,
                .to = to,
        };
        mUserSwitchCollections.push_back(userSwitchCollection);
    }
    if (mUserSwitchCollections.size() > mMaxUserSwitchEvents) {
        mUserSwitchCollections.erase(mUserSwitchCollections.begin());
    }
    return processLocked(time, SystemState::NORMAL_MODE, std::unordered_set<std::string>(),
                         uidStatsCollectorSp, procStatCollectorSp, &mUserSwitchCollections.back(),
                         /*resourceStats=*/nullptr);
}

Result<void> PerformanceProfiler::onWakeUpCollection(
        time_t time, const android::wp<UidStatsCollectorInterface>& uidStatsCollector,
        const android::wp<ProcStatCollectorInterface>& procStatCollector) {
    const sp<UidStatsCollectorInterface> uidStatsCollectorSp = uidStatsCollector.promote();
    const sp<ProcStatCollectorInterface> procStatCollectorSp = procStatCollector.promote();
    auto result = checkDataCollectors(uidStatsCollectorSp, procStatCollectorSp);
    if (!result.ok()) {
        return result;
    }
    Mutex::Autolock lock(mMutex);
    return processLocked(time, SystemState::NORMAL_MODE, std::unordered_set<std::string>(),
                         uidStatsCollectorSp, procStatCollectorSp, &mWakeUpCollection,
                         /*resourceStats=*/nullptr);
}

Result<void> PerformanceProfiler::onCustomCollection(
        time_t time, SystemState systemState, const std::unordered_set<std::string>& filterPackages,
        const wp<UidStatsCollectorInterface>& uidStatsCollector,
        const wp<ProcStatCollectorInterface>& procStatCollector, ResourceStats* resourceStats) {
    const sp<UidStatsCollectorInterface> uidStatsCollectorSp = uidStatsCollector.promote();
    const sp<ProcStatCollectorInterface> procStatCollectorSp = procStatCollector.promote();
    auto result = checkDataCollectors(uidStatsCollectorSp, procStatCollectorSp);
    if (!result.ok()) {
        return result;
    }
    Mutex::Autolock lock(mMutex);
    return processLocked(time, systemState, filterPackages, uidStatsCollectorSp,
                         procStatCollectorSp, &mCustomCollection, resourceStats);
}

// TODO(b/266008677): Use the systemState variable to correctly attribute the mode of the I/O
// usage stats when collecting the resource usage stats.
Result<void> PerformanceProfiler::processLocked(
        time_t time, [[maybe_unused]] SystemState systemState,
        const std::unordered_set<std::string>& filterPackages,
        const sp<UidStatsCollectorInterface>& uidStatsCollector,
        const sp<ProcStatCollectorInterface>& procStatCollector, CollectionInfo* collectionInfo,
        [[maybe_unused]] ResourceStats* resourceStats) {
    if (collectionInfo->maxCacheSize == 0) {
        return Error() << "Maximum cache size cannot be 0";
    }
    PerfStatsRecord record{
            .time = time,
    };
    processUidStatsLocked(filterPackages, uidStatsCollector, &record.userPackageSummaryStats);
    processProcStatLocked(procStatCollector, &record.systemSummaryStats);
    // The system-wide CPU time should be the same as CPU time aggregated here across all UID, so
    // reuse the total CPU time from SystemSummaryStat
    record.userPackageSummaryStats.totalCpuTimeMillis =
            record.systemSummaryStats.totalCpuTimeMillis;
    // The system-wide CPU cycles are the aggregate of all the UID's CPU cycles collected during
    // each poll.
    record.systemSummaryStats.totalCpuCycles = record.userPackageSummaryStats.totalCpuCycles;
    if (collectionInfo->records.size() > collectionInfo->maxCacheSize) {
        collectionInfo->records.erase(collectionInfo->records.begin());  // Erase the oldest record.
    }
    collectionInfo->records.push_back(record);
    return {};
}

void PerformanceProfiler::processUidStatsLocked(
        const std::unordered_set<std::string>& filterPackages,
        const sp<UidStatsCollectorInterface>& uidStatsCollector,
        UserPackageSummaryStats* userPackageSummaryStats) {
    const std::vector<UidStats>& uidStats = uidStatsCollector->deltaStats();
    if (uidStats.empty()) {
        return;
    }
    if (filterPackages.empty()) {
        userPackageSummaryStats->topNCpuTimes.resize(mTopNStatsPerCategory);
        userPackageSummaryStats->topNIoReads.resize(mTopNStatsPerCategory);
        userPackageSummaryStats->topNIoWrites.resize(mTopNStatsPerCategory);
        userPackageSummaryStats->topNIoBlocked.resize(mTopNStatsPerCategory);
        userPackageSummaryStats->topNMajorFaults.resize(mTopNStatsPerCategory);
    }
    for (const auto& curUidStats : uidStats) {
        // Set the overall stats.
        userPackageSummaryStats->totalCpuCycles += curUidStats.procStats.cpuCycles;
        addUidIoStats(curUidStats.ioStats.metrics, userPackageSummaryStats->totalIoStats);
        userPackageSummaryStats->totalMajorFaults += curUidStats.procStats.totalMajorFaults;

        // Transform |UidStats| to |UserPackageStats| for each stats view.
        UserPackageStats ioReadsPackageStats =
                UserPackageStats(MetricType::READ_BYTES, curUidStats);
        UserPackageStats ioWritesPackageStats =
                UserPackageStats(MetricType::WRITE_BYTES, curUidStats);
        UserPackageStats cpuTimePackageStats =
                UserPackageStats(CPU_TIME, curUidStats, mTopNStatsPerSubcategory);
        UserPackageStats ioBlockedPackageStats =
                UserPackageStats(IO_BLOCKED_TASKS_COUNT, curUidStats, mTopNStatsPerSubcategory);
        UserPackageStats majorFaultsPackageStats =
                UserPackageStats(MAJOR_FAULTS, curUidStats, mTopNStatsPerSubcategory);

        if (filterPackages.empty()) {
            cacheTopNStats(ioReadsPackageStats, &userPackageSummaryStats->topNIoReads);
            cacheTopNStats(ioWritesPackageStats, &userPackageSummaryStats->topNIoWrites);
            cacheTopNStats(cpuTimePackageStats, &userPackageSummaryStats->topNCpuTimes);
            if (cacheTopNStats(ioBlockedPackageStats, &userPackageSummaryStats->topNIoBlocked)) {
                userPackageSummaryStats->taskCountByUid[ioBlockedPackageStats.uid] =
                        curUidStats.procStats.totalTasksCount;
            }
            cacheTopNStats(majorFaultsPackageStats, &userPackageSummaryStats->topNMajorFaults);
        } else if (filterPackages.count(curUidStats.genericPackageName()) != 0) {
            userPackageSummaryStats->topNIoReads.push_back(ioReadsPackageStats);
            userPackageSummaryStats->topNIoWrites.push_back(ioWritesPackageStats);
            userPackageSummaryStats->topNCpuTimes.push_back(cpuTimePackageStats);
            userPackageSummaryStats->topNIoBlocked.push_back(ioBlockedPackageStats);
            userPackageSummaryStats->topNMajorFaults.push_back(majorFaultsPackageStats);
            userPackageSummaryStats->taskCountByUid[ioBlockedPackageStats.uid] =
                    curUidStats.procStats.totalTasksCount;
        }
    }
    if (mLastMajorFaults != 0) {
        int64_t increase = userPackageSummaryStats->totalMajorFaults - mLastMajorFaults;
        userPackageSummaryStats->majorFaultsPercentChange =
                (static_cast<double>(increase) / static_cast<double>(mLastMajorFaults)) * 100.0;
    }
    mLastMajorFaults = userPackageSummaryStats->totalMajorFaults;

    const auto removeEmptyStats = [](std::vector<UserPackageStats>& userPackageStats) {
        for (auto it = userPackageStats.begin(); it != userPackageStats.end(); ++it) {
            /* std::monostate is the first alternative in the variant. When the variant is
             * uninitialized, the index points to this alternative.
             */
            if (it->statsView.index() == 0) {
                userPackageStats.erase(it, userPackageStats.end());
                break;
            }
        }
    };
    removeEmptyStats(userPackageSummaryStats->topNCpuTimes);
    removeEmptyStats(userPackageSummaryStats->topNIoReads);
    removeEmptyStats(userPackageSummaryStats->topNIoWrites);
    removeEmptyStats(userPackageSummaryStats->topNIoBlocked);
    removeEmptyStats(userPackageSummaryStats->topNMajorFaults);
}

void PerformanceProfiler::processProcStatLocked(
        const sp<ProcStatCollectorInterface>& procStatCollector,
        SystemSummaryStats* systemSummaryStats) const {
    const ProcStatInfo& procStatInfo = procStatCollector->deltaStats();
    systemSummaryStats->cpuIoWaitTimeMillis = procStatInfo.cpuStats.ioWaitTimeMillis;
    systemSummaryStats->cpuIdleTimeMillis = procStatInfo.cpuStats.idleTimeMillis;
    systemSummaryStats->totalCpuTimeMillis = procStatInfo.totalCpuTimeMillis();
    systemSummaryStats->contextSwitchesCount = procStatInfo.contextSwitchesCount;
    systemSummaryStats->ioBlockedProcessCount = procStatInfo.ioBlockedProcessCount;
    systemSummaryStats->totalProcessCount = procStatInfo.totalProcessCount();
}

Result<void> PerformanceProfiler::onUserSwitchCollectionDump(int fd) const {
    if (!WriteStringToFd(StringPrintf(kUserSwitchCollectionTitle, std::string(75, '-').c_str(),
                                      std::string(38, '=').c_str()),
                         fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to dump the user-switch collection report.";
    }
    if (!mUserSwitchCollections.empty() &&
        !WriteStringToFd(StringPrintf(kUserSwitchCollectionSubtitle, mUserSwitchCollections.size()),
                         fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to dump the user-switch collection report.";
    }
    if (mUserSwitchCollections.empty() && !WriteStringToFd(kEmptyCollectionMessage, fd)) {
        return Error(FAILED_TRANSACTION) << "Failed to dump the user-switch collection report.";
    }
    for (size_t i = 0; i < mUserSwitchCollections.size(); ++i) {
        const auto& userSwitchCollection = mUserSwitchCollections[i];
        if (!WriteStringToFd(StringPrintf(kUserSwitchEventTitle, i, userSwitchCollection.from,
                                          userSwitchCollection.to, std::string(26, '=').c_str()),
                             fd) ||
            !WriteStringToFd(userSwitchCollection.toString(), fd)) {
            return Error(FAILED_TRANSACTION) << "Failed to dump the user-switch collection report.";
        }
    }
    return {};
}

void PerformanceProfiler::clearExpiredSystemEventCollections(time_t now) {
    Mutex::Autolock lock(mMutex);
    auto clearExpiredSystemEvent = [&](CollectionInfo* info) -> bool {
        if (info->records.empty() ||
            difftime(now, info->records.back().time) < mSystemEventDataCacheDurationSec.count()) {
            return false;
        }
        info->records.clear();
        return true;
    };
    if (clearExpiredSystemEvent(&mBoottimeCollection)) {
        ALOGI("Cleared boot-time collection stats");
    }
    if (clearExpiredSystemEvent(&mWakeUpCollection)) {
        ALOGI("Cleared wake-up collection stats");
    }
    if (!mUserSwitchCollections.empty() &&
        clearExpiredSystemEvent(&mUserSwitchCollections.front())) {
        mUserSwitchCollections.erase(mUserSwitchCollections.begin());
        ALOGI("Cleared the oldest user-switch event collection stats");
    }
}

}  // namespace watchdog
}  // namespace automotive
}  // namespace android
