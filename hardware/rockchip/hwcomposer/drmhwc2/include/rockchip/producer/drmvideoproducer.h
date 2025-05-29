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
#ifndef _DRM_VIDEO_PRODUCER_H_
#define _DRM_VIDEO_PRODUCER_H_

#include <mutex>
#include <queue>

#include "utils/drmfence.h"
#include "drmbuffer.h"
#include "rockchip/producer/videotunnel/video_tunnel.h"
#include "rockchip/producer/vpcontext.h"
#include "utils/worker.h"

namespace android {
class DrmVideoProducer : public Worker{
public:
  static DrmVideoProducer* getInstance(){
    static DrmVideoProducer drmVideoProducer;
    return &drmVideoProducer;
  };
  //Worker need constructor and destorctor be public, but don't call it!!!;
  DrmVideoProducer();
  ~DrmVideoProducer();


  // Init video tunel.
  int Init();
  // Is invalid.
  bool IsValid();
  // Create tunnel connection.
  int CreateConnection(int display_id, int tunnel_id, android_dataspace_t dataspace = HAL_DATASPACE_UNKNOWN);
  // Destory Connection
  int DestoryConnection(int display_id, int tunnel_id);
  // Get Last video buffer
  std::shared_ptr<DrmBuffer> AcquireBuffer(int display_id,
                                           int tunnel_id,
                                           vt_rect_t *dis_rect,
                                           int timeout_ms,
                                           bool wait_Fence = true
                                           );
  // Release video buffer
  int ReleaseBuffer(int display_id, int tunnel_id, uint64_t buffer_id);
  // Signal buffer's ReleaseFence
  int SignalReleaseFence(int display_id, int tunnel_id, uint64_t buffer_id);
  int SetProducerFps(int tunnel_id, float fps);
  float GetProducerFps(int tunnel_id);
  void PrintTimeStamp(int display_id, int tunnel_id, uint64_t buffer_id);
#ifdef USE_LIBPQ_HWPQ
  std::shared_ptr<DrmBuffer> DoHwPq(std::shared_ptr<VpContext> ctx, std::shared_ptr<DrmBuffer> buffer);
#endif

  enum ReleaseFenceMode{
    DisableReleaseFence = 0,
    EnableReleaseFence = 1
  };

 protected:
  void Routine() override;

 private:
  DrmVideoProducer(const DrmVideoProducer &) = delete;
  DrmVideoProducer &operator=(const DrmVideoProducer &) = delete;
  int InitLibHandle();
  bool bInit_;
  int iTunnelFd_;
  std::map<int, std::shared_ptr<VpContext>> mMapCtx_;
  mutable std::mutex mtx_;
  float uFastestFps_=120.0f;
  ReleaseFenceMode iFenceMode_ = EnableReleaseFence;
  //first:tunnel_id second:retry_counter
  std::vector<int> mPendingReleaseTunnel_;
  void printPendingReleaseTunnel();
  bool bLastAcquireSucceed = false;
#ifdef USE_LIBPQ_HWPQ
  std::shared_ptr<Pq> hwpq_ = NULL;
#endif
};

}; // namespace android


#endif // _VIDEO_PRODUCER_H_