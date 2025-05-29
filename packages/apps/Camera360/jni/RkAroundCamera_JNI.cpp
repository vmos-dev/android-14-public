/*
 * Copyright 2009 Cedric Priscal
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "include/RkAroundCamera_JNI.h"
#include "include/RenderThread.h"
#include "rk_avm_api.h"
#include <android/bitmap.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <assert.h>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <ui/GraphicBuffer.h>
#include <unistd.h>
#include <vector>
#include <vndk/hardware_buffer.h>

/*added by kenvenis.chen@rock-chips.com*/
std::vector<AHardwareBuffer *> myActivatedCamBuffer;
static android::AvmProcessAdapter *mProcess = nullptr;
static int g_start_x[2] = {0, 0};
static int g_start_y[2] = {0, 0};
static int gWidth = 0;
static int gHeight = 0;
static float split_ratio = 0.3125f;

//初始化
JNIEXPORT jint JNICALL native_init(JNIEnv *env, jobject thiz) {
  LOGD("native_init");

  if (mProcess == nullptr) {
    mProcess = new android::AvmProcessAdapter();
  }
  mProcess->startProcess();

  return 0;
}

JNIEXPORT void JNICALL nativeSetBitmap(JNIEnv *env, jobject thiz,
                                       jobject dstBitmap, jlong elapsedTime) {
  // Grab the dst bitmap info and pixels
  AndroidBitmapInfo dstInfo;
  void *dstPixels;
  AndroidBitmap_getInfo(env, dstBitmap, &dstInfo);
  AndroidBitmap_lockPixels(env, dstBitmap, &dstPixels);

  // LOGD("get bitmap dst size is [%d %d]\n", dstInfo.width, dstInfo.height);
  gWidth = dstInfo.width;
  gHeight = dstInfo.height;

  if (mProcess != nullptr)
    mProcess->setDisplayAddr(dstPixels, dstInfo.width, dstInfo.height);
  else
    LOGE("NO AVM OBJECT EXIT\n");

  AndroidBitmap_unlockPixels(env, dstBitmap);
}

JNIEXPORT void JNICALL nativeSetSurroundType(JNIEnv *env, jobject thiz,
                                             jint type) {
  LOGD("RKAVM set surround type is %d\n", type);

  if (mProcess != nullptr)
    mProcess->setSurround3dType(type);
  else
    LOGE("NO AVM OBJECT EXIT\n");
}

JNIEXPORT jintArray JNICALL native_getCameraList(JNIEnv *env, jobject thiz) {
  jintArray jarr = env->NewIntArray(4);
  jint *arr = env->GetIntArrayElements(jarr, NULL);
  int i = 0;
  for (; i < 4; i++) {
    arr[i] = rk_avm_get_camera_id(i);
  }
  env->ReleaseIntArrayElements(jarr, arr, 0);

  return jarr;
}

JNIEXPORT jint JNICALL native_process(JNIEnv *env, jobject thiz,
                                      jobjectArray hardwareBufferArray) {
  LOGD("RKAVM-JNI: enter process\n");

  struct timeval tpend1, tpend2;
  long usec0 = 0;
  gettimeofday(&tpend1, NULL);

  for (int k = 0; k < 4; k++) {
    jobject jHardwareBuffer =
        (*env).GetObjectArrayElement(hardwareBufferArray, k);
    myActivatedCamBuffer.push_back(
        AHardwareBuffer_fromHardwareBuffer(env, jHardwareBuffer));
    // LOGD("======== GET HARDWAREBUFFER ARRAY id : %d size: [%d %d]\n", k,
    // desc.width, desc.height);
  }

  if (mProcess != nullptr)
    mProcess->notifyNewFrame();
  myActivatedCamBuffer.clear();
  gettimeofday(&tpend2, NULL);
  usec0 = 1000 * (tpend2.tv_sec - tpend1.tv_sec) +
          (tpend2.tv_usec - tpend1.tv_usec) / 1000;

  LOGD("RKAVM-JNI: exit render cost time=%ld ms \n", usec0);
  return 1;
}

JNIEXPORT void JNICALL native_onScreenTouch(JNIEnv *env, jobject thiz, jint sx,
                                            jint sy, jint mx, jint my) {
  LOGD("onScreenTouch start(%d,%d) current (%d,%d)", sx, sy, mx, my);

  int l_limit = (int)gWidth * split_ratio;
  if (sx < l_limit)
    return;

  if (mx < l_limit)
    return;

  int dx, dy;
  if (sx != g_start_x[0]) {
    g_start_x[0] = sx;
    g_start_x[1] = mx;
    dx = mx - sx;
  } else {
    dx = mx - g_start_x[1];
    g_start_x[1] = mx;
  }

  if (sy != g_start_y[0]) {
    g_start_y[0] = sy;
    g_start_y[1] = my;
    dy = my - sy;
  } else {
    dy = my - g_start_y[1];
    g_start_y[1] = my;
  }

  if (mProcess != nullptr && mProcess->getDispInitState()) {
    int cmd_data[2];
    cmd_data[0] = dx;
    cmd_data[1] = dy;
    rk_avm_rev_cmd_proc(rk_cmd_render_set_screen_offset, cmd_data);
  }
}

