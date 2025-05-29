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

#include <cutils/properties.h>
#include "DrmApi.h"

DrmApi::DrmApi(){
}

DrmApi::~DrmApi(){
}

int DrmApi::get_property_id(int fd, drmModeObjectProperties *props, const char *name) {
    drmModePropertyPtr property;
    uint32_t i, id = 0;
    /* find property according to the name */
    for (i = 0; i < props->count_props; i++) {
        property = drmModeGetProperty(fd, props->props[i]);
        if (!strcmp(property->name, name))
            id = property->prop_id;
        drmModeFreeProperty(property);

        if (id)
            break;
    }
    return id;
}

int DrmApi::get_property_index(int fd, drmModeObjectProperties *props, const char *name) {
    drmModePropertyPtr property;
    int index = 0;
    /* find property according to the name */
    for (int i = 0; i < props->count_props; i++) {
        property = drmModeGetProperty(fd, props->props[i]);
        if (!strcmp(property->name, name)) {
            index = i;
            drmModeFreeProperty(property);
            break;
        }
        drmModeFreeProperty(property);
    }
    return index;
}

drmModeConnectorPtr DrmApi::get_connector(int fd, int dpy) {
    if (fd <= 0) {
        return NULL;
    }
    drmModeResPtr res = drmModeGetResources(fd);
    drmModeConnectorPtr c;
    int index = 0;
    for (int i = 0; i < res->count_connectors; i++) {
        c = drmModeGetConnector(fd, res->connectors[i]);
        if (!c || c->connector_type == DRM_MODE_CONNECTOR_WRITEBACK) {
            //ALOGD("Could not get connector %u: %s\n", res->connectors[i], strerror(errno));
            continue;
        }

        if (index == dpy) {
            drmModeFreeResources(res);
            return c;
        }
        index++;
    }
    drmModeFreeConnector(c);
    drmModeFreeResources(res);
    return NULL;
}

