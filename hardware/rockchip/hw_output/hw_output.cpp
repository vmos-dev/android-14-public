/*
 * Copyright 2014 The Android Open Source Project
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
#define LOG_TAG "hw_output"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <drm_fourcc.h>

#include <string>
#include <map>
#include <vector>
#include <iostream>

#include <cutils/native_handle.h>
#include <cutils/properties.h>
#include <log/log.h>

#include "hw_output.h"
#include "baseparameter_api.h"
#include "hw_types.h"

#include "rkdisplay/drmresources.h"
#include "rkdisplay/drmmode.h"
#include "rkdisplay/drmconnector.h"
#include "rkdisplay/drmgamma.h"
#include "rkdisplay/drmhdcp.h"
#include "rockchip/baseparameter.h"
#include "rockchip/autopq.h"
#include "j2s/j2s.h"
#include "j2s/pq_setting_config.h"
#include "j2s/cJSON_Utils.h"

#ifdef USE_HWC_PROXY_SERVICE
#include "rk_hwc_proxy_client.h"
using ::aidl::rockchip::hwc::proxy::aidl::RkHwcProxyClient;
#endif

using namespace android;

#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARN 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3
#define UN_USED(x) (void)(x)
#define PROPERTY_PQ_MODE "persist.vendor.tvinput.rkpq.mode"
#define PROPERTY_RKPQ_ENABLE "persist.vendor.tvinput.rkpq.enable"
#define PROPERTY_RKPQ_TIMELINE "vendor.tvinput.rkpq.timeline"
#define PROPERTY_TV_INPUT_HDMIIN "vendor.tvinput.rk.hdmiin"
#define PROPERTY_BOARD_PLATFORM "ro.board.platform"
#define PQ_SETTING_CONFIG_JSON_PATH "/vendor/etc/pq_setting_config.json"
#define PQ_CONFIG_PATH "/data/vendor/rkalgo/pq_config.json"
#define AIPQ_CONFIG_PATH "/data/vendor/rkalgo/aipq_config.json"
static int mHwcVersion = 0;
static unsigned short mBaseparameterMajorVersion = 0;
static unsigned short mBaseparameterMinorVersion = 0;
std::map<int,DrmConnector*> mGlobalConns;
rk_platform mRkPlatform;
static int dbgLevel = 3;

#ifdef USE_HWC_PROXY_SERVICE
RkHwcProxyClient mClient;
#endif

/*****************************************************************************/

typedef struct hw_output_private {
    hw_output_device_t device;

    // Callback related data
    void* callback_data;
    DrmResources *drm_;
    DrmConnector* primary;
    DrmConnector* extend;
    BaseParameter* mBaseParmeter;
    struct lut_info* mlut;
}hw_output_private_t;

static bcsh_cfg_type bcsh_mode[] =
{
     //mode     brightness  contrast    saturation     hue
    {"DYNAMIC",   100,         101,        102,        103},
    {"STANDARD",  200,         201,        202,        203},
    {"MILD",      300,         301,        302,        303},
    {"USER",      400,         401,        402,        403},
};


static white_balance_cfg_type white_balance_mode[] =
{
     //mode       rgain        ggain       bgain
    {"COLD",      200,         201,        202},
    {"STANDARD",  300,         301,        302},
    {"WARM",      400,         401,        402},
    {"USER",      500,         501,        502},
};

static int hw_output_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device);

static struct hw_module_methods_t hw_output_module_methods = {
    .open = hw_output_device_open
};

hw_output_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = HW_OUTPUT_HARDWARE_MODULE_ID,
        .name = "Sample hw output module",
        .author = "The Android Open Source Project",
        .methods = &hw_output_module_methods,
        .dso = NULL,
        .reserved = {0},
    }
};

static bool builtInHdmi(int type){
    return type == DRM_MODE_CONNECTOR_HDMIA || type == DRM_MODE_CONNECTOR_HDMIB;
}

static void checkBcshInfo(uint32_t* mBcsh)
{
    if (mBcsh[0] < 0)
        mBcsh[0] = 0;
    else if (mBcsh[0] > 100)
        mBcsh[0] = 100;

    if (mBcsh[1] < 0)
        mBcsh[1] = 0;
    else if (mBcsh[1] > 100)
        mBcsh[1] = 100;

    if (mBcsh[2] < 0)
        mBcsh[2] = 0;
    else if (mBcsh[2] > 100)
        mBcsh[2] = 100;

    if (mBcsh[3] < 0)
        mBcsh[3] = 0;
    else if (mBcsh[3] > 100)
        mBcsh[3] = 100;
}

static void updateTimeline()
{
    std::string strTimeline;
    char property[100];
    int timeline = property_get_int32("vendor.display.timeline", 1);

    timeline++;
    strTimeline = std::to_string(timeline);
    property_set("vendor.display.timeline", strTimeline.c_str());

    property_get("vendor.hw_output.debug", property, "3");
    dbgLevel = atoi(property);
}

static void updatePQTimeline()
{
    std::string strTimeline;
    char property[100];
    int timeline = property_get_int32(PROPERTY_RKPQ_TIMELINE, 1);
    timeline++;
    strTimeline = std::to_string(timeline);
    property_set(PROPERTY_RKPQ_TIMELINE, strTimeline.c_str());
}


DrmConnector* getValidDrmConnector(hw_output_private_t *priv, int dpy)
{
    std::map<int, DrmConnector*> mConns = mGlobalConns;
    std::map<int, DrmConnector*>::iterator iter;
    DrmConnector* mConnector = nullptr;
    (void)priv;

    iter = mConns.find(dpy);
    if (iter != mConns.end()) {
        mConnector = iter->second;
    }

    return mConnector;
}

