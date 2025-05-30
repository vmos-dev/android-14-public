/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "Lights.h"

#include <android-base/logging.h>
#include <log/log.h>
#include <unistd.h>
#include <errno.h>
#include <cutils/properties.h>

//xml parse
#include <tinyxml2.h>

#include <aidl/android/hardware/light/LightType.h>
#include <aidl/android/hardware/light/FlashMode.h>

#define XML_DISPLAY "vendor/etc/display_settings.xml"

using namespace std;

namespace aidl {
namespace android {
namespace hardware {
namespace light {

const char* getDriverPath(LightType type) {
    switch (type) {
        case LightType::BACKLIGHT:
            return "/sys/class/backlight/backlight/brightness";
        case LightType::BUTTONS:
            return "/sys/class/leds/button-backlight/brightness";
        case LightType::BATTERY:
        case LightType::NOTIFICATIONS:
        case LightType::ATTENTION:
            return "/sys/class/leds";
        case LightType::BLUETOOTH:
        case LightType::WIFI:
        case LightType::MICROPHONE:
        case LightType::KEYBOARD:
        default:
            return "/not_supported";
    }
}

static int access_rgb() {
    std::string led_path(getDriverPath(LightType::NOTIFICATIONS));
    if (access((led_path + "/led_r/brightness").c_str(), F_OK) < 0)
        return -errno;
    if (access((led_path + "/led_g/brightness").c_str(), F_OK) < 0)
        return -errno;
    if (access((led_path + "/led_b/brightness").c_str(), F_OK) < 0)
        return -errno;
    return 0;
}

static int access_rgb_blink() {
    std::string led_path(getDriverPath(LightType::NOTIFICATIONS));
    if (access((led_path + "/led_r/blink").c_str(), F_OK) < 0)
        return -errno;
    if (access((led_path + "/led_g/blink").c_str(), F_OK) < 0)
        return -errno;
    if (access((led_path + "/led_b/blink").c_str(), F_OK) < 0)
        return -errno;
    return 0;
}

static int access_backlight() {
    std::string backlight_path(getDriverPath(LightType::BACKLIGHT));
    ALOGV("backlight_path: %s", backlight_path.c_str());
    if (access(backlight_path.c_str(), F_OK) < 0) {
        ALOGE("error: %s", strerror(errno));
        return -errno;
    }
    return 0;
}

static int init_display_backlight_path(vector<BacklightPath*> *vector) {
    bool identifier = true;
    tinyxml2::XMLDocument doc;
    doc.LoadFile(XML_DISPLAY);
    tinyxml2::XMLElement *root = doc.RootElement();
    if (root == nullptr) {
        ALOGE("Display_settings.xml is not exist!");
        return 0;
    }
    for (tinyxml2::XMLElement* node = root->FirstChildElement(); node != nullptr; node = node->NextSiblingElement()) {
        const char* name = node->Name();
        const char* value = node->GetText();
        if (!strcmp(name, "config")) {
            const char* config_identifier = node->Attribute("identifier");
            ALOGI("display_settings config identifier = %d", identifier);
            if (!strcmp(config_identifier, "0")) {
                identifier = true;
            } else {
                identifier = false;
            }
        }
        if (!strcmp(name, "display")) {
            char *local_id = (char*)node->Attribute("name");
            char *path_backlight = (char*)node->Attribute("backlightPath");
            int physic_id = 0;
            physic_id = atoi(local_id + (identifier ? strlen("local:") : strlen("port:")));
            if (access(path_backlight, F_OK) < 0) {
                continue;
            }
            ALOGI("local_id = %s, backlight_path = %s, physic_id = %d", local_id, path_backlight, physic_id);
            BacklightPath *backlightpath = new BacklightPath();
            backlightpath->physic_id = physic_id;
            strcpy(backlightpath->backlight_path, path_backlight);
            vector->push_back(backlightpath);
        }
        ALOGV("name = %s, value = %s", name, value);
    }
    bool havePrimary = false;
    for (auto i = vector->begin(); i != vector->end(); i++) {
        if ((*i)->physic_id == 0) {
            ALOGV("have primary display, don't add default backlight!");
            havePrimary = true;
        }
    }
    if (!havePrimary && access_backlight() == 0) {
        BacklightPath *backlightpath = new BacklightPath();
        backlightpath->physic_id = 0;
        strcpy(backlightpath->backlight_path, getDriverPath(LightType::BACKLIGHT));
        vector->push_back(backlightpath);
    }
    return 0;
}

static int write_int(const char* path, int value) {
    int fd;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buf[20];
        int bytes = snprintf(buf, sizeof(buf), "%d\n", value);
        ssize_t amt = write(fd, buf, (size_t)bytes);
        close(fd);
        return amt == -1? -errno : 0;
    } else {
        ALOGE("write_int() failed to open %s:%s\n", path, strerror(errno));
        return -errno;
    }
}

static int state2brightbess(const HwLightState& state) {
    int color = state.color;
    return ((77*((color>>16)&0x00ff))
            + (150*((color>>8)&0x00ff)) + (29*(color&0x00ff))) >> 8;
}

static int setLightFromType(LightType type,const char* path, const HwLightState& state) {
    int err = 0;
    switch (type) {
        case LightType::BACKLIGHT:
        case LightType::BUTTONS: {
            int brightness = state2brightbess(state);
            ALOGE("type == BACKLIGHT & BUTTONS, brightness = %d, path = %s", brightness, path);
            err = write_int(path, brightness);
            break;
        }
        case LightType::BATTERY:
        case LightType::NOTIFICATIONS:
        case LightType::ATTENTION: {
            int red, green, blue;
            int blink;
            int onMS, offMS;
            unsigned int colorRGB;
            std::string led_path(getDriverPath(type));

            switch (state.flashMode) {
                case FlashMode::TIMED: {
                    onMS = state.flashOnMs;
                    offMS = state.flashOffMs;
                    break;
                }
                case FlashMode::NONE:
                case FlashMode::HARDWARE:
                default: {
                    onMS = 0;
                    offMS = 0;
                    break;
                }
            }
            colorRGB = state.color;
            red = (colorRGB >> 16) & 0xFF;
            green = (colorRGB >> 8) & 0xFF;
            blue = colorRGB & 0xFF;
            if (onMS > 0 && offMS > 0) {
                if (onMS == offMS) blink = 2;
                else blink = 1;
            } else {
                blink = 0;
            }
            if (blink) {
                if (red) {
                    if (write_int((led_path + "/led_r/blink").c_str(), blink)) {
                        err = write_int((led_path + "/led_r/brightness").c_str(), 0);
                    }
                }
                if (green) {
                    if (write_int((led_path + "/led_g/blink").c_str(), blink)) {
                        err = write_int((led_path + "/led_g/brightness").c_str(), 0);
                    }
                }
                if (blue) {
                    if (write_int((led_path + "/led_b/blink").c_str(), blink)) {
                        err = write_int((led_path + "/led_b/brightness").c_str(), 0);
                    }
                }
            } else {
                err = write_int((led_path + "/led_r/brightness").c_str(), red);
                err = write_int((led_path + "/led_g/brightness").c_str(), green);
                err = write_int((led_path + "/led_b/brightness").c_str(), blue);
            }
            break;
        }
        default:
            break;
    }
    if (err != 0) {
        ALOGE("Failed to setLightState: %d", err);
        return err;
    }
    return 0;
}

ndk::ScopedAStatus Lights::setLightState(int id, const HwLightState& state) {
    ALOGV("Lights setting state for id=%d to color:%x", id, state.color);
    LightType type;
    int err = -1;
    char *path;
    std::vector<HwLight>::iterator it = _lights.begin();
    for (; it != _lights.end(); ++it) {
        if (it->id == id) {
            type = it->type;
            err = 0;
            break;
        }
    }
    if (err != 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    // Set lights.
    for (auto i = _backlights.begin(); i != _backlights.end(); i++) {
        if ((*i)->physic_id == id) {
            path = const_cast<char*>((*i)->backlight_path);
            break;
        }
    }
    err = setLightFromType(type, path, state);
    if (err == 0) {
        return ndk::ScopedAStatus::ok();
    }
    return ndk::ScopedAStatus::fromServiceSpecificError(err);
}

ndk::ScopedAStatus Lights::getLights(std::vector<HwLight>* lights) {
    ALOGI("Lights reporting supported lights");
    _lights.clear();
    for (auto i = _backlights.begin(); i != _backlights.end(); i++) {
        delete *i;
    }
    _backlights.clear();
    init_display_backlight_path(&_backlights);
    for (auto i = _backlights.begin(); i != _backlights.end(); i++) {
        addLight(0, LightType::BACKLIGHT, (*i)->physic_id);
    }
    if (access_rgb() == 0) {
        addLight(0, LightType::BATTERY);
    }
    if (access_rgb_blink() == 0) {
        addLight(0, LightType::NOTIFICATIONS);
    }
    for (auto i = _lights.begin(); i != _lights.end(); i++) {
        lights->push_back(*i);
    }
    return ndk::ScopedAStatus::ok();
}

void Lights::addLight(int const ordinal, LightType const type) {
    ALOGI("addLight: %s", getDriverPath(type));
    HwLight light{};
    light.id = _lights.size();
    light.ordinal = ordinal;
    light.type = type;
    _lights.emplace_back(light);
}

void Lights::addLight(int const ordinal, LightType const type, int id) {
    ALOGI("addLight: type = %hhd, ordinal = %d, id = %d", type, ordinal, id);
    HwLight light{};
    light.id = id;
    light.ordinal = ordinal;
    light.type = type;
    _lights.emplace_back(light);
}

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