//清除
JNIEXPORT jint JNICALL native_clear(JNIEnv *env, jobject thiz) {
  LOGD("Flash test : ************* native_clear()");

  if (mProcess) {
    mProcess->pauseProcess();
    mProcess->stopProcess();
    delete mProcess;
    mProcess = nullptr;
  }

  LOGD("destroy success");

  return JNI_OK;
}

//======================================================================================

static JNINativeMethod gMethods[] = {
    {"native_init", "()I", (void *)native_init},
    {"native_process", "([Landroid/hardware/HardwareBuffer;)I",
     (void *)native_process},
    {"nativeSetBitmap", "(Landroid/graphics/Bitmap;J)V",
     (void *)nativeSetBitmap},
    {"native_onScreenTouch", "(IIII)V", (void *)native_onScreenTouch},
    {"nativeSetSurroundType", "(I)V", (void *)nativeSetSurroundType},
    {"native_getCameraList", "()[I", (void *)native_getCameraList},
    {"native_clear", "()I", (void *)native_clear},
};

static int registerNativeMethods(JNIEnv *env, const char *className,
                                 JNINativeMethod *gMethods, int numMethods) {
  jclass clazz;
  clazz = env->FindClass(className);
  if (clazz == NULL) {
    LOGE("---------clazz is null");
    return JNI_FALSE;
  }
  if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
    LOGE("---------RegisterNatives < 0");
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

/*
 * System.loadLibrary("libxxx")
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  LOGW("");
  JNIEnv *env = NULL;

  jint ret = vm->GetEnv((void **)&env, JNI_VERSION_1_6);
  if (ret != JNI_OK) {
    LOGE("GetEnv JNI_VERSION_1_6 failed");
    return JNI_ERR;
  }
  assert(env != NULL);

  const char *kClassName = "com/rockchip/aroundcamera/AroundCameraJNI";
  ret = registerNativeMethods(env, kClassName, gMethods,
                              sizeof(gMethods) / sizeof(gMethods[0]));

  if (ret != JNI_TRUE) {
    LOGE("registerNatives failed");
    return JNI_ERR;
  }

  return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM *vm, void *reserved) {
  LOGW("");
  JNIEnv *env = NULL;
  jint ret = vm->GetEnv((void **)&env, JNI_VERSION_1_6);
  LOGE("ret=%d", ret);
}

namespace android {
AvmProcessAdapter::AvmProcessAdapter() {
  mProcessRuning = -1;
  mProcessState = 0;
  mPCmdsem.Create();
  mStateComplete.Create();

  mProcessThread = new ProcessThread(this);
  mProcessThread->run("ProcessThread", ANDROID_PRIORITY_DISPLAY);
}

AvmProcessAdapter::~AvmProcessAdapter() {
  if (mProcessThread != NULL) {
    // stop thread and exit
    if (mProcessRuning != STA_PROCESS_STOP)
      stopProcess();
    mProcessThread->requestExitAndWait();
    mProcessThread.clear();
    printf("processThread loop ending\n");
  }

  // printf("process thread addr %p mutex %p\n", this, &mProcessLock);

  mPCmdsem.Release();
  mStateComplete.Release();
  printf("processThread loop exit\n");
}

void AvmProcessAdapter::initSurround3d() {

  char base_path[256];
  char avm_set[] = "/avm_settings.xml";
  memset(base_path, 0, 256);

  sprintf(base_path, "/vendor/etc/camera/AppData_rk/");

  property_get("persist.vendor.rockchip.evs.cam", dump_type, "NVP6188-AHD");

  strcat(base_path, dump_type);
  strcat(base_path, avm_set);

  LOGD("dump_type: %s LOAD AVM SETTING XML %s\n", dump_type, base_path);

  if (mReadyInit == false) {
    // const char *init_path =
    // "/system/etc/automotive/evs/RKAVM/AppDataRK/avm_settings.xml";
    rk_avm_set_extern_buf_enable(true);
    rkavm_set_evs_apk_show(true);
    int ret_record = rk_avm_init(base_path, true);
    if (ret_record >= 0) {
      LOGD("surround3d init success\n");
      mReadyInit = true;
    } else {
      LOGE("surround3d init fail\n");
    }
  }
}

void AvmProcessAdapter::deinitSurround3d() {
  if (mReadyInit) {
    rk_avm_deinit();
    mReadyInit = false;
  }
}

void AvmProcessAdapter::setSurround3dType(int type) {
  myType = type;
  rk_avm_rev_cmd_proc(type, nullptr);
}

void AvmProcessAdapter::setProcessState(int state) {
  // Mutex::Autolock lock(mProcessLock);
  mProcessState = state;
}

int AvmProcessAdapter::getProcessState() {
  Mutex::Autolock lock(mProcessLock);
  return mProcessState;
}

int AvmProcessAdapter::startProcess() {
  Mutex::Autolock lock(mProcessLock);
  int err = NO_ERROR;
  RSemaphore sem;

  // mProcessLock.lock();
  if (mProcessRuning == STA_PROCESS_RUNNING) {
    LOGW("%s(%d): process thread is already run\n", __FUNCTION__, __LINE__);
    goto cameraProcessThreadStart_end;
  }

  setProcessState(CMD_PROCESS_START_PREPARE);
  mPThreadCmdsQueue.push(CMD_PROCESS_START);
  mPCmdsem.Signal();
  mStateComplete.Wait();
  // mProcessCond.signal();

cameraProcessThreadStart_end:
  // mProcessLock.unlock();

  if (mProcessState != CMD_PROCESS_START_DONE)
    err = -1;

  return err;
}

int AvmProcessAdapter::stopProcess() {
  Mutex::Autolock lock(mProcessLock);
  int err = 0;
  RSemaphore sem;

  // mProcessLock.lock();
  if (mProcessRuning == STA_PROCESS_STOP) {
    LOGW("%s(%d): process thread is already pause\n", __FUNCTION__, __LINE__);
    goto cameraProcessThreadPause_end;
  }
  setProcessState(CMD_PROCESS_STOP_PREPARE);

  mPThreadCmdsQueue.push(CMD_PROCESS_STOP);
  mPCmdsem.Signal();
  // mProcessCond.signal();
  mStateComplete.Wait();

cameraProcessThreadPause_end:

  if (mProcessState != CMD_PROCESS_STOP_DONE)
    err = -1;
  return err;
}

void AvmProcessAdapter::notifyNewFrame() {
  Mutex::Autolock lock(mProcessLock);
  // mProcessLock.lock();
  // send a frame to display
  if ((mProcessRuning == STA_PROCESS_RUNNING) &&
      (mProcessState != CMD_PROCESS_PAUSE_PREPARE) &&
      (mProcessState != CMD_PROCESS_PAUSE_DONE) &&
      (mProcessState != CMD_PROCESS_STOP_PREPARE) &&
      (mProcessState != CMD_PROCESS_STOP_DONE)) {
    if (mReadyInit) {
      mPThreadCmdsQueue.push(CMD_PROCESS_FRAME);
      mPCmdsem.Signal();
      mStateComplete.Wait();
      return;
    }
  }
  // mProcessLock.unlock();
}

void AvmProcessAdapter::notifyNewFrame(
    const std::vector<AHardwareBuffer *> &InputCam) {
  Mutex::Autolock lock(mProcessLock);
  // mProcessLock.lock();
  // send a frame to display
  if ((mProcessRuning == STA_PROCESS_RUNNING) &&
      (mProcessState != CMD_PROCESS_PAUSE_PREPARE) &&
      (mProcessState != CMD_PROCESS_PAUSE_DONE) &&
      (mProcessState != CMD_PROCESS_STOP_PREPARE) &&
      (mProcessState != CMD_PROCESS_STOP_DONE)) {
    if (mReadyInit) {
      if (InputCam.size() > 0) {
        mActivatedCam = InputCam;
      }
      mPThreadCmdsQueue.push(CMD_PROCESS_FRAME);
      mPCmdsem.Signal();
      mStateComplete.Wait();
      return;
    }
  }
  // mProcessLock.unlock();
}

int AvmProcessAdapter::setDisplay(int sw, int sh) {
  Mutex::Autolock lock(mProcessLock);
  int err = NO_ERROR;
  RSemaphore sem;
  // mProcessLock.lock();
  if (mProcessRuning != STA_PROCESS_RUNNING) {
    printf("%s(%d): display thread is already stop\n", __FUNCTION__, __LINE__);
    goto cameraProcessThreadStop_end;
  }
  setProcessState(CMD_PROCESS_SET_DISP_PREPARE);
  bit_dw = sw;
  bit_dh = sh;
  mPThreadCmdsQueue.push(CMD_PROCESS_DISPLAY);
  mPCmdsem.Signal();
  // mProcessCond.signal();
  mStateComplete.Wait();

cameraProcessThreadStop_end:
  // mProcessLock.unlock();
  if (mProcessState != CMD_PROCESS_SET_DISP_DONE)
    err = -1;
  return err;
}

int AvmProcessAdapter::pauseProcess() {
  Mutex::Autolock lock(mProcessLock);
  int err = NO_ERROR;
  RSemaphore sem;
  // mProcessLock.lock();
  if (mProcessRuning == STA_PROCESS_PAUSE) {
    LOGW("%s(%d): display thread is already stop\n", __FUNCTION__, __LINE__);
    goto cameraProcessThreadStop_end;
  }
  setProcessState(CMD_PROCESS_PAUSE_PREPARE);
  mPThreadCmdsQueue.push(CMD_PROCESS_PAUSE);
  mPCmdsem.Signal();
  // mProcessCond.signal();
  mStateComplete.Wait();

cameraProcessThreadStop_end:
  if (mProcessState != CMD_PROCESS_PAUSE_DONE)
    err = -1;
  return err;
}

void AvmProcessAdapter::AvmPreSplitData(int type, void *addr, int w, int h,
                                        int camId) {
  switch (type) {
  case rk_cmd_render_front_top_fish_eye:
  case rk_cmd_render_front_top_far:
  case rk_cmd_render_back_top_fish_eye:
  case rk_cmd_render_back_top_far:
  case rk_cmd_render_left_top_2D:
  case rk_cmd_render_right_top_2D:
    rk_avm_split_transfer_input_data(addr, w, h, camId);
    break;
  default:
    break;
  }
}

int AvmProcessAdapter::getProcessStatus(void) {
  Mutex::Autolock lock(mProcessLock);
  return mProcessRuning;
}

void AvmProcessAdapter::processThread() {
  while (mProcessRuning != STA_PROCESS_STOP) {

    mPCmdsem.Wait();

  process_receive_cmd:

    if (mPThreadCmdsQueue.empty() == false) {
      enum ProcessThreadCommands cur_cmd = mPThreadCmdsQueue.front();
      mPThreadCmdsQueue.pop();
      switch (cur_cmd) {
      case CMD_PROCESS_START: {
        LOGD("%s(%d): receive CMD_PROCESS_START\n", __FUNCTION__, __LINE__);
        initSurround3d();
        mProcessRuning = STA_PROCESS_RUNNING;
        setProcessState(CMD_PROCESS_START_DONE);
        mStateComplete.Signal();
        break;
      }
      case CMD_PROCESS_DISPLAY: {
        LOGD("%s(%d): receive CMD_PROCESS_DISPLAY\n", __FUNCTION__, __LINE__);
        rk_avm_set_display(bit_dw, bit_dh);
        disp_init = true;
        setProcessState(CMD_PROCESS_SET_DISP_DONE);
        mStateComplete.Signal();
        break;
      }
      case CMD_PROCESS_PAUSE: {
        LOGD("%s(%d): receive CMD_PROCESS_PAUSE\n", __FUNCTION__, __LINE__);
        mProcessRuning = STA_PROCESS_PAUSE;
        setProcessState(CMD_PROCESS_PAUSE_DONE);
        rk_avm_set_state_stop();
        mStateComplete.Signal();
        break;
      }

      case CMD_PROCESS_STOP: {
        deinitSurround3d();
        LOGD("%s(%d): receive CMD_PROCESS_STOP\n", __FUNCTION__, __LINE__);
        mProcessRuning = STA_PROCESS_STOP;
        setProcessState(CMD_PROCESS_STOP_DONE);
        mStateComplete.Signal();
        continue;
      }
      case CMD_PROCESS_FRAME: {
        if (mProcessRuning != STA_PROCESS_RUNNING)
          goto process_receive_cmd;
        setProcessState(CMD_PROCESS_FRAME_PROCESSING);

        for (auto k = 0; k < (int)myActivatedCamBuffer.size(); k++)
          rk_avm_render_update_android_hardwarebuffer((int)k,
                                                      myActivatedCamBuffer[k]);

        if (disp_init) {
          if (daddr) {
            rk_avm_gl_render(daddr, false);
          }
        }

        setProcessState(CMD_PROCESS_FRAME_DONE);
        mStateComplete.Signal();
        break;
      }

      default: {
        // LOG(ERROR) << __FUNCTION__ << __LINE__ <<  ":receive unknow command "
        // << cur_cmd;
        printf("%s%d::receive unknow command %d\n", __FUNCTION__, __LINE__,
               cur_cmd);
        break;
      }
      }
    }

    // mProcessLock.lock();
    if (mPThreadCmdsQueue.empty() == false) {
      // mProcessLock.unlock();
      goto process_receive_cmd;
    }
  }
}
} // namespace android