static std::string getPropertySuffix(hw_output_private_t *priv, std::string header, int dpy)
{
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    std::string suffix;

    suffix = header;
    if (mHwcVersion == 2) {
        if (conn != nullptr) {
            const char* connTypeStr = priv->drm_->connector_type_str(conn->get_type());
            int id = conn->connector_id();
            suffix += connTypeStr;
            suffix += '-';
            ALOGD("id=%d", id);
            suffix += std::to_string(id);
        }
    } else {
        std::string propertyStr = "vendor.hwc.device.primary";
        char mainProperty[100];
        property_get(propertyStr.c_str(), mainProperty, "");
        ALOGE("mainProperty = %s\n", mainProperty);
        if ((conn->get_type() == DRM_MODE_CONNECTOR_HDMIA && strstr(mainProperty, "HDMI-A"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_HDMIB && strstr(mainProperty, "HDMI-B"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_TV && strstr(mainProperty, "TV"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_VGA && strstr(mainProperty, "VGA"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_DisplayPort && strstr(mainProperty, "DP"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_eDP && strstr(mainProperty, "eDP"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_VIRTUAL && strstr(mainProperty, "Virtual"))
            || (conn->get_type() == DRM_MODE_CONNECTOR_DSI && strstr(mainProperty, "DSI")))
            suffix += "main";
        else
            suffix += "aux";
    }
    ALOGD("suffix=%s", suffix.c_str());
    return suffix;
}

static int findSuitableInfoSlot(struct disp_info* info, int type, int id)
{
    int found=0;
    for (int i=0;i<4;i++) {
        if (info->screen_info[i].type !=0 && info->screen_info[i].type == type &&
            info->screen_info[i].id == id) {
            found = i;
            break;
        } else if (info->screen_info[i].type !=0 && found == false){
            found++;
        }
    }
    if (found == -1) {
        found = 0;
        ALOGD("noting saved, used the first slot");
    }
    ALOGD("findSuitableInfoSlot: %d type=%d", found, type);
    return found;
}

static bool getResolutionInfo(hw_output_private_t *priv, int dpy, char* resolution)
{
    drmModePropertyBlobPtr blob;
    drmModeObjectPropertiesPtr props;
    DrmConnector* mCurConnector = NULL;
    DrmCrtc *crtc = NULL;
    struct drm_mode_modeinfo *drm_mode;
    struct disp_info info;
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    int value;
    bool found = false;

    mCurConnector = getValidDrmConnector(priv, dpy);
    if (mCurConnector == nullptr) {
        sprintf(resolution, "%s", "Auto");
        return false;
    }

    if (mBaseParmeter && mBaseParmeter->have_baseparameter()) {
        if (mCurConnector)
            mBaseParmeter->get_disp_info(mCurConnector->get_type(), mCurConnector->connector_id(), &info);
        int slot = findSuitableInfoSlot(&info, mCurConnector->get_type(), mCurConnector->connector_id());
        if (!info.screen_info[slot].resolution.hdisplay ||
            !info.screen_info[slot].resolution.clock ||
            !info.screen_info[slot].resolution.vdisplay) {
            sprintf(resolution, "%s", "Auto");
            return false;
        }
    }

    if (mCurConnector != NULL) {
        crtc = priv->drm_->GetCrtcFromConnector(mCurConnector);
        if (crtc == NULL) {
            return false;
        }
        props = drmModeObjectGetProperties(priv->drm_->fd(), crtc->id(), DRM_MODE_OBJECT_CRTC);
        for (int i = 0; !found && (size_t)i < props->count_props; ++i) {
            drmModePropertyPtr p = drmModeGetProperty(priv->drm_->fd(), props->props[i]);
            if (!strcmp(p->name, "MODE_ID")) {
                found = true;
                if (!drm_property_type_is(p, DRM_MODE_PROP_BLOB)) {
                    ALOGE("%s:line=%d,is not blob",__FUNCTION__,__LINE__);
                    drmModeFreeProperty(p);
                    drmModeFreeObjectProperties(props);
                    return false;
                }
                if (!p->count_blobs)
                    value = props->prop_values[i];
                else
                    value = p->blob_ids[0];
                blob = drmModeGetPropertyBlob(priv->drm_->fd(), value);
                if (!blob) {
                    ALOGE("%s:line=%d, blob is null",__FUNCTION__,__LINE__);
                    drmModeFreeProperty(p);
                    drmModeFreeObjectProperties(props);
                    return false;
                }

                float vfresh;
                drm_mode = (struct drm_mode_modeinfo *)blob->data;
                if (drm_mode->flags & DRM_MODE_FLAG_INTERLACE)
                    vfresh = drm_mode->clock *2/ (float)(drm_mode->vtotal * drm_mode->htotal) * 1000.0f;
                else
                    vfresh = drm_mode->clock / (float)(drm_mode->vtotal * drm_mode->htotal) * 1000.0f;
                ALOGD("nativeGetCurMode: crtc_id=%d clock=%d w=%d %d %d %d %d %d flag=0x%x vfresh %.2f drm.vrefresh=%.2f",
                        crtc->id(), drm_mode->clock, drm_mode->htotal, drm_mode->hsync_start,
                        drm_mode->hsync_end, drm_mode->vtotal, drm_mode->vsync_start, drm_mode->vsync_end, drm_mode->flags,
                        vfresh, (float)drm_mode->vrefresh);
                sprintf(resolution, "%dx%d@%.2f-%d-%d-%d-%d-%d-%d-%x-%d", drm_mode->hdisplay, drm_mode->vdisplay, vfresh,
                        drm_mode->hsync_start, drm_mode->hsync_end, drm_mode->htotal,
                        drm_mode->vsync_start, drm_mode->vsync_end, drm_mode->vtotal,
                        (drm_mode->flags&0xFFFF), drm_mode->clock);
                drmModeFreePropertyBlob(blob);
            }
            drmModeFreeProperty(p);
        }
        drmModeFreeObjectProperties(props);
    } else {
        return false;
    }

    return true;
}

static void updateConnectors(hw_output_private_t *priv){
    if (priv->drm_->connectors().size() == 2) {
        bool foundHdmi=false;
        int cnt=0,crtcId1=0,crtcId2=0;
        for (auto &conn : priv->drm_->connectors()) {
            if (cnt == 0 && priv->drm_->GetCrtcFromConnector(conn.get())) {
                ALOGD("encoderId1: %d", conn->encoder()->id());
                crtcId1 = priv->drm_->GetCrtcFromConnector(conn.get())->id();
            } else if (priv->drm_->GetCrtcFromConnector(conn.get())){
                ALOGD("encoderId2: %d", conn->encoder()->id());
                crtcId2 = priv->drm_->GetCrtcFromConnector(conn.get())->id();
            }

            if (builtInHdmi(conn->get_type()))
                foundHdmi=true;
            cnt++;
        }
        ALOGD("crtc: %d %d foundHdmi %d 2222", crtcId1, crtcId2, foundHdmi);
        char property[PROPERTY_VALUE_MAX];
        property_get("vendor.hwc.device.primary", property, "null");
        if (crtcId1 == crtcId2 && foundHdmi && strstr(property, "HDMI-A") == NULL) {
            for (auto &conn : priv->drm_->connectors()) {
                if (builtInHdmi(conn->get_type()) && conn->state() == DRM_MODE_CONNECTED) {
                    priv->extend = conn.get();
                    conn->set_display(1);
                } else if(!builtInHdmi(conn->get_type()) && conn->state() == DRM_MODE_CONNECTED) {
                    priv->primary = conn.get();
                    conn->set_display(0);
                }
            }
        }
    }
}

static int hw_output_update_disp_header(struct hw_output_device *dev)
{
    bool found = false;
    int ret = 0, firstEmptyHeader = -1;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    struct disp_header * headers = (disp_header *)malloc(sizeof(disp_header) * 8);
    for (auto &conn : priv->drm_->connectors()) {
        if(conn->state() == DRM_MODE_CONNECTED){
            found = false;
            firstEmptyHeader = -1;
            ret = mBaseParmeter->get_all_disp_header(headers);
            if(ret != 0) {
                break;
            }
            for(int i = 0; i < 8; i++){
                if(headers[i].connector_type == conn->get_type() && headers[i].connector_id == conn->connector_id()){
                    found = true;
                }
                if(firstEmptyHeader == -1 && headers[i].connector_type == 0 && headers[i].connector_id == 0){
                    firstEmptyHeader = i;
                }
            }
            if(!found){
                ret = mBaseParmeter->set_disp_header(firstEmptyHeader, conn->get_type(), conn->connector_id());
            }
        }
    }
    free(headers);
    return ret;
}

/*****************************************************************************/
static void hw_output_save_config(struct hw_output_device* dev){
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    if (priv->mBaseParmeter)
        priv->mBaseParmeter->saveConfig();
}

static void hw_output_hotplug_update(struct hw_output_device* dev){
    hw_output_private_t* priv = (hw_output_private_t*)dev;

    DrmConnector *mextend = NULL;
    DrmConnector *mprimary = NULL;
    int dpy = 0;
    int index = 0;

    for (auto &conn : priv->drm_->connectors()) {
        drmModeConnection old_state = conn->state();

        conn->UpdateModes();

        drmModeConnection cur_state = conn->state();
        ALOGD("old_state %d cur_state %d conn->get_type() %d", old_state, cur_state, conn->get_type());

        if (cur_state == old_state) {
            index++;
            continue;
        }
        ALOGI("%s event  for connector %u\n",
                cur_state == DRM_MODE_CONNECTED ? "Plug" : "Unplug", conn->id());

        for (DrmEncoder *enc : conn->possible_encoders()) {
            for (DrmCrtc *crtc : enc->possible_crtcs()) {
                if(conn->state() == DRM_MODE_CONNECTED) {
                    enc->set_crtc(crtc);
                    conn->set_encoder(enc);
                } else {
                    enc->set_crtc(NULL);
                    conn->set_encoder(NULL);
                }
            }
        }
        mGlobalConns[index] = conn.get();
        if (cur_state == DRM_MODE_CONNECTED) {
            if (conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT) {
                mextend = conn.get();
            } else if (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT) {
                mprimary = conn.get();
            }
        }
        index++;
    }

    /*
     * status changed?
     */
    priv->drm_->DisplayChanged();
    dpy = mGlobalConns.size();

    DrmConnector *old_primary = priv->drm_->GetConnectorFromType(HWC_DISPLAY_PRIMARY);
    mprimary = mprimary ? mprimary : old_primary;
    if (!mprimary || mprimary->state() != DRM_MODE_CONNECTED) {
        mprimary = NULL;
        for (auto &conn : priv->drm_->connectors()) {
            if (!(conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT))
                continue;
            if (conn->state() == DRM_MODE_CONNECTED) {
                mprimary = conn.get();
                //mGlobalConns[HWC_DISPLAY_PRIMARY] = conn.get();
                break;
            }
        }
    }

    if (!mprimary) {
        ALOGE("%s %d Failed to find primary display\n", __FUNCTION__, __LINE__);
        //return;
    }
    if (mprimary && mprimary != old_primary) {
        priv->drm_->SetPrimaryDisplay(mprimary);
    }

    DrmConnector *old_extend = priv->drm_->GetConnectorFromType(HWC_DISPLAY_EXTERNAL);
    mextend = mextend ? mextend : old_extend;
    dpy = 1;
    if (!mextend || mextend->state() != DRM_MODE_CONNECTED) {
        mextend = NULL;
        for (auto &conn : priv->drm_->connectors()) {
            if (!(conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT))
                continue;
            if (mprimary && conn->id() == mprimary->id())
                continue;
            if (conn->state() == DRM_MODE_CONNECTED) {
                mextend = conn.get();
                //mGlobalConns[dpy] = conn.get();
                break;
            }
        }
    }
    priv->drm_->SetExtendDisplay(mextend);
    priv->drm_->DisplayChanged();
    priv->drm_->UpdateDisplayRoute();
    priv->drm_->ClearDisplay();

    updateConnectors(priv);
    hw_output_update_disp_header(dev);
}

static int hw_output_init_baseparameter(BaseParameter** mBaseParmeter)
{
    char property[100];
    property_get("vendor.ghwc.version", property, NULL);
    if (strstr(property, "HWC2") != NULL) {
        *mBaseParmeter = new BaseParameterV2();
        mHwcVersion = 2;
    } else {
        *mBaseParmeter = new BaseParameterV1();
        mHwcVersion = 1;
    }
	return 0;
}

static int hw_output_initialize(struct hw_output_device* dev, void* data)
{
#ifdef USE_HWC_PROXY_SERVICE
    ALOGD("use hwc proxy service !!!");
    mClient.setup();
#endif
    hw_output_private_t* priv = (hw_output_private_t*)dev;

    priv->drm_ = NULL;
    priv->primary = NULL;
    priv->extend = NULL;
    priv->mlut = NULL;
    priv->callback_data = data;
    hw_output_init_baseparameter(&priv->mBaseParmeter);
    priv->mBaseParmeter->get_version(&mBaseparameterMajorVersion, &mBaseparameterMinorVersion);
    ALOGD("baseparameter version: %d %d", mBaseparameterMajorVersion, mBaseparameterMinorVersion);

    char property[100];
    property_get(PROPERTY_BOARD_PLATFORM, property, NULL);
    if (strstr(property, "rk3588") != NULL) {
        mRkPlatform = HW_OUTPUT_RK3588;
    }   else if (strstr(property, "rk3576") != NULL) {
        mRkPlatform = HW_OUTPUT_RK3576;
    } else {
        mRkPlatform = HW_OUTPUT_OTHERS;
    }
    if (priv->drm_ == NULL) {
        priv->drm_ = new DrmResources();
        priv->drm_->Init();
        ALOGD("nativeInit: ");
        if (mHwcVersion >= 2) {
            int id=0;
            for (auto &conn : priv->drm_->connectors())
                mGlobalConns.insert(std::make_pair(id++, conn.get()));
        } else {
            int id=0;
            for (auto &conn : priv->drm_->connectors()) {
                // 同Hwc2相同处理方式，不区分主副屏
                mGlobalConns.insert(std::make_pair(id++, conn.get()));
                /* if (conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT)
                    mGlobalConns.insert(std::make_pair(HWC_DISPLAY_PRIMARY, conn.get()));
                else
                    mGlobalConns.insert(std::make_pair(id++, conn.get())); */
            }
        }
        priv->mBaseParmeter->set_drm_connectors(mGlobalConns);
        hw_output_hotplug_update(dev);
        if (priv->primary == NULL) {
            for (auto &conn : priv->drm_->connectors()) {
                if ((conn->possible_displays() & HWC_DISPLAY_PRIMARY_BIT)) {
                    // mGlobalConns[HWC_DISPLAY_PRIMARY] = conn.get();
                }
                if ((conn->possible_displays() & HWC_DISPLAY_EXTERNAL_BIT) && conn->state() == DRM_MODE_CONNECTED) {
                    priv->drm_->SetExtendDisplay(conn.get());
                    priv->extend = conn.get();
                }
            }
        }
        ALOGD("primary: %p extend: %p ", priv->primary, priv->extend);
    }
    int ret = remove(PQ_CONFIG_PATH);
    ALOGD("remove %s ret %d", PQ_CONFIG_PATH, ret);

    return 0;
}

/*****************************************************************************/

static int hw_output_set_mode(struct hw_output_device* dev, int dpy, const char* mode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    char property[PROPERTY_VALUE_MAX] = {0};
    std::string propertyStr;

    propertyStr = getPropertySuffix(priv, "persist.vendor.resolution.", dpy);

    ALOGD("nativeSetMode %s display %d", mode, dpy);

    if (strcmp(mode, property) !=0) {
        property_set(propertyStr.c_str(), mode);
        updateTimeline();
        struct disp_info info;
        float vfresh=0.0f;
        int slot = 0;

        mBaseParameter->get_disp_info(conn->get_type(), conn->connector_id(), &info);
        slot = findSuitableInfoSlot(&info, conn->get_type(), conn->connector_id());
        info.screen_info[slot].type = conn->get_type();
        info.screen_info[slot].id = conn->connector_id();
        if (strncmp(mode, "Auto", 4) != 0 && strncmp(mode, "0x0p0-0", 7) !=0) {
            sscanf(mode,"%dx%d@%f-%d-%d-%d-%d-%d-%d-%x-%d",
                    &info.screen_info[slot].resolution.hdisplay, &info.screen_info[slot].resolution.vdisplay,
                    &vfresh, &info.screen_info[slot].resolution.hsync_start,&info.screen_info[slot].resolution.hsync_end,
                    &info.screen_info[slot].resolution.htotal,&info.screen_info[slot].resolution.vsync_start,
                    &info.screen_info[slot].resolution.vsync_end, &info.screen_info[slot].resolution.vtotal,
                    &info.screen_info[slot].resolution.flags, &info.screen_info[slot].resolution.clock);
           info.screen_info[slot].resolution.vrefresh = (int)vfresh;
        } else {
            info.screen_info[slot].feature|= RESOLUTION_AUTO;
            memset(&info.screen_info[slot].resolution, 0, sizeof(info.screen_info[slot].resolution));
        }
        mBaseParameter->set_disp_info(conn->get_type(), conn->connector_id(), &info);
    }
    return 0;
}

static int hw_output_set_3d_mode(struct hw_output_device*, const char* mode)
{
    char property[PROPERTY_VALUE_MAX];

    property_get("vendor.3d_resolution.main", property, "null");
    if (strcmp(mode, property) !=0) {
        property_set("vendor.3d_resolution.main", mode);
        updateTimeline();
    }
    return 0;
}

static int hw_output_set_gamma(struct hw_output_device* dev, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    int ret = -1;
    int crtc_id = 0;

    if (mConnector)
        crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();

    if (mBaseparameterMajorVersion >= 2 && mBaseparameterMinorVersion >= 1) {
        csc_info csc;
        mBaseParameter->get_csc_info(&csc);
        gamma_lut_data gamma;
        gamma.size = size;
        for(int i = 0; i< size; i++){
           gamma.lred[i] = r[i];
           gamma.lgreen[i] = g[i];
           gamma.lblue[i] = b[i];
        }
        mBaseParameter->set_pq_tuning_gamma(&gamma);
        unsigned int rgain = 256, ggain = 256, bgain = 256;
        if(csc.cscRGain != 0) {
            rgain = csc.cscRGain;
        }
        if (csc.cscGGain != 0) {
            ggain = csc.cscGGain;
        }
        if (csc.cscBGain == 0) {
            bgain = csc.cscBGain;
        }
        DrmGamma::gamma_color_temp_adjust(gamma.lred, gamma.lgreen, gamma.lblue, rgain, bgain, ggain);
#ifdef USE_HWC_PROXY_SERVICE
        ret = mClient.setGamma(dpy, size, gamma.lred, gamma.lgreen, gamma.lblue);
#else
        ret = DrmGamma::set_3x1d_gamma(priv->drm_->fd(), crtc_id, size, gamma.lred, gamma.lgreen, gamma.lblue);
#endif
        if(mConnector && ret == 0)
            mBaseParameter->set_gamma_lut_data(mConnector->get_type(), mConnector->connector_id(), &gamma);
    } else {
#ifdef USE_HWC_PROXY_SERVICE
        ret = mClient.setGamma(dpy, size, r, g, b);
#else
        ret = DrmGamma::set_3x1d_gamma(priv->drm_->fd(), crtc_id, size, r, g, b);
#endif
        if (ret < 0)
            ALOGE("fail to SetGamma %d(%s)", ret, strerror(errno));
        if(ret == 0){
            struct gamma_lut_data data;
            data.size = size;
            for(int i = 0; i< size; i++){
                data.lred[i] = r[i];
                data.lgreen[i] = g[i];
                data.lblue[i] = b[i];
            }
            if(mConnector)
                mBaseParameter->set_gamma_lut_data(mConnector->get_type(), mConnector->connector_id(), &data);
        }
    }

    return ret;
}

static int hw_output_set_3d_lut(struct hw_output_device* dev, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    int ret = -1;
    int crtc_id = 0;
    uint16_t dst_red[size];
    uint16_t dst_green[size];
    uint16_t dst_blue[size];
    uint32_t dst_size = 0;

    if (!mConnector)
        return -1;
    crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();
        dst_size = DrmGamma::get_cubic_lut_size(priv->drm_->fd(), crtc_id);
    DrmGamma::convert_3d_lut_data(size, dst_size, r, g, b, dst_red, dst_green, dst_blue);


#ifdef USE_HWC_PROXY_SERVICE
    ret = mClient.set3DLut(dpy, dst_size, dst_red, dst_green, dst_blue);
#else
    ret = DrmGamma::set_cubic_lut(priv->drm_->fd(), crtc_id, size, dst_red, dst_green, dst_blue);
#endif
    if (ret < 0)
        ALOGE("fail to set 3d lut %d(%s)", ret, strerror(errno));
    if(ret == 0){
        struct cubic_lut_data data;
        data.size = dst_size;
        for(int i = 0; i < dst_size; i++){
            data.lred[i] = dst_red[i];
            data.lgreen[i] = dst_green[i];
            data.lblue[i] = dst_blue[i];
        }
        ret = mBaseParameter->set_cubic_lut_data(mConnector->get_type(), mConnector->connector_id(), &data);
    }
    return ret;
}

static int hw_output_set_brightness(struct hw_output_device* dev, int dpy, int brightness)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    propertyStr = getPropertySuffix(priv, "persist.vendor.brightness.", dpy);
    sprintf(tmp, "%d", brightness);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != brightness) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_brightness(conn->get_type(), conn->connector_id(), brightness);
    }
    return 0;
}

static int hw_output_set_contrast(struct hw_output_device* dev, int dpy, int contrast)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    sprintf(tmp, "%d", contrast);
    propertyStr = getPropertySuffix(priv, "persist.vendor.contrast.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != contrast) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_contrast(conn->get_type(), conn->connector_id(), contrast);
    }
    return 0;
}

static int hw_output_set_sat(struct hw_output_device* dev, int dpy, int sat)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    sprintf(tmp, "%d", sat);
    propertyStr = getPropertySuffix(priv, "persist.vendor.saturation.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != sat) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_saturation(conn->get_type(), conn->connector_id(), sat);
    }
    return 0;
}

static int hw_output_set_hue(struct hw_output_device* dev, int dpy, int hue)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];
    std::string propertyStr;

    sprintf(tmp, "%d", hue);
    propertyStr = getPropertySuffix(priv, "persist.vendor.hue.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != hue) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
        mBaseParameter->set_hue(conn->get_type(), conn->connector_id(), hue);
    }
    return 0;
}

