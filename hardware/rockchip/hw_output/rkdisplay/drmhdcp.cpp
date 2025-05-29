/*
 * DRM based mode setting test program
 * Copyright 2008 Tungsten Graphics
 *   Jakob Bornecrantz <jakob@tungstengraphics.com>
 * Copyright 2008 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/*
 * This fairly simple test program dumps output in a similar format to the
 * "xrandr" tool everyone knows & loves.  It's necessarily slightly different
 * since the kernel separates outputs into encoder and connector structures,
 * each with their own unique ID.  The program also allows test testing of the
 * memory management and mode setting APIs by allowing the user to specify a
 * connector and mode to use for mode setting.  If all works as expected, a
 * blue background should be painted on the monitor attached to the specified
 * connector after the selected mode is set.
 *
 * TODO: use cairo to write the mode info on the selected output once
 *       the mode has been programmed, along with possible test patterns.
 */
#include "drmhdcp.h"

namespace android {

DrmHdcp::DrmHdcp(){
}

DrmHdcp::~DrmHdcp(){
}

int DrmHdcp::get_property_id(int fd, drmModeObjectProperties *props, const char *name) {
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

int DrmHdcp::get_property_index(int fd, drmModeObjectProperties *props, const char *name) {
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

drmModeConnectorPtr DrmHdcp::get_connector(int fd, int dpy) {
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

        if (c->connection == DRM_MODE_CONNECTED && index == dpy) {
            drmModeFreeResources(res);
            return c;
        }
        index++;
    }
    drmModeFreeConnector(c);
    drmModeFreeResources(res);
    return NULL;
}

int DrmHdcp::set_hdcp_enable(int fd, int dpy, bool enable) {
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
int DrmHdcp::get_hdcp_enable_status(int fd, int dpy) {
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
int DrmHdcp::set_hdcp_type(int fd, int dpy, int type) {
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
int DrmHdcp::get_hdcp_encrypted_status(int fd, int dpy) {
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

int DrmHdcp::set_dvi_status(int fd, int dpy, int value) {
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

int DrmHdcp::get_dvi_status(int fd, int dpy) {
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

}
