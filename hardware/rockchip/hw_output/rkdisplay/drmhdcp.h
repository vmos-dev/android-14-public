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
#ifndef ANDROID_DRM_HDCP_H_
#define ANDROID_DRM_HDCP_H_

#include <errno.h>
#include <inttypes.h>
#include <log/log.h>
#include <stdint.h>
#include <string>

#include "xf86drm.h"
#include "xf86drmMode.h"
#include "drm_fourcc.h"

namespace android {

class DrmHdcp {
 public:
 DrmHdcp();
 ~DrmHdcp();
 static int set_hdcp_enable(int fd, int dpy, bool enable);
 static int get_hdcp_enable_status(int fd, int dpy);
 static int set_hdcp_type(int fd, int dpy, int type);
 static int get_hdcp_encrypted_status(int fd, int dpy);
 static int set_dvi_status(int fd, int dpy, int value);
 static int get_dvi_status(int fd, int dpy);
 private:
 static drmModeConnectorPtr get_connector(int fd, int dpy);
 static int get_property_id(int fd, drmModeObjectProperties *props, const char *name);
 static int get_property_index(int fd, drmModeObjectProperties *props, const char *name);
};
}

#endif  // ANDROID_DRM_HDCP_H_