static int hw_output_set_screen_scale(struct hw_output_device* dev, int dpy, int direction, int value)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    std::string propertyStr;
    char property[PROPERTY_VALUE_MAX];
    char overscan[128];
    int left,top,right,bottom;

    propertyStr = getPropertySuffix(priv, "persist.vendor.overscan.", dpy);
    property_get(propertyStr.c_str(), property, "overscan 100,100,100,100");
    sscanf(property, "overscan %d,%d,%d,%d", &left, &top, &right, &bottom);

    if (direction == OVERSCAN_LEFT)
        left = value;
    else if (direction == OVERSCAN_TOP)
        top = value;
    else if (direction == OVERSCAN_RIGHT)
        right = value;
    else if (direction == OVERSCAN_BOTTOM)
        bottom = value;

    sprintf(overscan, "overscan %d,%d,%d,%d", left, top, right, bottom);

    if (strcmp(property, overscan) != 0) {
        property_set(propertyStr.c_str(), overscan);
        updateTimeline();
        struct overscan_info overscan;
        overscan.maxvalue = 100;
        overscan.leftscale = left;
        overscan.topscale = top;
        overscan.rightscale = right;
        overscan.bottomscale = bottom;
        mBaseParameter->set_overscan_info(conn->get_type(), conn->id(), &overscan);
    }

    return 0;
}

static int hw_output_set_hdr_mode(struct hw_output_device* dev, int dpy, int hdr_mode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    std::string propertyStr, hdrStr;
    char property[PROPERTY_VALUE_MAX];
    char tmp[128];

    sprintf(tmp, "%d", hdr_mode);
    propertyStr = getPropertySuffix(priv, "persist.vendor.hdr_mode.", dpy);
    property_get(propertyStr.c_str(), property, "50");

    if (atoi(property) != hdr_mode) {
        property_set(propertyStr.c_str(), tmp);
        updateTimeline();
    }
    return 0;
}

static int hw_output_set_color_mode(struct hw_output_device* dev, int dpy, const char* color_mode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    std::string propertyStr;
    struct disp_info info;
    char property[PROPERTY_VALUE_MAX];

    propertyStr = getPropertySuffix(priv, "persist.vendor.color.", dpy);
    property_get(propertyStr.c_str(), property, NULL);
    ALOGD("hw_output_set_color_mode %s display %d property=%s", color_mode, dpy, property);

    if (strcmp(color_mode, property) !=0) {
        property_set(propertyStr.c_str(), color_mode);
        property_get(propertyStr.c_str(), property, NULL);
        updateTimeline();
    }
    if (conn) {
        mBaseParameter->get_disp_info(conn->get_type(), conn->connector_id(), &info);
        int slot = findSuitableInfoSlot(&info, conn->get_type(), conn->connector_id());
        if (strncmp(property, "Auto", 4) != 0){
            if (strstr(property, "RGB") != 0)
                info.screen_info[slot].format = output_rgb;
            else if (strstr(property, "YCBCR444") != 0)
                info.screen_info[slot].format = output_ycbcr444;
            else if (strstr(property, "YCBCR422") != 0)
                info.screen_info[slot].format = output_ycbcr422;
            else if (strstr(property, "YCBCR420") != 0)
                info.screen_info[slot].format = output_ycbcr420;
            else {
                info.screen_info[slot].feature |= COLOR_AUTO;
                info.screen_info[slot].format = output_ycbcr_high_subsampling;
            }

            if (strstr(property, "8bit") != NULL)
                info.screen_info[slot].depthc = depth_24bit;
            else if (strstr(property, "10bit") != NULL)
                info.screen_info[slot].depthc = depth_30bit;
            else
                info.screen_info[slot].depthc = Automatic;
        } else {
            info.screen_info[slot].depthc = Automatic;
            info.screen_info[slot].format = output_ycbcr_high_subsampling;
            info.screen_info[slot].feature |= COLOR_AUTO;
        }
        ALOGD("saveConfig: color=%d-%d", info.screen_info[slot].format, info.screen_info[slot].depthc);
        mBaseParameter->set_disp_info(conn->get_type(), conn->connector_id(), &info);
    }
    return 0;
}

static int hw_output_get_cur_mode(struct hw_output_device* dev, int dpy, char* curMode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    bool found=false;

    if (curMode != NULL)
        found = getResolutionInfo(priv, dpy, curMode);
    else
        return -1;

    if (!found) {
        sprintf(curMode, "%s", "Auto");
    }

    return 0;
}

static int hw_output_get_cur_color_mode(struct hw_output_device* dev, int dpy, char* curColorMode)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* mCurConnector = getValidDrmConnector(priv, dpy);
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    struct disp_info dispInfo;
    std::string propertyStr;
    char colorMode[PROPERTY_VALUE_MAX];

    int len=0;

    propertyStr = getPropertySuffix(priv, "persist.vendor.color.", dpy);
    len = property_get(propertyStr.c_str(), colorMode, NULL);

    ALOGD("nativeGetCurCorlorMode: property=%s", colorMode);
    if (!len && mBaseParmeter && mBaseParmeter->have_baseparameter()) {
        mBaseParmeter->get_disp_info(mCurConnector->get_type(), mCurConnector->connector_id(), &dispInfo);
        int slot = findSuitableInfoSlot(&dispInfo, mCurConnector->get_type(), mCurConnector->connector_id());
        if (dispInfo.screen_info[slot].depthc == Automatic &&
                dispInfo.screen_info[slot].format == output_ycbcr_high_subsampling)
            sprintf(colorMode, "%s", "Auto");
        }

    sprintf(curColorMode, "%s", colorMode);
    ALOGD("nativeGetCurCorlorMode: colorMode=%s", colorMode);
    return 0;
}