int DrmApi::drm_proc(int fd, const RkHwcProxyAidlRequest& request, RkHwcProxyAidlResponse* response) {
    response->id = request.id;
    switch(request.id) {
        case CMD_GET_RESOLUTION: {
            char resolution[256];
            int dpy = request.data[0];
            ALOGD("drm_proc get_resolution dpy: %d", dpy);
            get_resolution(fd, dpy, resolution);
            ALOGD("drm_proc get_resolution: %s",  resolution);
            std::vector<uint8_t> responseValue(256);
            for (int i = 0; i < 256; i++) {
                responseValue[i] = resolution[i];
            }
            response->value = responseValue;
            break;
        }
        case CMD_SET_HDCP_ENABLE: {
            int dpy = request.data[0];
            bool enable = request.data[1];
            ALOGD("drm_proc set hdcp enable dpy:%d enable %d", dpy, enable);
            int ret = set_hdcp_enable(fd, dpy, enable);
            ALOGD("set_hdcp_enable ret %d", ret);
            std::vector<uint8_t> responseValue(1);
            ret = 0;
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_GET_HDCP_ENABLE_STATUS: {
            int dpy = request.data[0];
            ALOGD("drm_proc get hdcp status dpy:%d", dpy);
            int ret = get_hdcp_enable_status(fd, dpy);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_SET_HDCP_TYPE: {
            int dpy = request.data[0];
            int type = request.data[1];
            ALOGD("drm_proc set hdcp type dpy:%d type:%d", dpy, type);
            int ret = set_hdcp_type(fd, dpy, type);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_GET_HDCP_ENCRYPTED_STATUS: {
            int dpy = request.data[0];
            ALOGD("drm_proc get hdcp encrypted status dpy:%d", dpy);
            int ret = get_hdcp_encrypted_status(fd, dpy);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_SET_GAMMA: {
            uint8_t buffer[request.req_len];
            memcpy(buffer, &request.data[0], request.data.size());
            int ret = set_gamma(fd, ((drm_gamma*)buffer)->dpy, ((drm_gamma*)buffer)->size,
                ((drm_gamma*)buffer)->red, ((drm_gamma*)buffer)->green, ((drm_gamma*)buffer)->blue);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_SET_3DLUT: {
            uint8_t buffer[request.req_len];
            memcpy(buffer, &request.data[0], request.data.size());
            int ret = set_3d_lut(fd, ((drm_cubic*)buffer)->dpy, ((drm_cubic*)buffer)->size,
                ((drm_cubic*)buffer)->red, ((drm_cubic*)buffer)->green, ((drm_cubic*)buffer)->blue);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_SET_DVI_STATUS: {
            int dpy = request.data[0];
            int value = request.data[1];
            ALOGD("drm_proc set dvi status dpy:%d value:%d", dpy, value);
            int ret = set_dvi_status(fd, dpy, value);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
        case CMD_GET_DVI_STATUS: {
            int dpy = request.data[0];
            ALOGD("drm_proc get dvi status dpy:%d", dpy);
            int ret = get_dvi_status(fd, dpy);
            std::vector<uint8_t> responseValue(1);
            responseValue[0] = -ret;
            response->value = responseValue;
            break;
        }
    }
    return 0;
}

int DrmApi::get_resolution(int fd, int dpy, char* resolution) {
    int ret = 0, value = 0, crtc_id = 0;
    float vfresh;
    bool found = false;
    drmModeEncoderPtr encoder = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    drmModePropertyBlobPtr blob = NULL;
    drmModePropertyPtr p = NULL;
    drmModeConnectorPtr connector = NULL;
    drmModeModeInfoPtr drm_mode;
    connector = get_connector(fd, dpy);
    if (connector) {
        encoder = drmModeGetEncoder(fd, connector->encoders[0]);
        crtc_id = encoder->crtc_id;
        props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
        for (int i = 0; !found && i < props->count_props; i++) {
            p = drmModeGetProperty(fd, props->props[i]);
            if (!strcmp(p->name, "MODE_ID")) {
                found = true;
                if (!drm_property_type_is(p, DRM_MODE_PROP_BLOB)) {
                    ALOGE("%s:line=%d,is not blob",__FUNCTION__,__LINE__);
                    ret = -1;
                    goto error;
                }
                if (!p->count_blobs)
                    value = props->prop_values[i];
                else
                    value = p->blob_ids[0];
                blob = drmModeGetPropertyBlob(fd, value);
                if (!blob) {
                    ALOGE("%s:line=%d, blob is null",__FUNCTION__,__LINE__);
                    ret = -1;
                    goto error;
                }
                drm_mode = (drmModeModeInfoPtr)blob->data;
                if (drm_mode->flags & DRM_MODE_FLAG_INTERLACE)
                    vfresh = drm_mode->clock *2/ (float)(drm_mode->vtotal * drm_mode->htotal) * 1000.0f;
                else
                    vfresh = drm_mode->clock / (float)(drm_mode->vtotal * drm_mode->htotal) * 1000.0f;
                    ALOGD("get_resolution: crtc_id=%d clock=%d w=%d %d %d %d %d %d flag=0x%x vfresh %.2f drm.vrefresh=%.2f",
                        crtc_id, drm_mode->clock, drm_mode->htotal, drm_mode->hsync_start,
                    drm_mode->hsync_end, drm_mode->vtotal, drm_mode->vsync_start, drm_mode->vsync_end, drm_mode->flags,
                    vfresh, (float)drm_mode->vrefresh);
                    sprintf(resolution, "%dx%d@%.2f-%d-%d-%d-%d-%d-%d-%x-%d", drm_mode->hdisplay, drm_mode->vdisplay, vfresh,
                    drm_mode->hsync_start, drm_mode->hsync_end, drm_mode->htotal,
                    drm_mode->vsync_start, drm_mode->vsync_end, drm_mode->vtotal,
                    (drm_mode->flags&0xFFFF), drm_mode->clock);
            }
        }
    }
error:
    if(encoder)
        drmModeFreeEncoder(encoder);
    if(blob)
        drmModeFreePropertyBlob(blob);
    if(p)
        drmModeFreeProperty(p);
    if(props)
        drmModeFreeObjectProperties(props);
    if(connector)
        drmModeFreeConnector(connector);
    return ret;
}

int DrmApi::set_gamma(int fd, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b) {
    unsigned blob_id = 0;
    int ret = 0, crtc_id = 0;
    drmModeEncoderPtr encoder = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    drmModeConnectorPtr connector = NULL;    struct drm_color_lut gamma_lut[size];
    for (int i = 0; i < size; i++) {
        gamma_lut[i].red = r[i];
        gamma_lut[i].green = g[i];
        gamma_lut[i].blue = b[i];
    };
    connector = get_connector(fd, dpy);
    if (connector) {
        encoder = drmModeGetEncoder(fd, connector->encoders[0]);
        if (encoder) {
            crtc_id = encoder->crtc_id;
            if (crtc_id != 0) {
                props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
                uint32_t property_id = get_property_id(fd, props, "GAMMA_LUT");
                if(property_id != 0) {
                    drmModeCreatePropertyBlob(fd, gamma_lut, sizeof(gamma_lut), &blob_id);
                    ret = drmModeObjectSetProperty(fd, crtc_id, DRM_MODE_OBJECT_CRTC, property_id, blob_id);
                    drmModeDestroyPropertyBlob(fd, blob_id);
                }
            }
        }
    }
    if(encoder)
        drmModeFreeEncoder(encoder);
    if(props)
        drmModeFreeObjectProperties(props);
    if(connector)
        drmModeFreeConnector(connector);
    return ret;
}

int DrmApi::set_3d_lut(int fd, int dpy, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b) {
    unsigned blob_id = 0;
    int ret = 0, crtc_id = 0;
    drmModeEncoderPtr encoder = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    drmModeConnectorPtr connector = NULL;    struct drm_color_lut gamma_lut[size];
    for (int i = 0; i < size; i++) {
        gamma_lut[i].red = r[i];
        gamma_lut[i].green = g[i];
        gamma_lut[i].blue = b[i];
    };
    connector = get_connector(fd, dpy);
    if (connector) {
        encoder = drmModeGetEncoder(fd, connector->encoders[0]);
        if (encoder) {
            crtc_id = encoder->crtc_id;
            if (crtc_id != 0) {
                props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
                uint32_t property_id = get_property_id(fd, props, "CUBIC_LUT");
                if(property_id != 0) {
                    drmModeCreatePropertyBlob(fd, gamma_lut, sizeof(gamma_lut), &blob_id);
                    ret = drmModeObjectSetProperty(fd, crtc_id, DRM_MODE_OBJECT_CRTC, property_id, blob_id);
                    drmModeDestroyPropertyBlob(fd, blob_id);
                }
            }
        }
    }
    if(encoder)
        drmModeFreeEncoder(encoder);
    if(props)
        drmModeFreeObjectProperties(props);
    if(connector)
        drmModeFreeConnector(connector);
    return ret;
}

int DrmApi::set_hdcp_enable(int fd, int dpy, bool enable) {
    int ret = 0;
    drmModeConnectorPtr connector = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    connector = get_connector(fd, dpy);
    props = drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    int prop_id = get_property_id(fd, props, "Content Protection");
    if (enable)
        ret = drmModeObjectSetProperty(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR,
            prop_id, DRM_MODE_CONTENT_PROTECTION_DESIRED);
    else
        ret = drmModeObjectSetProperty(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR,
            prop_id, DRM_MODE_CONTENT_PROTECTION_UNDESIRED);
    if(connector)
        drmModeFreeConnector(connector);
    if(props)
        drmModeFreeObjectProperties(props);
    return ret;
}
int DrmApi::get_hdcp_enable_status(int fd, int dpy) {
    int ret = 0;
    drmModeConnectorPtr connector = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    connector = get_connector(fd, dpy);
    props = drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    int index = get_property_index(fd, props, "Content Protection");
    uint64_t prop_value = props->prop_values[index];
    if (prop_value == DRM_MODE_CONTENT_PROTECTION_ENABLED) {
        ret = 1;
    } else {
        ret = 0;
    }
    if(connector)
        drmModeFreeConnector(connector);
    if(props)
        drmModeFreeObjectProperties(props);
    return ret;
}
int DrmApi::set_hdcp_type(int fd, int dpy, int type) {
    int ret = 0;
    drmModeConnectorPtr connector = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    connector = get_connector(fd, dpy);
    props = drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    int prop_id = get_property_id(fd, props, "HDCP Content Type");
    drmModeObjectSetProperty(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR, prop_id, type);
    if(connector)
        drmModeFreeConnector(connector);
    if(props)
        drmModeFreeObjectProperties(props);
    return ret;
}
int DrmApi::get_hdcp_encrypted_status(int fd, int dpy) {
    drmModeConnectorPtr connector = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    connector = get_connector(fd, dpy);
    props = drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    int index = get_property_index(fd, props, "hdcp_encrypted");
    uint64_t prop_value = props->prop_values[index];
    if(connector)
        drmModeFreeConnector(connector);
    if(props)
        drmModeFreeObjectProperties(props);
    return prop_value;
}

int DrmApi::set_dvi_status(int fd, int dpy, int value) {
    int ret = 0;
    drmModeConnectorPtr connector = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    connector = get_connector(fd, dpy);
    props = drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    int prop_id = get_property_id(fd, props, "output_hdmi_dvi");
    ret = drmModeObjectSetProperty(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR,
        prop_id, value);
    if(connector)
        drmModeFreeConnector(connector);
    if(props)
        drmModeFreeObjectProperties(props);
    return ret;
}

int DrmApi::get_dvi_status(int fd, int dpy) {
    drmModeConnectorPtr connector = NULL;
    drmModeObjectPropertiesPtr props = NULL;
    connector = get_connector(fd, dpy);
    props = drmModeObjectGetProperties(fd, connector->connector_id, DRM_MODE_OBJECT_CONNECTOR);
    int index = get_property_index(fd, props, "output_hdmi_dvi");
    uint64_t prop_value = props->prop_values[index];
    if(connector)
        drmModeFreeConnector(connector);
    if(props)
        drmModeFreeObjectProperties(props);
    return prop_value;
}
