/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ANDROID_DRM_DISPLAY_COMPOSITOR_H_
#define ANDROID_DRM_DISPLAY_COMPOSITOR_H_

#include "drmdisplaycomposition.h"
#include "drmframebuffer.h"
#include "drmlayer.h"
#include "resources/resourcemanager.h"
#include "vsyncworker.h"
#include "drmcompositorworker.h"
#ifdef USE_LIBPQ
#include "rkpq.h"
#endif

#include <pthread.h>
#include <memory>
#include <sstream>
#include <tuple>
#include <queue>

#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>

#define OVERSCAN_MIN_VALUE              (80)
#define OVERSCAN_MAX_VALUE              (100)

// One for the front, one for the back, and one for cases where we need to
// squash a frame that the hw can't display with hw overlays.
#define DRM_DISPLAY_BUFFERS 3

// If a scene is still for this number of vblanks flatten it to reduce power
// consumption.
#define FLATTEN_COUNTDOWN_INIT 60

namespace android {
class ResourceManager;

class DrmDisplayCompositor {
 public:
  DrmDisplayCompositor();
  ~DrmDisplayCompositor();

  int Init(ResourceManager *resource_manager, int display);

  std::unique_ptr<DrmDisplayComposition> CreateComposition() const;
  std::unique_ptr<DrmDisplayComposition> CreateInitializedComposition() const;
  int QueueComposition(std::unique_ptr<DrmDisplayComposition> composition);
  int TestComposition(DrmDisplayComposition *composition);
  int Composite();
  int CollectSFInfo();
  // 顺序模式
  int CollectSFInfoBySequence();
  // 丢帧模式
  int CollectSFInfoByDrop();

  int CollectVPInfo();
  int CollectVPHdrInfo(DrmHwcLayer &layer);
  // HwVirtualDisplay功能
  int WriteBackByRGA();
  void Dump(std::ostringstream *out) const;
  void Vsync(int display, int64_t timestamp);
  void SingalCompsition(std::unique_ptr<DrmDisplayComposition> composition);
  void ClearDisplayHdrState();
  void ClearDisplay();
  int display() { return display_;};
  std::tuple<uint32_t, uint32_t, int> GetActiveModeResolution();

  bool HaveQueuedComposites() const;
  bool IsSidebandMode() const;
  int GetCompositeQueueMaxSize(DrmDisplayComposition* composition);

 private:
  struct ModeState {
    bool needs_modeset = false;
    DrmMode mode;
    uint32_t blob_id = 0;
    uint32_t old_blob_id = 0;
  };

struct SidebandState {
  bool enable_ = false;
  uint64_t tunnel_id_ = 0;
  std::shared_ptr<DrmBuffer> buffer_ = NULL;
};

  struct HdrState{
    DrmHdrType mode_ = DRM_HWC_SDR;
    bool bHasYuv10bit_ = false;
    android_dataspace_t datespace_;
  };

  struct ModeSetState{
    HdrState hdr_;
  #ifdef USE_LIBPQ_HWPQ
    std::shared_ptr<rk_hwpq_reg> hwpq_regs_;
  #endif
  };

  DrmDisplayCompositor(const DrmDisplayCompositor &) = delete;

  // We'll wait for acquire fences to fire for kAcquireWaitTimeoutMs,
  // kAcquireWaitTries times, logging a warning in between.
  static const int kAcquireWaitTries = 5;
  static const int kAcquireWaitTimeoutMs = 100;
  int CheckOverscan(drmModeAtomicReqPtr pset, DrmCrtc* crtc, int display, const char *UniqueName);
  // Multi Thread function.
  int GetTimestamp();
  int64_t GetPhasedVSync(int64_t frame_ns, int64_t current);
  int SyntheticWaitVBlank();
  int CollectInfo(std::unique_ptr<DrmDisplayComposition> composition,
                  int status, bool writeback = false);
  void Commit();
  int CollectCommitInfo(drmModeAtomicReqPtr pset,
                  DrmDisplayComposition *display_comp,
                  bool test_only,
                  DrmConnector *writeback_conn = NULL,
                  DrmHwcBuffer *writeback_buffer = NULL);
  int CommitSidebandStream(drmModeAtomicReqPtr pset,
                           DrmPlane* plane,
                           DrmHwcLayer &layer,
                           int zpos,
                           int crtc_id);
  int CommitFrame(DrmDisplayComposition *display_comp,
                  bool test_only,
                  DrmConnector *writeback_conn = NULL,
                  DrmHwcBuffer *writeback_buffer = NULL);
  int SetupWritebackCommit(drmModeAtomicReqPtr pset, uint32_t crtc_id,
                           DrmConnector *writeback_conn,
                           DrmHwcBuffer *writeback_buffer);
  int DisableWritebackCommit(drmModeAtomicReqPtr pset,
                             DrmConnector *writeback_conn);
#ifdef USE_LIBPQ_HWPQ
  int CollectHwPqInfo();
  int UpdateHwPqState();
#endif                      
  int CollectModeSetInfo(drmModeAtomicReqPtr pset,
                         DrmDisplayComposition *display_comp,
                         bool is_sideband_collect);
  int UpdateModeSetState();
  int UpdateDrmPlaneAssignState();
  int UpdateSidebandState();
  int ApplyDpms(DrmDisplayComposition *display_comp);
  int DisablePlanes(DrmDisplayComposition *display_comp);