static int hw_output_get_num_connectors(struct hw_output_device* dev, int, int* numConnectors)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    (void)priv;

    *numConnectors = mGlobalConns.size();//priv->drm_->connectors().size();
    return 0;
}

static int hw_output_get_connector_state(struct hw_output_device* dev, int dpy, int* state)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    int ret = 0;
    DrmConnector* mConn = getValidDrmConnector(priv, dpy);

    if (mConn != nullptr) {
        *state = mConn->state();
    } else {
        ret = -1;
    }
    return ret;
}

static int hw_output_get_color_configs(struct hw_output_device* dev, int dpy, int* configs)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* mCurConnector = getValidDrmConnector(priv, dpy);;
    uint64_t color_capacity=0;
    uint64_t depth_capacity=0;

    if (mCurConnector != NULL) {
        if (mCurConnector->hdmi_output_mode_capacity_property().id())
            mCurConnector->hdmi_output_mode_capacity_property().value( &color_capacity);

        if (mCurConnector->hdmi_output_depth_capacity_property().id())
            mCurConnector->hdmi_output_depth_capacity_property().value(&depth_capacity);

        configs[0] = (int)color_capacity;
        configs[1] = (int)depth_capacity;
        ALOGD("nativeGetCorlorModeConfigs: corlor=%d depth=%d configs:%d %d",(int)color_capacity,(int)depth_capacity, configs[0], configs[1]);
    }
    return 0;
}

static int hw_output_get_overscan(struct hw_output_device* dev, int dpy, uint32_t* overscans)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    char property[PROPERTY_VALUE_MAX];
    std::string propertyStr;
    int left,top,right,bottom;

    propertyStr = getPropertySuffix(priv, "persist.vendor.overscan.", dpy);
    property_get(propertyStr.c_str(), property, "overscan 100,100,100,100");

    sscanf(property, "overscan %d,%d,%d,%d", &left, &top, &right, &bottom);
    overscans[0] = left;
    overscans[1] = top;
    overscans[2] = right;
    overscans[3] = bottom;
    return 0;
}

static int hw_output_get_bcsh(struct hw_output_device* dev, int dpy, uint32_t* bcshs)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParmeter = priv->mBaseParmeter;
    DrmConnector* conn = getValidDrmConnector(priv, dpy);
    char mBcshProperty[PROPERTY_VALUE_MAX];
    std::string propertyStr;

    propertyStr = getPropertySuffix(priv, "persist.vendor.brightness.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[0] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[0] = mBaseParmeter->get_brightness(conn->get_type(), conn->connector_id());
    } else {
         bcshs[0] = DEFAULT_BRIGHTNESS;
    }

    memset(mBcshProperty, 0, sizeof(mBcshProperty));
    propertyStr = getPropertySuffix(priv, "persist.vendor.contrast.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[1] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[1] = mBaseParmeter->get_contrast(conn->get_type(), conn->connector_id());
    } else {
         bcshs[1] = DEFAULT_CONTRAST;
    }

    memset(mBcshProperty, 0, sizeof(mBcshProperty));
    propertyStr = getPropertySuffix(priv, "persist.vendor.saturation.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[2] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[2] = mBaseParmeter->get_contrast(conn->get_type(), conn->connector_id());
    } else {
         bcshs[2] = DEFAULT_SATURATION;
    }

    memset(mBcshProperty, 0, sizeof(mBcshProperty));
    propertyStr = getPropertySuffix(priv, "persist.vendor.hue.", dpy);
    if (property_get(propertyStr.c_str(), mBcshProperty, NULL) > 0) {
        bcshs[3] = atoi(mBcshProperty);
    } else if (mBaseParmeter&&mBaseParmeter->have_baseparameter()) {
        bcshs[3] = mBaseParmeter->get_hue(conn->get_type(), conn->connector_id());
    } else {
         bcshs[3] = DEFAULT_SATURATION;
    }

    checkBcshInfo(bcshs);
    ALOGD("Bcsh: %d %d %d %d ", bcshs[0], bcshs[1], bcshs[2], bcshs[3]);
    return 0;
}

static int hw_output_get_builtin(struct hw_output_device* dev, int dpy, int* builtin)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    if (mConnector) {
        *builtin = mConnector->get_type();
    } else {
        *builtin = 0;
    }
    return 0;
}

static drm_mode_t* hw_output_get_display_modes(struct hw_output_device* dev, int dpy, uint32_t* size)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    std::vector<DrmMode> mModes;
    DrmConnector* mCurConnector;
    drm_mode_t* drm_modes = NULL;
    int idx=0;

    *size = 0;
    mCurConnector = getValidDrmConnector(priv, dpy);
    if (mCurConnector) {
        mModes = mCurConnector->modes();
    } else {
        return NULL;
    }

    if (mModes.size() == 0)
        return NULL;

    drm_modes = (drm_mode_t*)malloc(sizeof(drm_mode_t) * mModes.size());

    for (size_t c = 0; c < mModes.size(); ++c) {
        const DrmMode& info = mModes[c];
        float vfresh;

        if (info.flags() & DRM_MODE_FLAG_INTERLACE)
            vfresh = info.clock()*2 / (float)(info.v_total()* info.h_total()) * 1000.0f;
        else
            vfresh = info.clock()/ (float)(info.v_total()* info.h_total()) * 1000.0f;
        drm_modes[c].width = info.h_display();
        drm_modes[c].height = info.v_display();
        drm_modes[c].refreshRate = vfresh;
        drm_modes[c].clock = info.clock();
        drm_modes[c].flags = info.flags();
        drm_modes[c].interlaceFlag = info.flags()&(1<<4);
        drm_modes[c].yuvFlag = (info.flags()&(1<<24) || info.flags()&(1<<23));
        drm_modes[c].connectorId = mCurConnector->id();
        drm_modes[c].mode_type = info.type();
        drm_modes[c].idx = idx;
        drm_modes[c].hsync_start = info.h_sync_start();
        drm_modes[c].hsync_end = info.h_sync_end();
        drm_modes[c].htotal = info.h_total();
        drm_modes[c].hskew = info.h_skew();
        drm_modes[c].vsync_start = info.v_sync_start();
        drm_modes[c].vsync_end = info.v_sync_end();
        drm_modes[c].vtotal = info.v_total();
        drm_modes[c].vscan = info.v_scan();
        idx++;
        ALOGV("display%d mode[%d]  %dx%d fps %f clk %d  h_start %d h_enc %d htotal %d hskew %d",
                dpy,(int)c, info.h_display(), info.v_display(), info.v_refresh(),
                info.clock(),  info.h_sync_start(),info.h_sync_end(),
                info.h_total(), info.h_skew());
        ALOGV("vsync_start %d vsync_end %d vtotal %d vscan %d flags 0x%x",
                info.v_sync_start(), info.v_sync_end(), info.v_total(), info.v_scan(),
                info.flags());
    }
    *size = idx;
    return drm_modes;
}

/*****************************************************************************/
static int hw_output_device_close(struct hw_device_t *dev)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;

    if (priv->mBaseParmeter) {
        delete priv->mBaseParmeter;
    }
    if (priv) {
        free(priv);
    }
    return 0;
}

static connector_info_t* hw_output_get_connector_info(struct hw_output_device* dev, uint32_t* size)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    *size = 0;
    connector_info_t* connector_info = NULL;
    connector_info = (connector_info_t*)malloc(sizeof(connector_info_t) * priv->drm_->connectors().size());
    int i = 0;
    for (auto &conn : mGlobalConns) {
        DrmConnector* mConn = conn.second;
        connector_info[i].type = mConn->get_type();
        connector_info[i].id = (uint32_t)mConn->connector_id();
        connector_info[i].state = (uint32_t)mConn->state();
        i++;
    }
    *size = i;
    ALOGE("%s:%d i=%d", __FUNCTION__, __LINE__, i);
    return connector_info;
}

static int hw_output_get_mode_state(struct hw_output_device *, const char* mode, char *state)
{
    property_get(mode, state, "");
    ALOGD("%s:%d mode = %s, state = %s\n", __FUNCTION__, __LINE__, mode, state);
    return 0;
}

static int hw_output_set_mode_state(struct hw_output_device *, const char* mode, const char* state)
{
    int ret = property_set(mode, state);
    updateTimeline();
    ALOGD("%s:%d mode = %s, state = %s, ret = %d\n", __FUNCTION__, __LINE__, mode, state, ret);
    return ret;
}

static int hw_output_get_hdr_resolution_supported(struct hw_output_device *dev, int dpy, const char *mode, uint32_t* hdr_state)
{
    hw_output_private_t *priv = (hw_output_private_t *)dev;
    DrmConnector *mCurConnector = getValidDrmConnector(priv, dpy);
    drmModeObjectPropertiesPtr props;
    drmModePropertyBlobPtr blob;
    struct hdr_static_metadata* blob_data = nullptr;
    bool found = false;
    int value;

    *hdr_state = 0;

    if (mCurConnector == NULL)
        return -1;
    props = drmModeObjectGetProperties(priv->drm_->fd(), mCurConnector->id(), DRM_MODE_OBJECT_CONNECTOR);
    for (int i = 0; !found && (size_t)i < props->count_props; ++i)
    {
        drmModePropertyPtr p = drmModeGetProperty(priv->drm_->fd(), props->props[i]);
        if (p && !strcmp(p->name, "HDR_PANEL_METADATA"))
        {
            if (!drm_property_type_is(p, DRM_MODE_PROP_BLOB))
            {
                ALOGE("%s:%d HDR_PANEL_METADATA property is not blob type", __FUNCTION__, __LINE__);
                drmModeFreeProperty(p);
                drmModeFreeObjectProperties(props);
                return -1;
            }

            if (!p->count_blobs)
                value = props->prop_values[i];
            else
                value = p->blob_ids[0];

            blob = drmModeGetPropertyBlob(priv->drm_->fd(), value);
            if (!blob)
            {
                ALOGE("%s:line=%d, blob is null", __FUNCTION__, __LINE__);
                drmModeFreeProperty(p);
                drmModeFreeObjectProperties(props);
                return -1;
            }

            blob_data = (struct hdr_static_metadata *)blob->data;
            found = true;
            *hdr_state = blob_data->eotf & HW_OUTPUT_VALUE_HDR10_MASK ? *hdr_state | HW_OUTPUT_FOR_FRAMEWORK_HDR10_MASK : *hdr_state;
            *hdr_state = blob_data->eotf & HW_OUTPUT_VALUE_HLG_MASK ? *hdr_state | HW_OUTPUT_FOR_FRAMEWORK_HLG_MASK : *hdr_state;
            ALOGD("%s:%d HDR_PANEL_METADATA property is found , mode = %s, hdr_static_metadata.eotf = %d, hdr_state = %d", __FUNCTION__, __LINE__, mode, blob_data->eotf, *hdr_state);

            drmModeFreePropertyBlob(blob);
        }
        drmModeFreeProperty(p);
        if (found)
            break;
    }
    drmModeFreeObjectProperties(props);
    return found ? 0 : -1;
}

