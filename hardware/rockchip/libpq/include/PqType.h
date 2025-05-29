/*
 * Copyright (C) 2016 The Android Open Source Project
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
#pragma once

#include <stdint.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <time.h>
#include "autofd.h"
#ifdef USE_LIBPQ_HWPQ
#include "rkhwpq_api.h"
#endif

namespace android {

#define PQ_MAGIC 0x83991906
#define PQ_VERSION "Pq-1.2.33"
#define PQ_VERSION_NAME "vendor.tvinput.rkpq.version"
#define PQ_DEBUG_NAME "vendor.tvinput.rkpq.log"

#define PQ_ALOGE(x, ...)  \
    ALOGE("%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define PQ_ALOGW(x, ...)  \
    ALOGW("%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define PQ_ALOGI(x, ...)  \
    ALOGI("%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define PQ_ALOGD_IF(x, ...)  \
    ALOGD_IF(PqLogLevel(), "%s,line=%d " x ,__FUNCTION__,__LINE__, ##__VA_ARGS__)

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int UpdatePqLogLevel();
bool PqLogLevel();

enum PqError {
    PqNone = 0,
    PqBadVersion,
    PqBadStage,
    PqBadParameter,
    PqUnSupported,
    PqUnSucess,
    PqUnInit,
};

enum PqStage {
    PQ_UN_INIT = 0,
    PQ_INIT_SUCCESS,
    PQ_VERITY_SRC_SUCCESS,
    PQ_VERITY_DST_SUCCESS,
};

enum PqInitState {
    RKSWPQ_UN_INIT = 0,
    RKSWPQ_INITING,
    RKSWPQ_INIT_SUCCESS,
};

enum PqBufferMask {
    PQ_BUFFER_NONE = 0,
    PQ_AFBC_FORMATE = 1 << 1
};

struct PqVersion{
  int iMajor_;
  int iMinor_;
  int iPatchLevel_;
};

class PqRect{
public:
  int iLeft_;
  int iTop_;
  int iRight_;
  int iBottom_;

  int Width() const {return iRight_ - iLeft_; };
  int Height() const {return iBottom_ - iTop_; };

  PqRect() = default;
  PqRect(const PqRect& rhs){
    iLeft_   = rhs.iLeft_;
    iTop_    = rhs.iTop_;
    iRight_  = rhs.iRight_;
    iBottom_ = rhs.iBottom_;
  };

  PqRect& operator=(const PqRect& rhs){
    iLeft_   = rhs.iLeft_;
    iTop_    = rhs.iTop_;
    iRight_  = rhs.iRight_;
    iBottom_ = rhs.iBottom_;
    return *this;
  };

  bool isValid() const {
    return Width() > 0 && Height() > 0;
  };
};

class PqBufferInfo{
public:
  int iFd_;
  int iWidth_;
  int iHeight_;
  int iFormat_;
  int iStride_;
  int iHeightStride_;
  uint64_t uBufferId_;
  uint64_t uDataSpace_;
  uint64_t uBufferMask_;

  PqBufferInfo() = default;
  PqBufferInfo(const PqBufferInfo& rhs){
    iFd_     = rhs.iFd_;
    iWidth_  = rhs.iWidth_;
    iHeight_ = rhs.iHeight_;
    iFormat_ = rhs.iFormat_;
    iStride_ = rhs.iStride_;
    iHeightStride_ = rhs.iHeightStride_;
    uBufferId_ = rhs.uBufferId_;
    uDataSpace_ =  rhs.uDataSpace_;
    uBufferMask_ =  rhs.uBufferMask_;

  };

  PqBufferInfo& operator=(const PqBufferInfo& rhs){
    iFd_     = rhs.iFd_;
    iWidth_  = rhs.iWidth_;
    iHeight_ = rhs.iHeight_;
    iFormat_ = rhs.iFormat_;
    iStride_ = rhs.iStride_;
    iHeightStride_ = rhs.iHeightStride_;
    uBufferId_ = rhs.uBufferId_;
    uDataSpace_ =  rhs.uDataSpace_;
    uBufferMask_ =  rhs.uBufferMask_;
    return *this;
  };

  bool isValid() const {
    return iFd_ > 0 &&
           iWidth_ > 0 &&
           iHeight_ > 0 &&
           iStride_ > 0 &&
           iFormat_ > 0;
  }
};

class PqImageInfo{
public:
  PqBufferInfo mBufferInfo_;
  PqRect mCrop_;
  UniqueFd mAcquireFence_;
  bool mValid;
  PqImageInfo() = default;
  PqImageInfo(const PqImageInfo& rhs){
    mBufferInfo_ = rhs.mBufferInfo_;
    mCrop_ = rhs.mCrop_;
    mAcquireFence_ = rhs.mAcquireFence_.Dup();
  };

  PqImageInfo& operator=(const PqImageInfo& rhs){
    mBufferInfo_ = rhs.mBufferInfo_;
    mCrop_ = rhs.mCrop_;
    mAcquireFence_ = rhs.mAcquireFence_.Dup();
    return *this;
  };
};

#ifdef USE_LIBPQ_HWPQ
class HwPqImageInfo{
public:
  PqBufferInfo mBufferInfo_;
  PqRect mCrop_;
  UniqueFd mAcquireFence_;
  bool mValid;
  void * mVirtualAddress_;
  rk_hwpq_reg * mRkHwpqReg_;
  bool mNeedAllocNewBuffer_;
  HwPqImageInfo() = default;
  HwPqImageInfo(const HwPqImageInfo& rhs){
    mBufferInfo_ = rhs.mBufferInfo_;
    mCrop_ = rhs.mCrop_;
    mAcquireFence_ = rhs.mAcquireFence_.Dup();
    mVirtualAddress_ = rhs.mVirtualAddress_;
    mRkHwpqReg_ = rhs.mRkHwpqReg_;
    mNeedAllocNewBuffer_ = rhs.mNeedAllocNewBuffer_;
  };

  HwPqImageInfo& operator=(const HwPqImageInfo& rhs){
    mBufferInfo_ = rhs.mBufferInfo_;
    mCrop_ = rhs.mCrop_;
    mAcquireFence_ = rhs.mAcquireFence_.Dup();
    mVirtualAddress_ = rhs.mVirtualAddress_;
    mRkHwpqReg_ = rhs.mRkHwpqReg_;
    mNeedAllocNewBuffer_ = rhs.mNeedAllocNewBuffer_;
    return *this;
  };
};

struct HwPqPreInfo{
  int mVdppRunMode_;
  // TO ADD
};
#endif

class PqContext {
public:
  int mMagic_;
  PqVersion mVersion_;
  PqStage mStage_;
  PqImageInfo mSrc_;
  PqImageInfo mDst_;
#ifdef USE_LIBPQ_HWPQ
  HwPqImageInfo mHwPqSrc_;
  HwPqImageInfo mHwPqDst_;
  HwPqPreInfo mHwPqPreInfo_;
#endif
  float mEnhancementRate_;
};

class PqBackendContext {
public:
  int mMagic_;
  PqVersion mVersion_;
  PqStage mStage_;
  PqImageInfo mSrc_;
  PqImageInfo mDst_;
#ifdef USE_LIBPQ_HWPQ
  HwPqImageInfo mHwPqSrc_;
  HwPqImageInfo mHwPqDst_;
  HwPqPreInfo mHwPqPreInfo_;
#endif
  int iFenceTimeline_;
  UniqueFd ufCurrentFinishFence_;

  PqBackendContext(const PqContext ac):
    mMagic_(ac.mMagic_),
    mVersion_(ac.mVersion_),
    mStage_(ac.mStage_),
    mSrc_(ac.mSrc_),
    mDst_(ac.mDst_),
#ifdef USE_LIBPQ_HWPQ
    mHwPqSrc_(ac.mHwPqSrc_),
    mHwPqDst_(ac.mHwPqDst_),
    mHwPqPreInfo_(ac.mHwPqPreInfo_),
#endif
    iFenceTimeline_(0) {};
};

} // namespace android