  void ApplyFrame(std::unique_ptr<DrmDisplayComposition> composition,
                  int status, bool writeback = false);
  int FlattenActiveComposition();
  int FlattenSerial(DrmConnector *writeback_conn);
  int FlattenConcurrent(DrmConnector *writeback_conn);
  int FlattenOnDisplay(std::unique_ptr<DrmDisplayComposition> &src,
                       DrmConnector *writeback_conn, DrmMode &src_mode,
                       DrmHwcLayer *writeback_layer);

  bool CountdownExpired() const;

  std::tuple<int, uint32_t> CreateModeBlob(const DrmMode &mode);

  ResourceManager *resource_manager_;
  int display_;
  DrmCompositorWorker worker_;
  drmModeAtomicReqPtr pset_ = NULL;

  // Store the display request from SF.
  std::queue<std::unique_ptr<DrmDisplayComposition>> composite_queue_;
  // Store the display request from SF.
  std::queue<std::unique_ptr<DrmDisplayComposition>> composite_queue_temp_;
  // Store the request that is about to be submitted for display.
  std::map<int,std::unique_ptr<DrmDisplayComposition>> collect_composition_map_;
  // Store the request currently being displayed on the screen.
  std::map<int,std::unique_ptr<DrmDisplayComposition>> active_composition_map_;

  std::unique_ptr<DrmDisplayComposition> active_composition_;

  pthread_cond_t composite_queue_cond_;

  bool initialized_;
  bool active_;
  bool use_hw_overlays_;
  // Enter ClearDisplay state must SignalCompositionDone to signal releaseFence
  bool clear_;

  ModeState mode_;

  // RK Support:
  bool need_mode_set_;
  ModeSetState request_mode_set_;
  ModeSetState current_mode_set_;

  int framebuffer_index_;
  DrmFramebuffer framebuffers_[DRM_DISPLAY_BUFFERS];

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t lock_;

  // State tracking progress since our last Dump(). These are mutable since
  // we need to reset them on every Dump() call.
  mutable uint64_t dump_frames_composited_;
  mutable uint64_t dump_last_timestamp_ns_;
  VSyncWorker vsync_worker_;
  int64_t flatten_countdown_;
  std::unique_ptr<Planner> planner_;
  int writeback_fence_;
  // Multi Thread function.
  int64_t last_timestamp_;
  struct timespec vsync_;

  std::map<int, uint64_t> mapDisplayHaveQeueuCnt_;

  bool bWriteBackRequestDisable_;
  bool bWriteBackEnable_;

  uint32_t hdr_blob_id_ = 0;

  uint64_t frame_no_ = 0;
  // RK Support Sideband mode
  SidebandState current_sideband2_;
  SidebandState drawing_sideband2_;

  // 丢帧模式
  bool drop_mode_ = false;

  // 动态DrmPlane迁移需要记录DrmPlane的disable信息
  std::vector<uint64_t> will_disable_drmplane_types;

#ifdef USE_LIBPQ
  // Sideband YUV444 tmp buffer
  std::shared_ptr<DrmBuffer> sidbenad_pq_tmp_buffer_ = NULL;
  std::shared_ptr<rkpq> pq_ = NULL;
  int pq_last_init_format_ = 0;
#endif
#ifdef USE_LIBPQ_HWPQ
  uint32_t hwpq_shp_id_ = 0;
  uint32_t hwpq_csc_id_ = 0;
  uint32_t hwpq_acm_id_ = 0;
  uint32_t hwpq_dci_id_ = 0;
  DrmPlane* hwpq_plane = NULL;
  DrmCrtc *hwpq_crtc = NULL;
#endif
};
}  // namespace android

#endif  // ANDROID_DRM_DISPLAY_COMPOSITOR_H_