static pq_setting_config* getPQConfig(char* path) {
    if(path == NULL) {
        return NULL;
    }
    if (access(path, F_OK) != 0) {
        return NULL;
    }
    size_t json_data_size = 0;
    char * str = (char *)j2s_read_file(path, &json_data_size);
    cJSON* base = cJSON_Parse(str);
    cJSON* param = cJSON_GetObjectItem(base, "pq_tuning_param");
    pq_setting_config* config = (pq_setting_config*)malloc(sizeof(pq_setting_config));
    j2s_ctx ctx;
    j2s_init(&ctx);
    ctx.format_json = false;
    int ret = j2s_json_to_struct(&ctx, param, "pq_tuning_param", &config->pq_tuning_param);
    j2s_deinit(&ctx);
    cJSON_Delete(base);
    return config;
}

static int hw_output_set_sw_bcsh(struct hw_output_device* dev, int dpy, int bright, int contrast, int saturation, int hue)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    cscInfo.cscBrightness = bright;
    cscInfo.cscContrast = contrast;
    cscInfo.cscSaturation = saturation;
    cscInfo.cscHue = hue;
    ret = mBaseParameter->set_csc_info(&cscInfo);
    updatePQTimeline();

    return ret;
}

static int hw_output_set_sw_brightness(struct hw_output_device* dev, int dpy, int bright)
{
    int ret = 0;
    if (property_get_int32(PROPERTY_PQ_MODE, 0) || property_get_int32(PROPERTY_RKPQ_ENABLE, 0)) {
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        csc_info cscInfo;
        ret = mBaseParameter->get_csc_info(&cscInfo);
        int cscBrightness = (float)bright / 100 * 511 + 1;
        cscInfo.cscBrightness = cscBrightness > 511 ? 511 : cscBrightness;
        ret = mBaseParameter->set_csc_info(&cscInfo);
        updatePQTimeline();
    } else {
        hw_output_set_brightness(dev, 0, bright);
    }
    return ret;
}

static int hw_output_get_sw_brightness(struct hw_output_device* dev, int dpy, int* bright)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *bright = (float)cscInfo.cscBrightness / 511 * 100;
    return ret;
}

static int hw_output_set_sw_contrast(struct hw_output_device* dev, int dpy, int contrast)
{
    int ret = 0;
    if (property_get_int32(PROPERTY_PQ_MODE, 0) || property_get_int32(PROPERTY_RKPQ_ENABLE, 0)) {
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        csc_info cscInfo;
        ret = mBaseParameter->get_csc_info(&cscInfo);
        int cscContrast = (float)contrast / 100 * 511 + 1;
        cscInfo.cscContrast = cscContrast > 511 ? 511 : cscContrast;
        ret = mBaseParameter->set_csc_info(&cscInfo);
        updatePQTimeline();
    } else {
        hw_output_set_contrast(dev, 0, contrast);
    }
    return ret;
}

static int hw_output_get_sw_contrast(struct hw_output_device* dev, int dpy, int* contrast)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *contrast = (float)cscInfo.cscContrast / 511 * 100;
    return ret;
}

static int hw_output_set_sw_saturation(struct hw_output_device* dev, int dpy, int saturation)
{
    int ret = 0;
    if (property_get_int32(PROPERTY_PQ_MODE, 0) || property_get_int32(PROPERTY_RKPQ_ENABLE, 0)) {
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        csc_info cscInfo;
        ret = mBaseParameter->get_csc_info(&cscInfo);
        int cscSaturation = (float)saturation / 100 * 511 + 1;
        cscInfo.cscSaturation = cscSaturation > 511 ? 511 : cscSaturation;
        ret = mBaseParameter->set_csc_info(&cscInfo);
        updatePQTimeline();
    } else {
        hw_output_set_sat(dev, 0, saturation);
    }
    return ret;
}

static int hw_output_get_sw_saturation(struct hw_output_device* dev, int dpy, int* saturation)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *saturation = (float)cscInfo.cscSaturation / 511 * 100;
    return ret;
}

static int hw_output_set_sw_hue(struct hw_output_device* dev, int dpy, int hue)
{
    int ret = 0;
    if (property_get_int32(PROPERTY_PQ_MODE, 0) || property_get_int32(PROPERTY_RKPQ_ENABLE, 0)) {
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        csc_info cscInfo;
        ret = mBaseParameter->get_csc_info(&cscInfo);
        int cscHue = (float)hue / 100 * 511 + 1;
        cscInfo.cscHue = cscHue > 511 ? 511 : cscHue;
        ret = mBaseParameter->set_csc_info(&cscInfo);
        updatePQTimeline();
    } else {
        hw_output_set_hue(dev, 0, hue);
    }
    return ret;
}

static int hw_output_get_sw_hue(struct hw_output_device* dev, int dpy, int* hue)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *hue = (float)cscInfo.cscHue / 511 * 100;
    return ret;
}

static int hw_output_set_white_balance(struct hw_output_device* dev, int dpy, int rgain, int ggain, int bgain)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    if (!mConnector) {
        return -ENOENT;
    }
    gamma_lut_data gamma;
    mBaseParameter->get_pq_tuning_gamma(&gamma);
    int crtc_id = 0;
    crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();
    csc_info csc;
    DrmGamma::gamma_color_temp_adjust(gamma.lred, gamma.lgreen, gamma.lblue, rgain, ggain, bgain);
    int ret = DrmGamma::set_3x1d_gamma(priv->drm_->fd(), crtc_id, 1024, gamma.lred, gamma.lgreen, gamma.lblue);
    if (ret == 0) {
        csc_info cscInfo;
        mBaseParameter->get_csc_info(&cscInfo);
        cscInfo.cscRGain = rgain;
        cscInfo.cscGGain = ggain;
        cscInfo.cscBGain = bgain;
        mBaseParameter->set_csc_info(&cscInfo);
        gamma.size = 1024;
        mBaseParameter->set_gamma_lut_data(mConnector->get_type(), mConnector->connector_id(), &gamma);
    }
    return ret;
}

static int hw_output_set_rgain(struct hw_output_device* dev, int dpy, int rgain)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    cscInfo.cscRGain = rgain;
    //ret = mBaseParameter->set_csc_info(&cscInfo);
    //updatePQTimeline();
    ret = hw_output_set_white_balance(dev, dpy, cscInfo.cscRGain, cscInfo.cscGGain, cscInfo.cscBGain);
    return ret;
}

static int hw_output_get_rgain(struct hw_output_device* dev, int dpy, int* rgain)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *rgain = cscInfo.cscRGain;
    return ret;
}

static int hw_output_set_ggain(struct hw_output_device* dev, int dpy, int ggain)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    cscInfo.cscGGain = ggain;
    //ret = mBaseParameter->set_csc_info(&cscInfo);
    //updatePQTimeline();
    ret = hw_output_set_white_balance(dev, dpy, cscInfo.cscRGain, cscInfo.cscGGain, cscInfo.cscBGain);
    return ret;
}

static int hw_output_get_ggain(struct hw_output_device* dev, int dpy, int* ggain)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *ggain = cscInfo.cscGGain;
    return ret;
}

static int hw_output_set_bgain(struct hw_output_device* dev, int dpy, int bgain)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    cscInfo.cscBGain = bgain;
    //ret = mBaseParameter->set_csc_info(&cscInfo);
    //updatePQTimeline();
    ret = hw_output_set_white_balance(dev, dpy, cscInfo.cscRGain, cscInfo.cscGGain, cscInfo.cscBGain);
    return ret;
}

static int hw_output_get_bgain(struct hw_output_device* dev, int dpy, int* bgain)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    *bgain = cscInfo.cscBGain;
    return ret;
}

static int hw_output_set_csc_enable(struct hw_output_device* dev, bool enable)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    cscInfo.cscEnable = enable;
    ret = mBaseParameter->set_csc_info(&cscInfo);
    updatePQTimeline();
    return ret;
}

static int hw_output_set_dci_enable(struct hw_output_device* dev, bool enable)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    dci_info dciInfo;
    ret = mBaseParameter->get_dci_info(&dciInfo);
    dciInfo.dciEnable = enable;
    ret = mBaseParameter->set_dci_info(&dciInfo);
    updatePQTimeline();
    return ret;
}

static int hw_output_set_acm_enable(struct hw_output_device* dev, bool enable)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    acm_info acmInfo;
    ret = mBaseParameter->get_acm_info(&acmInfo);
    acmInfo.acmEnable = enable;
    ret = mBaseParameter->set_acm_info(&acmInfo);
    updatePQTimeline();
    return ret;
}

static int hw_output_set_sharp_enable(struct hw_output_device* dev, bool enable)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_sharp_info sharpInfo;
    ret = mBaseParameter->get_pq_sharp_info(&sharpInfo);
    sharpInfo.sharpEnable = enable;
    ret = mBaseParameter->set_pq_sharp_info(&sharpInfo);
    updatePQTimeline();
    return ret;
}

static int hw_output_set_pq_enable(struct hw_output_device* dev, bool enable)
{
    UN_USED(dev);
    int hdmiin = property_get_int32(PROPERTY_TV_INPUT_HDMIIN, 0);
    if (mRkPlatform == HW_OUTPUT_RK3588) {
    if (enable) {
        if(hdmiin)
            property_set(PROPERTY_PQ_MODE, "0");
        else
            property_set(PROPERTY_PQ_MODE, "1");
       property_set(PROPERTY_RKPQ_ENABLE, "1");
    } else {
       property_set(PROPERTY_PQ_MODE, "0");
       property_set(PROPERTY_RKPQ_ENABLE, "0");
    }
    } else if (mRkPlatform == HW_OUTPUT_RK3576) {
        if (enable) {
            if(hdmiin)
                property_set(PROPERTY_PQ_MODE, "2");
            else
                property_set(PROPERTY_PQ_MODE, "3");
        } else {
           property_set(PROPERTY_PQ_MODE, "0");
        }
    }
    return 0;
}

static int hw_output_get_pq_enable(struct hw_output_device* dev)
{
    UN_USED(dev);
    int ret = 0;
    if (mRkPlatform == HW_OUTPUT_RK3588) {
        int hdmiin = property_get_int32(PROPERTY_TV_INPUT_HDMIIN, 0);
        if (hdmiin) {
            ret = property_get_int32(PROPERTY_RKPQ_ENABLE, 0);
        } else {
            ret = property_get_int32(PROPERTY_PQ_MODE, 0);
        }
        return ret > 0 ? 1 : 0;
    } else if (mRkPlatform == HW_OUTPUT_RK3576) {
        ret = property_get_int32(PROPERTY_PQ_MODE, 0);
        return ret == 2 || ret == 3;
    }
    return 0;
}

