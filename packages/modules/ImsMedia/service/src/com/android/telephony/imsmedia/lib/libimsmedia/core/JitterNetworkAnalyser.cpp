/**
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

#include <ImsMediaDefine.h>
#include <JitterNetworkAnalyser.h>
#include <ImsMediaTimer.h>
#include <ImsMediaTrace.h>
#include <numeric>
#include <cmath>
#include <algorithm>

#define MAX_JITTER_LIST_SIZE    (150)
#define PACKET_INTERVAL         (20)    // milliseconds
#define BUFFER_INCREASE_TH      (200)   // milliseconds
#define BUFFER_DECREASE_TH      (3000)  // milliseconds
#define MARGIN_WEIGHT           (1.8f)
#define BUFFER_IN_DECREASE_SIZE (1)

JitterNetworkAnalyser::JitterNetworkAnalyser()
{
    mMinJitterBufferSize = 0;
    mMaxJitterBufferSize = 0;
    mBufferIncThreshold = BUFFER_INCREASE_TH;
    mBufferDecThreshold = BUFFER_DECREASE_TH;
    mBufferStepSize = BUFFER_IN_DECREASE_SIZE;
    mBufferWeight = MARGIN_WEIGHT;

    IMLOGD4("[JitterNetworkAnalyser] incThreshold[%d], decThreshold[%d], stepSize[%d], "
            "weight[%.3f]",
            mBufferIncThreshold, mBufferDecThreshold, mBufferStepSize, mBufferWeight);
    Reset();
}

JitterNetworkAnalyser::~JitterNetworkAnalyser() {}

void JitterNetworkAnalyser::Reset()
{
    mNetworkStatus = NETWORK_STATUS_NORMAL;
    mGoodStatusEnteringTime = 0;
    mBadStatusChangedTime = 0;

    {
        std::lock_guard<std::mutex> guard(mMutex);
        mListJitters.clear();
        mMapDelta.clear();
        mTimeLateArrivals = 0;
    }
}

void JitterNetworkAnalyser::SetMinMaxJitterBufferSize(
        uint32_t nMinBufferSize, uint32_t nMaxBufferSize)
{
    mMinJitterBufferSize = nMinBufferSize;
    mMaxJitterBufferSize = nMaxBufferSize;
}

void JitterNetworkAnalyser::SetJitterOptions(
        uint32_t incThreshold, uint32_t decThreshold, uint32_t stepSize, double weight)
{
    mBufferIncThreshold = incThreshold;
    mBufferDecThreshold = decThreshold;
    mBufferStepSize = stepSize;
    mBufferWeight = weight;

    IMLOGD4("[SetJitterOptions] incThreshold[%d], decThreshold[%d], stepSize[%d], weight[%.3f]",
            mBufferIncThreshold, mBufferDecThreshold, mBufferStepSize, mBufferWeight);
}

template <typename Map>
typename Map::const_iterator getGreatestLess(Map const& m, typename Map::key_type const& k)
{
    typename Map::const_iterator it = m.lower_bound(k);
    if (it != m.begin())
    {
        return --it;
    }
    return m.end();
}

int32_t JitterNetworkAnalyser::CalculateTransitTimeDifference(
        uint32_t timestamp, uint32_t arrivalTime)
{
    std::lock_guard<std::mutex> guard(mMutex);

    if (mMapDelta.size() == 0)
    {
        mMapDelta.insert({timestamp, arrivalTime});
        return 0;
    }

    if (mMapDelta.size() > MAX_JITTER_LIST_SIZE)
    {
        mMapDelta.erase(mMapDelta.begin());
    }

    mMapDelta.insert({timestamp, arrivalTime});

    if (getGreatestLess(mMapDelta, timestamp) == mMapDelta.end())
    {
        mListJitters.push_back(0);
        return 0;
    }

    int32_t inputTimestampGap = timestamp - getGreatestLess(mMapDelta, timestamp)->first;
    int32_t inputTimeGap = arrivalTime - getGreatestLess(mMapDelta, timestamp)->second;
    int32_t jitter = std::abs(inputTimeGap - inputTimestampGap);

    if (jitter < mMaxJitterBufferSize * PACKET_INTERVAL)
    {
        mListJitters.push_back(jitter);
    }

    if (mListJitters.size() > MAX_JITTER_LIST_SIZE)
    {
        mListJitters.pop_front();
    }

    return jitter;
}

void JitterNetworkAnalyser::SetLateArrivals(uint32_t time)
{
    mTimeLateArrivals = time;
}

double JitterNetworkAnalyser::CalculateDeviation(double* pMean)
{
    std::lock_guard<std::mutex> guard(mMutex);

    if (mListJitters.empty())
    {
        *pMean = 0;
        return 0.0f;
    }

    double mean =
            std::accumulate(mListJitters.begin(), mListJitters.end(), 0.0f) / mListJitters.size();

    *pMean = mean;

    double dev = sqrt(std::accumulate(mListJitters.begin(), mListJitters.end(), 0.0f,
                              [mean](int x, int y)
                              {
                                  return x + std::pow(y - mean, 2);
                              }) /
            mListJitters.size());

    return dev;
}

int32_t JitterNetworkAnalyser::GetMaxJitterValue()
{
    std::lock_guard<std::mutex> guard(mMutex);

    if (mListJitters.empty())
    {
        return 0;
    }

    return *std::max_element(mListJitters.begin(), mListJitters.end());
}

uint32_t JitterNetworkAnalyser::GetNextJitterBufferSize(
        uint32_t nCurrJitterBufferSize, uint32_t currentTime)
{
    uint32_t nextJitterBuffer = nCurrJitterBufferSize;
    NETWORK_STATUS networkStatus;

    double dev, mean;
    double calcJitterSize = 0;
    int32_t maxJitter = GetMaxJitterValue();
    dev = CalculateDeviation(&mean);
    calcJitterSize = (double)maxJitter * mBufferWeight;

    if (calcJitterSize >= nCurrJitterBufferSize * PACKET_INTERVAL ||
            maxJitter >= nCurrJitterBufferSize * PACKET_INTERVAL)
    {
        networkStatus = NETWORK_STATUS_BAD;
    }
    else if (calcJitterSize < (nCurrJitterBufferSize - 1) * PACKET_INTERVAL &&
            maxJitter < (nCurrJitterBufferSize - 1) * PACKET_INTERVAL - 10)
    {
        networkStatus = NETWORK_STATUS_GOOD;
    }
    else
    {
        networkStatus = NETWORK_STATUS_NORMAL;
    }

    IMLOGD_PACKET6(IM_PACKET_LOG_JITTER,
            "[GetNextJitterBufferSize] size[%4.2f], mean[%4.2f], dev[%4.2f], max[%d], curr[%d], "
            "status[%d]",
            calcJitterSize, mean, dev, maxJitter, nCurrJitterBufferSize, networkStatus);

    switch (networkStatus)
    {
        case NETWORK_STATUS_BAD:
        {
            if (mBadStatusChangedTime == 0 ||
                    (currentTime - mBadStatusChangedTime) >= mBufferIncThreshold)
            {
                nextJitterBuffer = (calcJitterSize + PACKET_INTERVAL) / PACKET_INTERVAL;

                if (nextJitterBuffer > mMaxJitterBufferSize)
                {
                    nextJitterBuffer = mMaxJitterBufferSize;
                }

                if (nextJitterBuffer < mMinJitterBufferSize)
                {
                    nextJitterBuffer = mMinJitterBufferSize;
                }

                IMLOGD_PACKET2(IM_PACKET_LOG_JITTER,
                        "[GetNextJitterBufferSize] increase curr[%d], next[%d]",
                        nCurrJitterBufferSize, nextJitterBuffer);
                mBadStatusChangedTime = currentTime;
            }

            break;
        }
        case NETWORK_STATUS_GOOD:
        {
            if (mNetworkStatus != NETWORK_STATUS_GOOD)
            {
                mGoodStatusEnteringTime = currentTime;
            }
            else
            {
                uint32_t nTimeDiff = currentTime - mGoodStatusEnteringTime;

                if (nTimeDiff >= mBufferDecThreshold &&
                        (mTimeLateArrivals == 0 ||
                                currentTime - mTimeLateArrivals > mBufferDecThreshold))
                {
                    if (nCurrJitterBufferSize > mMinJitterBufferSize)
                    {
                        nextJitterBuffer = nCurrJitterBufferSize - mBufferStepSize;
                    }

                    IMLOGD_PACKET2(IM_PACKET_LOG_JITTER,
                            "[GetNextJitterBufferSize] decrease curr[%d], next[%d]",
                            nCurrJitterBufferSize, nextJitterBuffer);
                    networkStatus = NETWORK_STATUS_NORMAL;
                }
            }

            break;
        }
        default:
            nextJitterBuffer = nCurrJitterBufferSize;
            break;
    }

    mNetworkStatus = networkStatus;
    return nextJitterBuffer;
}
