/*
 * Copyright (C) 2023 Rockchip Electronics Co., Ltd.
 * Authors:
 *  kenvenis.chen <kenvenis.chen@rock-chips.com>
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

#include "RenderAvmRk.h"
#include "VideoTex.h"
#include "glError.h"
#include "shader.h"
#include "shader_simpleTex.h"
#include "shader_projectedTex.h"
#include "rk_avm_api.h"

#if RKAVM
#include "rk_avm_api.h"
#include <RockchipRga.h>
#include <RgaUtils.h>

#include <math/mat4.h>
#include <math/vec3.h>
#include <android/hardware/camera/device/3.2/ICameraDevice.h>
#include <android-base/logging.h>
#include <math/mat2.h>

#include <time.h>
#include <sys/time.h>

using aidl::android::hardware::automotive::evs::BufferDesc;
using aidl::android::hardware::automotive::evs::IEvsEnumerator;

android::AvmAlgoProcessAdapter *RenderAvmRk::mProcess = nullptr;

namespace android
{
AvmAlgoProcessAdapter::AvmAlgoProcessAdapter()
{
    mProcessRuning = -1;
    mProcessState = 0;
    mPCmdsem.Create();
    mStateComplete.Create();
    myBuffer.resize(4);
    //myActivatedCamBuffer.resize(4);
    mProcessThread = new ProcessThread(this);
    mProcessThread->run("ProcessThread", ANDROID_PRIORITY_DISPLAY);

}

AvmAlgoProcessAdapter::~AvmAlgoProcessAdapter()
{
    if (mProcessThread != NULL)
    {
        //stop thread and exit
        if (mProcessRuning != STA_PROCESS_STOP)
            stopProcess();
        mProcessThread->requestExitAndWait();
        mProcessThread.clear();
        //printf("processThread loop ending\n");
    }

    //printf("process thread addr %p mutex %p\n", this, &mProcessLock);
    myBuffer.clear();
    mPCmdsem.Release();
    mStateComplete.Release();
    //printf("processThread loop exit\n");
}

void AvmAlgoProcessAdapter::initSurround3d()
{
    LOG(DEBUG) << "initSurround3d====\n";

    char base_path[256];
    char avm_set[] = "/avm_settings.xml";
    memset(base_path, 0, 256);

    sprintf(base_path, "/system/etc/automotive/evs/RKAVM/AppDataRK/");

    property_get("persist.vendor.rockchip.evs.cam", dump_type, "NVP6188-AHD");

    strcat(base_path, dump_type);
    strcat(base_path, avm_set);

    LOG(DEBUG) << "dump_type: " << dump_type << " LOAD AVM SETTING XML " << base_path;

    if (mReadyInit == false)
    {
        //const char *init_path = "/system/etc/automotive/evs/RKAVM/AppDataRK/avm_settings.xml";
        rk_avm_set_extern_buf_enable(true);
        int ret_record = rk_avm_init(base_path, true);
        if (ret_record >= 0)
        {
            LOG(DEBUG) << "surround3d init success\n";
            mReadyInit = true;
        }
        else
        {
            LOG(ERROR) << "surround3d init fail\n";
        }
    }
}

void AvmAlgoProcessAdapter::deinitSurround3d()
{
    if (mReadyInit)
    {
        rk_avm_deinit();
        mReadyInit = false;
    }
}

void AvmAlgoProcessAdapter::setProcessState(int state)
{
    //Mutex::Autolock lock(mProcessLock);
    mProcessState = state;
}

int AvmAlgoProcessAdapter::getProcessState()
{
    Mutex::Autolock lock(mProcessLock);
    return mProcessState;
}

int AvmAlgoProcessAdapter::startProcess()
{
    Mutex::Autolock lock(mProcessLock);
    int err = NO_ERROR;
    Semaphore sem;

    if (mProcessRuning == STA_PROCESS_RUNNING)
    {
        printf("%s(%d): process thread is already run\n", __FUNCTION__, __LINE__);
        goto cameraProcessThreadStart_end;
    }

    setProcessState(CMD_PROCESS_START_PREPARE);
    mPThreadCmdsQueue.push(CMD_PROCESS_START);
    mPCmdsem.Signal();
    mStateComplete.Wait();

cameraProcessThreadStart_end:
    if (mProcessState != CMD_PROCESS_START_DONE)
        err = -1;

    return err;
}

int AvmAlgoProcessAdapter::stopProcess()
{
    Mutex::Autolock lock(mProcessLock);
    int err = 0;
    Semaphore sem;

    if (mProcessRuning == STA_PROCESS_STOP)
    {
        printf("%s(%d): process thread is already stop\n", __FUNCTION__, __LINE__);
        goto cameraProcessThreadPause_end;
    }
    setProcessState(CMD_PROCESS_STOP_PREPARE);

    mPThreadCmdsQueue.push(CMD_PROCESS_STOP);
    mPCmdsem.Signal();
    mStateComplete.Wait();
cameraProcessThreadPause_end:


    if (mProcessState != CMD_PROCESS_STOP_DONE)
        err = -1;
    return err;
}

void AvmAlgoProcessAdapter::notifyNewFrame()
{
    Mutex::Autolock lock(mProcessLock);

    //send a frame to display
    if ((mProcessRuning == STA_PROCESS_RUNNING)
            && (mProcessState != CMD_PROCESS_PAUSE_PREPARE)
            && (mProcessState != CMD_PROCESS_PAUSE_DONE)
            && (mProcessState != CMD_PROCESS_STOP_PREPARE)
            && (mProcessState != CMD_PROCESS_STOP_DONE))
    {
        if (mReadyInit)
        {
            mPThreadCmdsQueue.push(CMD_PROCESS_FRAME);
            mPCmdsem.Signal();
            mProcessCond.signal();
            mProcessLock.unlock();
            mStateComplete.Wait();
            return;
        }
    }

}

int AvmAlgoProcessAdapter::setDisplay(int sw, int sh)
{
    Mutex::Autolock lock(mProcessLock);
    int err = NO_ERROR;
    Semaphore sem;
    if (mProcessRuning != STA_PROCESS_RUNNING)
    {
        printf("%s(%d): display thread is already stop\n", __FUNCTION__, __LINE__);
        goto cameraProcessThreadStop_end;
    }
    setProcessState(CMD_PROCESS_SET_DISP_PREPARE);
    dw = sw;
    dh = sh;
    mPThreadCmdsQueue.push(CMD_PROCESS_DISPLAY);
    mPCmdsem.Signal();
    mStateComplete.Wait();

cameraProcessThreadStop_end:
    if (mProcessState != CMD_PROCESS_SET_DISP_DONE)
        err = -1;
    return err;
}

int AvmAlgoProcessAdapter::pauseProcess()
{
    Mutex::Autolock lock(mProcessLock);
    int err = NO_ERROR;
    Semaphore sem;
    // mProcessLock.lock();
    if (mProcessRuning == STA_PROCESS_PAUSE)
    {
        printf("%s(%d): display thread is already stop\n", __FUNCTION__, __LINE__);
        goto cameraProcessThreadStop_end;
    }
    setProcessState(CMD_PROCESS_PAUSE_PREPARE);
    mPThreadCmdsQueue.push(CMD_PROCESS_PAUSE);
    mPCmdsem.Signal();
    mStateComplete.Wait();

cameraProcessThreadStop_end:
    if (mProcessState != CMD_PROCESS_PAUSE_DONE)
        err = -1;
    return err;
}

int AvmAlgoProcessAdapter::getProcessStatus(void)
{
    Mutex::Autolock lock(mProcessLock);
    return mProcessRuning;
}

void AvmAlgoProcessAdapter::processThread()
{
    while (mProcessRuning != STA_PROCESS_STOP)
    {

        mPCmdsem.Wait();

process_receive_cmd:

        if (mPThreadCmdsQueue.empty() == false)
        {
            enum ProcessThreadCommands cur_cmd = mPThreadCmdsQueue.front();
            mPThreadCmdsQueue.pop();
            switch (cur_cmd)
            {
            case CMD_PROCESS_START:
            {
                //printf("%s(%d): receive CMD_PROCESS_START\n", __FUNCTION__, __LINE__);
                initSurround3d();
                mProcessRuning = STA_PROCESS_RUNNING;
                setProcessState(CMD_PROCESS_START_DONE);
                mStateComplete.Signal();
                break;
            }
            case CMD_PROCESS_DISPLAY:
            {
                //printf("%s(%d): receive CMD_PROCESS_START\n", __FUNCTION__, __LINE__);
                rk_avm_set_display(dw, dh);
                disp_init = true;
                setProcessState(CMD_PROCESS_SET_DISP_DONE);
                mStateComplete.Signal();
                break;
            }
            case CMD_PROCESS_PAUSE:
            {
                //printf("%s(%d): receive CMD_PROCESS_PAUSE\n", __FUNCTION__, __LINE__);
                mProcessRuning = STA_PROCESS_PAUSE;
                setProcessState(CMD_PROCESS_PAUSE_DONE);
                rk_avm_set_state_stop();
                mStateComplete.Signal();
                break;
            }

            case CMD_PROCESS_STOP:
            {
                deinitSurround3d();
                //printf("%s(%d): receive CMD_PROCESS_STOP\n", __FUNCTION__, __LINE__);
                mProcessRuning = STA_PROCESS_STOP;
                setProcessState(CMD_PROCESS_STOP_DONE);
                mStateComplete.Signal();
                continue;
            }
            case CMD_PROCESS_FRAME:
            {
                if (mProcessRuning != STA_PROCESS_RUNNING)
                    goto process_receive_cmd;
                setProcessState(CMD_PROCESS_FRAME_PROCESSING);

                mBufRdyCnt = true;
                for(unsigned int k = 0; k < myActivatedCamBuffer.size(); k++){
                    //LOG(ERROR) << "RKAVM Get input buffer handle is " << getNativeHandle(myBuffer[k]) << "buffer size " << myBuffer.size() << "index" << k;
                    if(myActivatedCamBuffer[k] && myActivatedCamBuffer[k]->initCheck() == android::OK)
                        mBufRdyCnt &= rk_avm_render_update_android_graphicbuffer_nomap((int)k, myActivatedCamBuffer[k]->handle, myActivatedCamBuffer[k]);
                    else
                        LOG(ERROR) << "RKAVM : processing frame k " << k << "no buffer init";
                }

                rk_avm_gl_render((void *)&tmpFd, true);

                setProcessState(CMD_PROCESS_FRAME_DONE);
                mStateComplete.Signal();
                relInputBuffer();
                break;
            }

            default:
            {
                LOG(ERROR) << __FUNCTION__ << __LINE__ <<  ":receive unknow command " << cur_cmd;
                break;
            }
            }
        }

        if (mPThreadCmdsQueue.empty() == false)
        {
            goto process_receive_cmd;
        }

    }
}
}

static int trans_camera_function_to_id(std::string info)
{

    if (info.find("front") != std::string::npos)
    {
        return 0;
    }
    if (info.find("right") != std::string::npos)
    {
        return 1;
    }
    if (info.find("reverse") != std::string::npos)
    {
        return 2;
    }
    if (info.find("left") != std::string::npos)
    {
        return 3;
    }

    return 0;
}

static std::string tran_surround_type_to_camera_index(int type)
{
    enum RK_AVM_CMD stype = (enum RK_AVM_CMD)type;
    std::string camera;
    switch (stype)
    {
    case rk_cmd_render_front_top_fish_eye:
        camera = "front";
        break;
    case rk_cmd_render_back_top_fish_eye:
        camera = "reverse";
        break;
    case rk_cmd_render_left_top_2D:
        camera = "left";
        break;
    case rk_cmd_render_right_top_2D:
        camera = "right";
        break;
    default:
        camera = "reverse";
        break;
    }

    return camera;
}

RenderAvmRk::RenderAvmRk(std::shared_ptr<IEvsEnumerator> enumerator,
                const std::vector<ConfigManager::CameraInfo>& camList,
                const ConfigManager &config) :
    mEnumerator(enumerator),
    mConfig(config)
{

    // Copy the list of cameras we're to employ into our local storage.  We'll create and
    // associate a streaming video texture when we are activated.
    mActiveCameras.reserve(camList.size());
    for (unsigned i = 0; i < camList.size(); i++)
    {
        mActiveCameras.emplace_back(camList[i]);
    }

    if (mProcess == nullptr)
    {
        mProcess = new android::AvmAlgoProcessAdapter();
    }

    mProcess->startProcess();
}

//We use LINUX DMA(rockchip:dmabufheap) buf for binding RKAVM render out
bool RenderAvmRk::AVMCreateImageKHR(int TexWidth, int TexHeight, int kfd, int texid, EGLImageKHR *Img)
{
#define fourcc_code(a, b, c, d) ((__u32)(a) | ((__u32)(b) << 8) | \
                 ((__u32)(c) << 16) | ((__u32)(d) << 24))
#define ALIGN(_v, _d) (((_v) + ((_d) - 1)) & ~((_d) - 1))

    int stride = ALIGN(TexWidth, 32) * 4;

    EGLint attr[] =
    {
        EGL_WIDTH, TexWidth,
        EGL_HEIGHT, TexHeight,
        EGL_LINUX_DRM_FOURCC_EXT, fourcc_code('A', 'B', '2', '4'),//DRM_FORMAT_NV21, //DRM_FORMAT_ABGR8888 DRM_FORMAT_NV12
        EGL_DMA_BUF_PLANE0_FD_EXT, kfd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
        EGL_NONE
    };

    *Img = eglCreateImageKHR(sDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, (EGLClientBuffer)NULL, attr);
    if (*Img == EGL_NO_IMAGE_KHR)
    {
        LOG(ERROR) << "rk-debug eglCreateImageKHR NULL ";
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, texid);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, *Img);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;

}

bool RenderAvmRk::AVMCreateEGLImage(buffer_handle_t handle, int w, int h, int texid, EGLImageKHR *Img)
{
    android::sp<android::GraphicBuffer> pGfxBuffer = new android::GraphicBuffer(handle,
                                                     android::GraphicBuffer::CLONE_HANDLE,
                                                     w,
                                                     h,
                                                     HAL_PIXEL_FORMAT_RGBA_8888,
                                                     1,//pDesc->layers,
                                                     GRALLOC_USAGE_HW_TEXTURE,
                                                     w);
    if (pGfxBuffer.get() == nullptr) {
        LOG(ERROR) << "Failed to allocate GraphicBuffer to wrap image handle";
        return false;
    }

    // Get a GL compatible reference to the graphics buffer we've been given
    EGLint eglImageAttributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
    EGLClientBuffer clientBuf = static_cast<EGLClientBuffer>(pGfxBuffer->getNativeBuffer());
    *Img = eglCreateImageKHR(sDisplay, EGL_NO_CONTEXT,
                                  EGL_NATIVE_BUFFER_ANDROID, clientBuf,
                                  eglImageAttributes);
    if (*Img == EGL_NO_IMAGE_KHR) {
        const char *msg = getEGLError();
        LOG(ERROR) << __FUNCTION__ << "(" << __LINE__ << "): " << "Error creating EGLImage: " << msg;
    } else {
        // Update the texture handle we already created to refer to this gralloc buffer
        glBindTexture(GL_TEXTURE_2D, texid);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, static_cast<GLeglImageOES>(*Img));

        // Initialize the sampling properties (it seems the sample may not work if this isn't done)
        // The user of this texture may very well want to set their own filtering, but we're going
        // to pay the (minor) price of setting this up for them to avoid the dreaded "black image"
        // if they forget.
        // TODO:  Can we do this once for the texture ID rather than ever refresh?
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    return true;
}

bool RenderAvmRk::activate()
{
    std::lock_guard<std::mutex> lg(sMutex);
    // Ensure GL is ready to go...
    if (!prepareGL())
    {
        LOG(ERROR) << "Error initializing GL";
        return false;
    }

    glGenTextures(1, &myTexId);
    glGenTextures(1, &myAdjId);

    // Load our shader programs
    myProgramHandle = buildShaderProgram(vtxShader_simpleTexture,
                                         pixShader_simpleTexture,
                                         "simpleTexture");
    if (!myProgramHandle)
    {
        LOG(ERROR) << "Failed to build shader program";
        return false;
    }

    // Set up streaming video textures for our associated cameras
    for (auto &&cam : mActiveCameras)
    {
        cam.tex.reset(createVideoTexture(mEnumerator,
                                         cam.info.cameraId.c_str(),
                                         nullptr,
                                         sDisplay));
        if (!cam.tex)
        {
            LOG(ERROR) << "Failed to set up video texture for " << cam.info.cameraId
                       << " (" << cam.info.function << ")";
        }
    }

    return true;
}

void RenderAvmRk::AVMSetSurroundType(enum RK_AVM_CMD cmd)
{
    std::lock_guard<std::mutex> lg(sMutex);
    rk_avm_rev_cmd_proc(cmd, nullptr);
}

void RenderAvmRk::deactivate()
{
    // Release our video textures
    // We can't hold onto it because some other Render object might need the same camera
    // TODO(b/131492626):  investigate whether sharing video textures can save
    // the time.
    std::lock_guard<std::mutex> lg(sMutex);
    if (mProcess != nullptr)
    {
        mProcess->pauseProcess();
    }

    for (auto &&cam : mActiveCameras)
    {
        cam.tex = nullptr;
    }

    if (myRevImg != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(sDisplay, myRevImg);
        myRevImg = EGL_NO_IMAGE_KHR;
    }

    if (myImg != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(sDisplay, myImg);
        myImg = EGL_NO_IMAGE_KHR;
    }

    if (myTopImg != EGL_NO_IMAGE_KHR) {
        eglDestroyImageKHR(sDisplay, myTopImg);
        myTopImg = EGL_NO_IMAGE_KHR;
    }

    if (myTexId > 0)
    {
        glDeleteTextures(1, &myTexId);

    }
    myTexId = 0;

    if (myAdjId > 0)
    {
        glDeleteTextures(1, &myAdjId);

    }
    myAdjId = 0;

    if (myProgramHandle != 0)
    {
        glDeleteProgram(myProgramHandle);
    }
    myProgramHandle = 0;

    if (mProcess != nullptr)
    {
        mProcess->stopProcess();
        delete mProcess;
    }
    mProcess = nullptr;
}

bool RenderAvmRk::AVMRenderAdjust()
{
    glUseProgram(myProgramHandle);
    GLint loc = glGetUniformLocation(myProgramHandle, "cameraMat");
    if (loc < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'cameraMat'";
        return false;
    }
    else
    {
        const android::mat4 identityMatrix;
        glUniformMatrix4fv(loc, 1, false, identityMatrix.asArray());
    }

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, myTexId);

    GLint sampler = glGetUniformLocation(myProgramHandle, "tex");
    if (sampler < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'tex'";
        return false;
    }
    else
    {
        glUniform1i(sampler, 0);
    }


    // Draw a rectangle on the screen
    GLfloat vertsCarPos[] = { -1.0f,  1.0f, 0.0f,   // left top in window space
                              1.0f,  1.0f, 0.0f,   // right top
                              -1.0f, -1.0f, 0.0f,   // left bottom
                              1.0f, -1.0f, 0.0f    // right bottom
                            };

    GLfloat vertsCarTex[] = { 0.0f, 0.0f,
                              1.0f, 0.0f,
                              0.0f, 1.0f,
                              1.0f, 1.0f
                            };

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertsCarPos);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vertsCarTex);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

   // detachRenderTarget();

    // Wait for the rendering to finish
    glFinish();

    return true;
}

bool RenderAvmRk::AVMRenderTopFishEye(GLuint texId)
{
    glUseProgram(myProgramHandle);
    GLint loc = glGetUniformLocation(myProgramHandle, "cameraMat");
    if (loc < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'cameraMat'";
        return false;
    }
    else
    {
        const android::mat4 identityMatrix;
        glUniformMatrix4fv(loc, 1, false, identityMatrix.asArray());
    }

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, myTexId);

    GLint sampler = glGetUniformLocation(myProgramHandle, "tex");
    if (sampler < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'tex'";
        return false;
    }
    else
    {
        glUniform1i(sampler, 0);
    }


    // Draw a rectangle on the screen
    GLfloat vertsCarPos[] = { -1.0,  1.0, 0.0f,   // left top in window space
                              -0.3125,  1.0, 0.0f,   // right top
                              -1.0, -1.0, 0.0f,   // left bottom
                              -0.3125, -1.0, 0.0f    // right bottom
                            };

    GLfloat vertsCarTex[] = { 0.325625f, 0.0f,
                              0.674375f, 0.0f,
                              0.325625f, 1.0f,
                              0.674375f, 1.0f
                            };

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertsCarPos);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vertsCarTex);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);

    sampler = glGetUniformLocation(myProgramHandle, "tex");
    if (sampler < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'tex'";
        return false;
    }
    else
    {
        glUniform1i(sampler, 0);
    }

    // Draw a rectangle on the screen
    GLfloat vertsCarPosT[] = { -0.3125f, 1.0f, 0.0f,   // left top in window space
                               1.0f,  1.0f, 0.0f,   // right top
                               -0.3125f, -1.0f, 0.0f,   // left bottom
                               1.0f, -1.0f, 0.0f    // right bottom
                             };

    GLfloat vertsCarTexT[] = { 0.174375f, 1.0f,
                               0.825625f, 1.0f,
                               0.174375f, 0.0f,
                               0.825625f, 0.0f
                             };

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertsCarPosT);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vertsCarTexT);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    // Now that everything is submitted, release our hold on the texture resource
    //detachRenderTarget();

    // Wait for the rendering to finish
    glFinish();
    //detachRenderTarget();

    return true;
}

bool RenderAvmRk::AVMRenderTopAdjust()
{
    glUseProgram(myProgramHandle);
    GLint loc = glGetUniformLocation(myProgramHandle, "cameraMat");
    if (loc < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'cameraMat'";
        return false;
    }
    else
    {
        const android::mat4 identityMatrix;
        glUniformMatrix4fv(loc, 1, false, identityMatrix.asArray());
    }

    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, myTexId);

    GLint sampler = glGetUniformLocation(myProgramHandle, "tex");
    if (sampler < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'tex'";
        return false;
    }
    else
    {
        glUniform1i(sampler, 0);
    }


    // Draw a rectangle on the screen
    GLfloat vertsCarPos[] = { -1.0,  1.0, 0.0f,   // left top in window space
                              -0.3125,  1.0, 0.0f,   // right top
                              -1.0, -1.0, 0.0f,   // left bottom
                              -0.3125, -1.0, 0.0f    // right bottom
                            };

    GLfloat vertsCarTex[] = { 0.325625f, 0.0f,
                              0.674375f, 0.0f,
                              0.325625f, 1.0f,
                              0.674375f, 1.0f
                            };

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertsCarPos);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vertsCarTex);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, myAdjId);

    sampler = glGetUniformLocation(myProgramHandle, "tex");
    if (sampler < 0)
    {
        LOG(ERROR) << "Couldn't set shader parameter 'tex'";
        return false;
    }
    else
    {
        glUniform1i(sampler, 0);
    }

    // Draw a rectangle on the screen
    GLfloat vertsCarPosT[] = { -0.3125f, 1.0f, 0.0f,   // left top in window space
                               1.0f,  1.0f, 0.0f,   // right top
                               -0.3125f, -1.0f, 0.0f,   // left bottom
                               1.0f, -1.0f, 0.0f    // right bottom
                             };

    GLfloat vertsCarTexT[] = { 0.174375f, 0.0f,
                               0.825625f, 0.0f,
                               0.174375f, 1.0f,
                               0.825625f, 1.0f
                             };

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertsCarPosT);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, vertsCarTexT);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    // Now that everything is submitted, release our hold on the texture resource
    //detachRenderTarget();

    // Wait for the rendering to finish
    glFinish();
    //detachRenderTarget();

    return true;
}

bool RenderAvmRk::drawFrame(const BufferDesc &tgtBuffer)
{
    std::lock_guard<std::mutex> lg(sMutex);

    bool flag, create_egl_img;
    // Tell GL to render to the given buffer
    if (!attachRenderTarget(tgtBuffer))
    {
        LOG(ERROR) << "Failed to attached render target";
        return false;
    }

    if (mProcess && !mProcess->getDispInitState())
    {
        mProcess->setDisplay(sWidth, sHeight);
    }

    int surround_type = rk_avm_get_surround_type();

    create_egl_img = surround_type < 16 ? true : false;

    for (auto &&cam : mActiveCameras)
    {
        int camId = trans_camera_function_to_id(cam.info.function);//get camID
        if (cam.tex)
        {
            cam.tex->refreshGraphicBuffer();
            if(cam.tex->myBuffer.get() != nullptr)
            {
                mProcess->setInputBuffer(cam.tex->myBuffer, camId);
            }
        }
    }

    if (mProcess != nullptr) mProcess->notifyNewFrame();

    if (myImg == EGL_NO_IMAGE_KHR) {
        AVMCreateImageKHR(rk_avm_get_render_out_width(),
            rk_avm_get_render_out_height(), rk_avm_get_fbo_fd(0), myTexId, &myImg);
    }

    if (RK_AVM_CMD(surround_type) == rk_cmd_render_back_wide ||
         RK_AVM_CMD(surround_type) == rk_cmd_render_3d_view_adjuest)
    {
        flag = AVMRenderAdjust();
    }
    else if (RK_AVM_CMD(surround_type) == rk_cmd_render_top_3d_view_adjuest)
    {
        if (myTopImg == EGL_NO_IMAGE_KHR) {
            AVMCreateImageKHR(rk_avm_get_render_out_width(),
                rk_avm_get_render_out_height(), rk_avm_get_fbo_fd(1), myAdjId, &myTopImg);
        }

        flag = AVMRenderTopAdjust();

    }
    else
    {
        std::string camera_name = tran_surround_type_to_camera_index(surround_type);

        GLuint curTexId = 0;

        for (auto &&cam : mActiveCameras)
        {
            if (cam.info.function.find(camera_name) != std::string::npos)
            {
                if (cam.tex)
                {
                    curTexId = cam.tex->glId();
                }
                break;
            }
        }

        flag = AVMRenderTopFishEye(curTexId);

    }
    detachRenderTarget();

    // Wait for the rendering to finish
    glFinish();
    detachRenderTarget();
    return flag;
}
#endif