static int hw_output_get_csc_enable(struct hw_output_device* dev)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info cscInfo;
    ret = mBaseParameter->get_csc_info(&cscInfo);
    if (ret == 0) {
        ret = cscInfo.cscEnable;
    } else {
        ret = 0;
    }
    return ret;
}

static int hw_output_get_acm_enable(struct hw_output_device* dev)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    acm_info acmInfo;
    ret = mBaseParameter->get_acm_info(&acmInfo);
    if (ret == 0) {
        ret = acmInfo.acmEnable;
    } else {
        ret = 0;
    }
    return ret;
}

static int hw_output_get_dci_enable(struct hw_output_device* dev)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    dci_info dciInfo;
    ret = mBaseParameter->get_dci_info(&dciInfo);
    if (ret == 0) {
        ret = dciInfo.dciEnable;
    } else {
        ret = 0;
    }
    return ret;
}

static int hw_output_get_sharp_enable(struct hw_output_device* dev)
{
    int ret = 0;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_sharp_info sharpInfo;
    ret = mBaseParameter->get_pq_sharp_info(&sharpInfo);
    if (ret == 0) {
        ret = sharpInfo.sharpEnable;
    } else {
        ret = 0;
    }
    return ret;
}

static int hw_output_set_bcsh_mode(struct hw_output_device* dev, int dpy, int index)
{
    UN_USED(dpy);
    int mode_size = sizeof(bcsh_mode)/sizeof(bcsh_mode[0]);
    ALOGD("mode size %d ", mode_size);
    if(index < 0 || index >= mode_size) {
        return -1;
    }
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    csc_info csc;
    mBaseParameter->get_pq_factory_info(&info);
    mBaseParameter->get_csc_info(&csc);
    unsigned int brightness, contrast, saturation, hue;
    if(info.bcsh[index].brightness == 0 && info.bcsh[index].contrast == 0 && info.bcsh[index].saturation == 0 && info.bcsh[index].hue == 0) {
        brightness = bcsh_mode[index].brightness;
        contrast = bcsh_mode[index].contrast;
        saturation = bcsh_mode[index].saturation;
        hue = bcsh_mode[index].hue;
    } else {
        brightness = info.bcsh[index].brightness;
        contrast = info.bcsh[index].contrast;
        saturation = info.bcsh[index].saturation;
        hue = info.bcsh[index].hue;
   }

    int pq_enable = property_get_int32(PROPERTY_PQ_MODE, 0) ||
        property_get_int32(PROPERTY_RKPQ_ENABLE, 0);
    if (!pq_enable) {
        csc.cscBrightness = brightness;
        csc.cscContrast = contrast;
        csc.cscSaturation = saturation;
        csc.cscHue = hue;
        mBaseParameter->set_csc_info(&csc);
        brightness = (float)brightness / 511 * 100;
        contrast = (float)contrast / 511 * 100;
        saturation = (float)saturation / 511 * 100;
        hue = (float)hue / 511 * 100;
        hw_output_set_brightness(dev, 0, brightness);
        hw_output_set_contrast(dev, 0, contrast);
        hw_output_set_sat(dev, 0, saturation);
        hw_output_set_hue(dev, 0, hue);
    } else {
        hw_output_set_sw_bcsh(dev, 0, brightness, contrast, saturation, hue);
    }
    info.cur_bcsh_index = index;
    mBaseParameter->set_pq_factory_info(&info);
    return 0;
}

static int hw_output_set_white_balance_mode(struct hw_output_device* dev, int dpy, int index)
{
    int mode_size = sizeof(white_balance_mode)/sizeof(white_balance_mode[0]);
    ALOGE("mode size %d ", mode_size);
    if(index < 0 || index >= mode_size) {
        return -1;
    }
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    white_balance_data wb_data;
    pq_factory_info info;
    gamma_lut_data gamma;
    AutoPQ::get_auto_white_balance(index, &wb_data);
    mBaseParameter->get_pq_factory_info(&info);
    mBaseParameter->get_pq_tuning_gamma(&gamma);
    unsigned int rgain, ggain, bgain;
    if(wb_data.rgain != 0 && wb_data.ggain != 0 && wb_data.bgain != 0) {
        rgain = wb_data.rgain;
        ggain = wb_data.ggain;
        bgain = wb_data.bgain;
    } else if(info.white_balance[index].rgain != 0 && info.white_balance[index].ggain != 0 && info.white_balance[index].bgain != 0) {
        rgain = info.white_balance[index].rgain;
        ggain = info.white_balance[index].ggain;
        bgain = info.white_balance[index].bgain;
    } else {
        rgain = white_balance_mode[index].rgain;
        ggain = white_balance_mode[index].ggain;
        bgain = white_balance_mode[index].bgain;
    }

    int crtc_id = 0;
    if (mConnector)
        crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();
    csc_info csc;
    mBaseParameter->get_csc_info(&csc);
    csc.cscRGain = rgain;
    csc.cscGGain = ggain;
    csc.cscBGain = bgain;
    mBaseParameter->set_csc_info(&csc);
    DrmGamma::gamma_color_temp_adjust(gamma.lred, gamma.lgreen, gamma.lblue, rgain, ggain, bgain);
    int ret = DrmGamma::set_3x1d_gamma(priv->drm_->fd(), crtc_id, 1024, gamma.lred, gamma.lgreen, gamma.lblue);
    gamma.size = 1024;
    mBaseParameter->set_gamma_lut_data(mConnector->get_type(), mConnector->connector_id(), &gamma);
    info.cur_white_balance_index = index;
    mBaseParameter->set_pq_factory_info(&info);
    return ret;
}

static int hw_output_set_acm_mode(struct hw_output_device* dev, int dpy, int index)
{
    UN_USED(dpy);
    pq_setting_config* config = getPQConfig(PQ_SETTING_CONFIG_JSON_PATH);
    if(config) {
        acm_info info;
        acm acm_data = config->pq_tuning_param.acm[index];
        info.acmEnable = acm_data.acmEnable;
        for (int i = 0; i< 65; i++) {
            info.acmTableDeltaYbyH[i] = acm_data.acmTableDeltaYbyH[i];
        }
        for (int i = 0; i< 65; i++) {
            info.acmTableDeltaHbyH[i] = acm_data.acmTableDeltaHbyH[i];
        }
        for (int i = 0; i< 65; i++) {
            info.acmTableDeltaSbyH[i] = acm_data.acmTableDeltaSbyH[i];
        }
        for (int i = 0; i< 9 * 65; i++) {
            info.acmTableGainYbyY[i] = acm_data.acmTableGainYbyY[i];
        }
        for (int i = 0; i< 9 * 65; i++) {
            info.acmTableGainHbyY[i] = acm_data.acmTableGainHbyY[i];
        }
        for (int i = 0; i< 9 * 65; i++) {
            info.acmTableGainSbyY[i] = acm_data.acmTableGainSbyY[i];
        }
        for (int i = 0; i< 13 * 65; i++) {
            info.acmTableGainYbyS[i] = acm_data.acmTableGainYbyS[i];
        }
        for (int i = 0; i< 13 * 65; i++) {
            info.acmTableGainHbyS[i] = acm_data.acmTableGainHbyS[i];
        }
        for (int i = 0; i< 13 * 65; i++) {
            info.acmTableGainSbyS[i] = acm_data.acmTableGainSbyS[i];
        }
        info.lumGain = acm_data.lumGain;
        info.satGain = acm_data.satGain;
        info.hueGain = acm_data.hueGain;
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        mBaseParameter->set_acm_info(&info);
        pq_factory_info factory_info;
        mBaseParameter->get_pq_factory_info(&factory_info);
        factory_info.cur_acm_index = index;
        mBaseParameter->set_pq_factory_info(&factory_info);
        updatePQTimeline();
        free(config);
    }

    return 0;
}

static int hw_output_set_dci_mode(struct hw_output_device* dev, int dpy, int index)
{
    UN_USED(dpy);
    pq_setting_config* config = getPQConfig(PQ_SETTING_CONFIG_JSON_PATH);
    if(config) {
        dci_info info;
        dci dci_data = config->pq_tuning_param.dci[index];
        info.dciEnable = dci_data.dciEnable;
        for (int i = 0; i< 33; i++) {
            info.dciWgtCoef_low[i] = dci_data.dciWgtCoef_low[i];
        }
        for (int i = 0; i< 33; i++) {
            info.dciWgtCoef_mid[i] = dci_data.dciWgtCoef_mid[i];
        }
        for (int i = 0; i< 33; i++) {
            info.dciWgtCoef_high[i] = dci_data.dciWgtCoef_high[i];
        }
        for (int i = 0; i< 32; i++) {
            info.dciWeight_low[i] = dci_data.dciWeight_low[i];
        }
        for (int i = 0; i< 32; i++) {
            info.dciWeight_mid[i] = dci_data.dciWeight_mid[i];
        }
        for (int i = 0; i< 32; i++) {
            info.dciWeight_high[i] = dci_data.dciWeight_high[i];
        }
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        mBaseParameter->set_dci_info(&info);
        pq_factory_info factory_info;
        mBaseParameter->get_pq_factory_info(&factory_info);
        factory_info.cur_dci_index = index;
        mBaseParameter->set_pq_factory_info(&factory_info);
        updatePQTimeline();
        free(config);
   }

    return 0;
}

static int hw_output_set_sharp_mode(struct hw_output_device* dev, int dpy, int index)
{
    UN_USED(dpy);
    pq_setting_config* config = getPQConfig(PQ_SETTING_CONFIG_JSON_PATH);
    if(config) {
        pq_sharp_info info;
        sharp sharp_data = config->pq_tuning_param.sharp[index];
        info.sharpEnable = sharp_data.sharpEnable;
        info.sharpPeakingGain = sharp_data.sharpPeakingGain;
        info.sharpEnableShootCtrl = sharp_data.sharpEnableShootCtrl;
        info.sharpShootCtrlOver = sharp_data.sharpShootCtrlOver;
        info.sharpShootCtrlUnder = sharp_data.sharpShootCtrlUnder;
        info.sharpEnableCoringCtrl = sharp_data.sharpEnableCoringCtrl;
        info.sharpCoringCtrlRatio[0] = sharp_data.sharpCoringCtrlRatio0;
        info.sharpCoringCtrlRatio[1] = sharp_data.sharpCoringCtrlRatio1;
        info.sharpCoringCtrlRatio[2] = sharp_data.sharpCoringCtrlRatio2;
        info.sharpCoringCtrlRatio[3] = sharp_data.sharpCoringCtrlRatio3;
        info.sharpCoringCtrlZero[0] = sharp_data.sharpCoringCtrlZero0;
        info.sharpCoringCtrlZero[1] = sharp_data.sharpCoringCtrlZero1;
        info.sharpCoringCtrlZero[2] = sharp_data.sharpCoringCtrlZero2;
        info.sharpCoringCtrlZero[3] = sharp_data.sharpCoringCtrlZero3;
        info.sharpCoringCtrlThrd[0] = sharp_data.sharpCoringCtrlThrd0;
        info.sharpCoringCtrlThrd[1] = sharp_data.sharpCoringCtrlThrd1;
        info.sharpCoringCtrlThrd[2] = sharp_data.sharpCoringCtrlThrd2;
        info.sharpCoringCtrlThrd[3] = sharp_data.sharpCoringCtrlThrd3;
        info.sharpEnableGainCtrl = sharp_data.sharpEnableGainCtrl;
        info.sharpGainCtrlPos[0] = sharp_data.sharpGainCtrlPos0;
        info.sharpGainCtrlPos[1] = sharp_data.sharpGainCtrlPos1;
        info.sharpGainCtrlPos[2] = sharp_data.sharpGainCtrlPos2;
        info.sharpGainCtrlPos[3] = sharp_data.sharpGainCtrlPos3;
        info.sharpEnableLimitCtrl = sharp_data.sharpEnableLimitCtrl;
        info.sharpLimitCtrlPos0[0] = sharp_data.sharpLimitCtrlPos00;
        info.sharpLimitCtrlPos0[1] = sharp_data.sharpLimitCtrlPos01;
        info.sharpLimitCtrlPos0[2] = sharp_data.sharpLimitCtrlPos02;
        info.sharpLimitCtrlPos0[3] = sharp_data.sharpLimitCtrlPos03;
        info.sharpLimitCtrlPos1[0] = sharp_data.sharpLimitCtrlPos10;
        info.sharpLimitCtrlPos1[1] = sharp_data.sharpLimitCtrlPos11;
        info.sharpLimitCtrlPos1[2] = sharp_data.sharpLimitCtrlPos12;
        info.sharpLimitCtrlPos1[3] = sharp_data.sharpLimitCtrlPos13;
        info.sharpLimitCtrlBndPos[0] = sharp_data.sharpLimitCtrlBndPos0;
        info.sharpLimitCtrlBndPos[1] = sharp_data.sharpLimitCtrlBndPos1;
        info.sharpLimitCtrlBndPos[2] = sharp_data.sharpLimitCtrlBndPos2;
        info.sharpLimitCtrlBndPos[3] = sharp_data.sharpLimitCtrlBndPos3;
        info.sharpLimitCtrlRatio[0] = sharp_data.sharpLimitCtrlRatio0;
        info.sharpLimitCtrlRatio[1] = sharp_data.sharpLimitCtrlRatio1;
        info.sharpLimitCtrlRatio[2] = sharp_data.sharpLimitCtrlRatio2;
        info.sharpLimitCtrlRatio[3] = sharp_data.sharpLimitCtrlRatio3;
        info.cur_sharp_index = index;
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        mBaseParameter->set_pq_sharp_info(&info);
        updatePQTimeline();
        free(config);
   }

    return 0;
}

static int hw_output_set_gamma_mode(struct hw_output_device* dev, int dpy, int index)
{
    UN_USED(dpy);
    gamma_lut_data gamma;
    AutoPQ::get_auto_gamma(index, &gamma);
    pq_setting_config* config = getPQConfig(PQ_SETTING_CONFIG_JSON_PATH);
    if(gamma.size != 1024) {
        if (!config) {
            return -EPERM;
        }
        for (int i =0; i< 1024; i++) {
            gamma.lred[i] = config->pq_tuning_param.gamma[index].gammaTab_R[i];
            gamma.lgreen[i] = config->pq_tuning_param.gamma[index].gammaTab_G[i];
            gamma.lblue[i] = config->pq_tuning_param.gamma[index].gammaTab_B[i];
        }
    }
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    csc_info csc;
    mBaseParameter->get_csc_info(&csc);
    DrmConnector* mConnector = getValidDrmConnector(priv, dpy);
    int crtc_id = 0;
    if (mConnector)
        crtc_id = priv->drm_->GetCrtcFromConnector(mConnector)->id();
    mBaseParameter->set_pq_tuning_gamma(&gamma);
    DrmGamma::gamma_color_temp_adjust(gamma.lred, gamma.lgreen, gamma.lblue, csc.cscRGain, csc.cscGGain, csc.cscBGain);
    int ret = DrmGamma::set_3x1d_gamma(priv->drm_->fd(), crtc_id, 1024, gamma.lred, gamma.lgreen, gamma.lblue);
    mBaseParameter->set_gamma_lut_data(mConnector->get_type(), mConnector->connector_id(), &gamma);
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    info.cur_gamma_index = index;
    mBaseParameter->set_pq_factory_info(&info);
    free(config);
    return 0;
}

static int hw_output_set_3d_lut_mode(struct hw_output_device* dev, int dpy, int index)
{
    UN_USED(dpy);
    cubic_lut_data cublic;
    AutoPQ::get_auto_3d_lut(index, &cublic);
    if (cublic.size != 4913) {
        return -EPERM;
    }
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    hw_output_set_3d_lut(dev, dpy, 4913, cublic.lred, cublic.lgreen, cublic.lblue);
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    info.cur_cubic_index = index;
    mBaseParameter->set_pq_factory_info(&info);
    return 0;
}

static int hw_output_get_bcsh_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *index = info.cur_bcsh_index;
    return 0;
}

static int hw_output_get_white_balance_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *index = info.cur_white_balance_index;
    return 0;
}

static int hw_output_get_acm_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *index = info.cur_acm_index;
    return 0;
}

static int hw_output_get_dci_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *index = info.cur_dci_index;
    return 0;
}

static int hw_output_get_sharp_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_sharp_info info;
    mBaseParameter->get_pq_sharp_info(&info);
    *index = info.cur_sharp_index;
    return 0;
}

static int hw_output_get_gamma_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *index = info.cur_gamma_index;
    return 0;
}

static int hw_output_get_3d_lut_mode(struct hw_output_device* dev, int dpy, int* index)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *index = info.cur_cubic_index;
    return 0;
}

static int hw_output_get_preset_bcsh(struct hw_output_device* dev, int dpy, int path,
    int index, uint32_t* brightness, uint32_t* contrast, uint32_t* saturation, uint32_t* hue)
{
    UN_USED(dpy);
    if(path != PATH_BASEPARAMETER) {
        return -EPERM;
    }
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    *brightness = info.bcsh[index].brightness;
    *contrast = info.bcsh[index].contrast;
    *saturation = info.bcsh[index].saturation;
    *hue = info.bcsh[index].hue;

    return 0;
}

static int hw_output_set_preset_bcsh(struct hw_output_device* dev, int dpy, int path,
    int index, uint32_t brightness, uint32_t contrast, uint32_t saturation, uint32_t hue)
{
    UN_USED(dpy);
    if(path != PATH_BASEPARAMETER) {
        return -EPERM;
    }
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_factory_info info;
    mBaseParameter->get_pq_factory_info(&info);
    info.bcsh[index].brightness = brightness;
    info.bcsh[index].contrast = contrast;
    info.bcsh[index].saturation = saturation;
    info.bcsh[index].hue = hue;
    mBaseParameter->set_pq_factory_info(&info);
    return 0;
}

static int hw_output_get_preset_white_balance(struct hw_output_device* dev, int dpy, int path,
    int index, uint32_t* rgain, uint32_t* ggain, uint32_t* bgain)
{
    UN_USED(dpy);
    if(path != PATH_BASEPARAMETER && path != PATH_AUTOPQ) {
        return -EPERM;
    }
    if (path == PATH_BASEPARAMETER) {
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        pq_factory_info info;
        mBaseParameter->get_pq_factory_info(&info);
        *rgain = info.white_balance[index].rgain;
        *ggain = info.white_balance[index].ggain;
        *bgain = info.white_balance[index].bgain;
    } else {
        white_balance_data data;
        AutoPQ::get_auto_white_balance(index, &data);
        *rgain = data.rgain;
        *ggain = data.ggain;
        *bgain = data.bgain;
    }

    return 0;
}

static int hw_output_set_preset_white_balance(struct hw_output_device* dev, int dpy, int path,
    int index, uint32_t rgain, uint32_t ggain, uint32_t bgain)
{
    UN_USED(dpy);
    if(path != PATH_BASEPARAMETER && path != PATH_AUTOPQ) {
        return -EPERM;
    }
    if (path == PATH_BASEPARAMETER) {
        hw_output_private_t* priv = (hw_output_private_t*)dev;
        BaseParameter* mBaseParameter = priv->mBaseParmeter;
        pq_factory_info info;
        mBaseParameter->get_pq_factory_info(&info);
        info.white_balance[index].rgain = rgain;
        info.white_balance[index].ggain = ggain;
        info.white_balance[index].bgain = bgain;
         mBaseParameter->set_pq_factory_info(&info);
    } else {
        white_balance_data data;
        data.rgain = rgain;
        data.ggain = ggain;
        data.bgain = bgain;
        AutoPQ::set_auto_white_balance(index, &data);
    }

    return 0;
}

static int hw_output_get_preset_gamma(struct hw_output_device* dev, int dpy, int path, int index,
    uint32_t* size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    UN_USED(dpy);
    if(path != PATH_AUTOPQ) {
        return -EPERM;
    }
    gamma_lut_data gamma;
    AutoPQ::get_auto_gamma(index, &gamma);
    for (int i =0; i < gamma.size; i++) {
        r[i] = gamma.lred[i];
        g[i] = gamma.lgreen[i];
        b[i] = gamma.lblue[i];
    }
    *size = gamma.size;
    return 0;
}

static int hw_output_set_preset_gamma(struct hw_output_device* dev, int dpy, int path, int index,
    uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    UN_USED(dev);
    UN_USED(dpy);
    if(path != PATH_AUTOPQ) {
        return -EPERM;
    }
    gamma_lut_data gamma;
    for(int i = 0; i< size; i++){
        gamma.lred[i] = r[i];
        gamma.lgreen[i] = g[i];
        gamma.lblue[i] = b[i];
    }
    gamma.size = size;
    AutoPQ::set_auto_gamma(index, &gamma);
    return 0;
}

static int hw_output_get_preset_3d_lut(struct hw_output_device* dev, int dpy, int path, int index,
    uint32_t* size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    UN_USED(dpy);
    if(path != PATH_AUTOPQ) {
        return -EPERM;
    }
    cubic_lut_data cubic;
    AutoPQ::get_auto_3d_lut(index, &cubic);
    for (int i =0; i < cubic.size; i++) {
        r[i] = cubic.lred[i];
        g[i] = cubic.lgreen[i];
        b[i] = cubic.lblue[i];
    }
    *size = cubic.size;
    return 0;
}

static int hw_output_set_preset_3d_lut(struct hw_output_device* dev, int dpy, int path, int index,
    uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
    UN_USED(dev);
    UN_USED(dpy);
    if(path != PATH_AUTOPQ) {
       return -EPERM;
    }
    cubic_lut_data cubic;
    for(int i = 0; i< size; i++){
        cubic.lred[i] = r[i];
        cubic.lgreen[i] = g[i];
        cubic.lblue[i] = b[i];
    }
    cubic.size = size;
    AutoPQ::set_auto_3d_lut(index, &cubic);
    return 0;
}

uint16_t format_3d_lut_data(uint16_t in) {
	return in * 4;
}

static int hw_output_set_3d_lut_path(struct hw_output_device* dev,
    int dpy, const char* path) {
    UN_USED(dev);
    UN_USED(dpy);
    int b, g, r;
    int index = 0;
    uint16_t red[17 * 17 * 17];
    uint16_t green[17 * 17 * 17];
    uint16_t blue[17 * 17 * 17];
    FILE *fp = NULL;
    char buf[30];
    fp = fopen(path, "r");
    if (fp == NULL) {
        ALOGE("hw_output_set_3d_lut_path open path fail");
        return -1;
    }
    while(!feof(fp) && index < 17 * 17 * 17)
    {
        char* p = fgets(buf, sizeof(buf), fp);
        if (p != NULL) {
            sscanf(buf, "%d,%d,%d", &b, &g, &r);
            red[index] = format_3d_lut_data(r);
            green[index] = format_3d_lut_data(g);
            blue[index] = format_3d_lut_data(b);
            index++;
        }
    }
    if(17 * 17 * 17 == index) {
        hw_output_set_3d_lut(dev, dpy, index, red, green, blue);
    }
    if (fp != NULL) {
        fclose(fp);
    }
    return 0;
}

static int hw_output_set_sharp_peaking_gain(struct hw_output_device* dev, int dpy, int value)
{
    UN_USED(dpy);
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_sharp_info info;
    mBaseParameter->get_pq_sharp_info(&info);
    info.sharpPeakingGain = value;
    mBaseParameter->set_pq_sharp_info(&info);
    updatePQTimeline();

    return 0;
}

static int hw_output_get_sharp_peaking_gain(struct hw_output_device* dev, int dpy, int* value)
{
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    pq_sharp_info info;
    mBaseParameter->get_pq_sharp_info(&info);
    *value = info.sharpPeakingGain;

    return 0;
}

static int hw_output_set_aipq_enable(struct hw_output_device* dev, bool aisd, bool aisr)
{
    int ret = 0;
    bool enable = aisd || aisr;
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    BaseParameter* mBaseParameter = priv->mBaseParmeter;
    acm_info acmInfo;
    ret = mBaseParameter->get_acm_info(&acmInfo);
    acmInfo.acmEnable = enable;
    ret = mBaseParameter->set_acm_info(&acmInfo);
    dci_info dciInfo;
    ret = mBaseParameter->get_dci_info(&dciInfo);
    dciInfo.dciEnable = enable;
    ret = mBaseParameter->set_dci_info(&dciInfo);
    pq_sharp_info sharpInfo;
    ret = mBaseParameter->get_pq_sharp_info(&sharpInfo);
    sharpInfo.sharpEnable = enable;
    ret = mBaseParameter->set_pq_sharp_info(&sharpInfo);
    aipq_info aipq;
    ret = mBaseParameter->get_aipq_info(&aipq);
    aipq.aiSDEnable = aisd;
    aipq.aiSREnable = aisr;
    ret = mBaseParameter->set_aipq_info(&aipq);
    updatePQTimeline();
    ret = hw_output_set_pq_enable(dev, enable);
    return ret;
}

static int hw_output_set_hdcp_enable(struct hw_output_device* dev, int dpy, bool enable)
{
    int ret = 0;
#ifdef USE_HWC_PROXY_SERVICE
    ret  = mClient.set_hdcp_enable(dpy, enable);
#else
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    ret = DrmHdcp::set_hdcp_enable(priv->drm_->fd(), dpy, enable);
#endif
    return ret;
}

static int hw_output_get_hdcp_enable_status(struct hw_output_device* dev, int dpy, int* status)
{
#ifdef USE_HWC_PROXY_SERVICE
    *status = mClient.get_hdcp_enable_status(dpy);
#else
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    *status = DrmHdcp::get_hdcp_enable_status(priv->drm_->fd(), dpy);
#endif
    return 0;
}

static int hw_output_set_hdcp_type(struct hw_output_device* dev, int dpy, int type)
{
    int ret = 0;
#ifdef USE_HWC_PROXY_SERVICE
    ret = mClient.set_hdcp_type(dpy, type);
#else
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    ret = DrmHdcp::set_hdcp_type(priv->drm_->fd(), dpy, type);
#endif
    return ret;
}

static int hw_output_get_hdcp_encrypted_status(struct hw_output_device* dev, int dpy, int* status)
{
#ifdef USE_HWC_PROXY_SERVICE
    *status = mClient.get_hdcp_encrypted_status(dpy);
#else
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    *status = DrmHdcp::get_hdcp_encrypted_status(priv->drm_->fd(), dpy);
#endif
    return 0;
}

static int hw_output_set_dvi_status(struct hw_output_device* dev, int dpy, int value)
{
    int ret = 0;
#ifdef USE_HWC_PROXY_SERVICE
    ret = mClient.set_dvi_status(dpy, value);
#else
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    ret = DrmHdcp::set_dvi_status(priv->drm_->fd(), dpy, value);
#endif
    return ret;
}

static int hw_output_get_dvi_status(struct hw_output_device* dev, int dpy, int* value)
{
#ifdef USE_HWC_PROXY_SERVICE
    *value = mClient.get_dvi_status(dpy);
#else
    hw_output_private_t* priv = (hw_output_private_t*)dev;
    *value = DrmHdcp::get_dvi_status(priv->drm_->fd(), dpy);
#endif
    return 0;
}

static int hw_output_device_open(const struct hw_module_t* module,
        const char* name, struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HW_OUTPUT_DEFAULT_DEVICE)) {
        hw_output_private_t* dev = (hw_output_private_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        //memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HW_OUTPUT_DEVICE_API_VERSION_0_1;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hw_output_device_close;

        dev->device.initialize = hw_output_initialize;
        dev->device.setMode = hw_output_set_mode;
        dev->device.set3DMode = hw_output_set_3d_mode;
        dev->device.setBrightness = hw_output_set_brightness;
        dev->device.setContrast = hw_output_set_contrast;
        dev->device.setSat = hw_output_set_sat;
        dev->device.setHue = hw_output_set_hue;
        dev->device.setColorMode = hw_output_set_color_mode;
        dev->device.setHdrMode = hw_output_set_hdr_mode;
        dev->device.setGamma = hw_output_set_gamma;
        dev->device.setScreenScale = hw_output_set_screen_scale;

        dev->device.getCurColorMode = hw_output_get_cur_color_mode;
        dev->device.getBcsh = hw_output_get_bcsh;
        dev->device.getBuiltIn = hw_output_get_builtin;
        dev->device.getColorConfigs = hw_output_get_color_configs;
        dev->device.getConnectorState = hw_output_get_connector_state;
        dev->device.getCurMode = hw_output_get_cur_mode;
        dev->device.getDisplayModes = hw_output_get_display_modes;
        dev->device.getNumConnectors = hw_output_get_num_connectors;
        dev->device.getOverscan = hw_output_get_overscan;

        dev->device.hotplug = hw_output_hotplug_update;
        dev->device.saveConfig = hw_output_save_config;
        dev->device.set3DLut = hw_output_set_3d_lut;
        dev->device.getConnectorInfo = hw_output_get_connector_info;
        dev->device.updateDispHeader = hw_output_update_disp_header;
        dev->device.getModeState = hw_output_get_mode_state;
        dev->device.setModeState = hw_output_set_mode_state;
        dev->device.getHdrResolutionSupported = hw_output_get_hdr_resolution_supported;
        dev->device.setSWBrightness = hw_output_set_sw_brightness;
        dev->device.getSWBrightness = hw_output_get_sw_brightness;
        dev->device.setSWContrast = hw_output_set_sw_contrast;
        dev->device.getSWContrast = hw_output_get_sw_contrast;
        dev->device.setSWSaturation = hw_output_set_sw_saturation;
        dev->device.getSWSaturation = hw_output_get_sw_saturation;
        dev->device.setSWHue = hw_output_set_sw_hue;
        dev->device.getSWHue = hw_output_get_sw_hue;
        dev->device.setRGain = hw_output_set_rgain;
        dev->device.getRGain = hw_output_get_rgain;
        dev->device.setGGain = hw_output_set_ggain;
        dev->device.getGGain = hw_output_get_ggain;
        dev->device.setBGain = hw_output_set_bgain;
        dev->device.getBGain = hw_output_get_bgain;
        dev->device.setCscEnable = hw_output_set_csc_enable;
        dev->device.setDciEnable = hw_output_set_dci_enable;
        dev->device.setAcmEnable = hw_output_set_acm_enable;
        dev->device.setPqEnable = hw_output_set_pq_enable;
        dev->device.setBCSHMode = hw_output_set_bcsh_mode;
        dev->device.setWhiteBalanceMode = hw_output_set_white_balance_mode;
        dev->device.setAcmMode = hw_output_set_acm_mode;
        dev->device.setDciMode = hw_output_set_dci_mode;
        dev->device.setGammaMode = hw_output_set_gamma_mode;
        dev->device.set3DLutMode = hw_output_set_3d_lut_mode;
        dev->device.getBCSHMode = hw_output_get_bcsh_mode;
        dev->device.getWhiteBalanceMode = hw_output_get_white_balance_mode;
        dev->device.getAcmMode = hw_output_get_acm_mode;
        dev->device.getDciMode = hw_output_get_dci_mode;
        dev->device.getGammaMode = hw_output_get_gamma_mode;
        dev->device.get3DLutMode = hw_output_get_3d_lut_mode;
        dev->device.setPresetBcsh = hw_output_set_preset_bcsh;
        dev->device.getPresetBcsh = hw_output_get_preset_bcsh;
        dev->device.setPresetWhiteBalance = hw_output_set_preset_white_balance;
        dev->device.getPresetWhiteBalance = hw_output_get_preset_white_balance;
        dev->device.setPresetGamma = hw_output_set_preset_gamma;
        dev->device.getPresetGamma = hw_output_get_preset_gamma;
        dev->device.setPreset3DLut = hw_output_set_preset_3d_lut;
        dev->device.getPreset3DLut = hw_output_get_preset_3d_lut;
        dev->device.setWhiteBalance = hw_output_set_white_balance;
        dev->device.set3DLutPath = hw_output_set_3d_lut_path;
        dev->device.setSharpEnable = hw_output_set_sharp_enable;
        dev->device.setSharpMode = hw_output_set_sharp_mode;
        dev->device.getSharpMode = hw_output_get_sharp_mode;
        dev->device.setSharpPeakingGain = hw_output_set_sharp_peaking_gain;
        dev->device.getSharpPeakingGain = hw_output_get_sharp_peaking_gain;
        dev->device.setAiPqEnable = hw_output_set_aipq_enable;
        dev->device.getPqEnable = hw_output_get_pq_enable;
        dev->device.getCscEnable = hw_output_get_csc_enable;
        dev->device.getAcmEnable = hw_output_get_acm_enable;
        dev->device.getDciEnable = hw_output_get_dci_enable;
        dev->device.getSharpEnable = hw_output_get_sharp_enable;
        dev->device.setHdcpEnable = hw_output_set_hdcp_enable;
        dev->device.getHdcpEnableStatus = hw_output_get_hdcp_enable_status;
        dev->device.setHdcpType = hw_output_set_hdcp_type;
        dev->device.getHdcpEncryptedStatus = hw_output_get_hdcp_encrypted_status;
        dev->device.setDviStatus = hw_output_set_dvi_status;
        dev->device.getDviStatus = hw_output_get_dvi_status;
        *device = &dev->device.common;
        status = 0;
    }
    return status;
}
