/*
 * Copyright (c) 2023, Rockchip Electronics Co. Ltd. All rights reserved.
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

/**
 * @file    profile.c
 * @brief
 * @author  RkAudio
 * @version 1.0.0
 * @date    2023-12-28
 */

#include <libxml/parser.h>
#include <libxml/tree.h>

#include "audio_hw.h"
#include "voice.h"
#include "asoundlib.h"
#include "profile.h"

#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#endif

#define LOG_NDEBUG 0

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "voice-profile"

#define VOICE_SESSION_XML_PATH "/vendor/etc/session.xml"

struct device_xml {
    xmlChar *name;
    xmlChar *snd_card;
    xmlChar *snd_pcm;
    xmlChar *type;
    xmlChar *backend;
    xmlChar *quirks;
    xmlChar *channels;
    xmlChar *rate;
    xmlChar *period_size;
    xmlChar *period_count;
    xmlChar *format;
};

struct stream_xml {
    xmlChar *sink;
    xmlChar *source;
    xmlChar *route;
    xmlChar *quirks;
};

/*
 * NOTE: This is a pcm configuration template, enable 16ms period to apply
 * rockchip audio preprocess.
 */
static struct pcm_config pcm_config_voice_call = {
    .channels = 2,
    .rate = 8000,
    .period_size = 128,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

static int voice_create_devcie(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    int num = 0;

    adev->voice.device_num = 0;
    node = nodes->xmlChildrenNode;
    while (node) {
        /* FIXME: There is always a "text" node before the real node  */
        if (xmlStrcmp(node->name, (const xmlChar *)"device")) {
            node = node->next;
            continue;
        }

        num++;
        node = node->next;
    }

    if (!num)
        return -EINVAL;

    adev->voice.devices = (struct voice_device *)malloc((sizeof(struct voice_device) * num));
    if (!adev->voice.devices)
        return -ENOMEM;

    adev->voice.device_num = num;
    memset(adev->voice.devices, 0, (sizeof(struct voice_device) * num));

    return 0;
}

static int voice_parse_device_name(struct voice_device *device, struct device_xml *attr)
{
    device->name = strdup((const char *)attr->name);
    if (!device->name)
        return -ENOMEM;

    return 0;
}

static int voice_parse_device_snd_card(struct voice_device *device, struct device_xml *attr)
{
    int tmp = -1;

    tmp = atoi((const char *)attr->snd_card);
    if (tmp == -1)
        return -EINVAL;

    device->snd_card = tmp;

    return 0;
}

static int voice_parse_device_snd_pcm(struct voice_device *device, struct device_xml *attr)
{
    int tmp = -1;

    if (attr->snd_pcm) {
        tmp = atoi((const char *)attr->snd_pcm);
        if (tmp == -1)
            return -EINVAL;
    }

    device->snd_pcm = tmp;

    return 0;
}

static int voice_parse_device_type(struct voice_device *device, struct device_xml *attr)
{
    if (!xmlStrcmp(attr->type, (const xmlChar *)"VOICE_DAILINK_FE_BE")) {
        device->type = VOICE_DAILINK_FE_BE;
    } else if (!xmlStrcmp(attr->type, (const xmlChar *)"VOICE_DAILINK_BE_BE")) {
        device->type = VOICE_DAILINK_BE_BE;
    } else if (!xmlStrcmp(attr->type, (const xmlChar *)"VOICE_DAILINK_HOSTLESS_FE")) {
        device->type = VOICE_DAILINK_HOSTLESS_FE;
    } else {
        ALOGE("invalid dailink type");
        return -EINVAL;
    }

    if ((device->type == VOICE_DAILINK_HOSTLESS_FE) && !attr->backend) {
        ALOGE("a backend is necessary to hostless FE");
        return -EINVAL;
    }

    return 0;
}

static int voice_parse_device_quirks(struct voice_device *device, struct device_xml *attr)
{
    char *token;

    if (!attr->quirks)
        return 0;

    token = strtok((char *)attr->quirks, " ");
    while (token) {
        if (!strcmp(token, "VOICE_DEVICE_ROUTE_STATIC_UPDATE"))
            device->quirks |= VOICE_DEVICE_ROUTE_STATIC_UPDATE;
        else if (!strcmp(token, "VOICE_DEVICE_CHANNEL_MONO_LEFT"))
            device->quirks |= VOICE_DEVICE_CHANNEL_MONO_LEFT;
        else if (!strcmp(token, "VOICE_DEVICE_CHANNEL_MONO_RIGHT"))
            device->quirks |= VOICE_DEVICE_CHANNEL_MONO_RIGHT;
        else if (!strcmp(token, "VOICE_DEVICE_CHANNEL_HAS_REFERENCE"))
            device->quirks |= VOICE_DEVICE_CHANNEL_HAS_REFERENCE;
        else {
            ALOGE("unknown quirks %s", token);
            return -EINVAL;
        }

        token = strtok(NULL, " ");
    }

    return 0;
}

static int voice_parse_device_config(struct voice_device *device, struct device_xml *attr)
{
    struct pcm_config *config;

    config = (struct pcm_config *)malloc(sizeof(*config));
    if (!config)
        return -ENOMEM;

    memcpy(config, &pcm_config_voice_call, sizeof(*config));

    if (attr->channels)
        config->channels = atoi((const char *)attr->channels);

    if (attr->rate)
        config->rate = atoi((const char *)attr->rate);

    if (attr->period_size)
        config->period_size = atoi((const char *)attr->period_size);

    if (attr->period_count)
        config->period_count = atoi((const char *)attr->period_count);

    if (attr->format) {
        if (!xmlStrcmp(attr->format, (const xmlChar *)"PCM_FORMAT_S16_LE"))
            config->format = PCM_FORMAT_S16_LE;
        else if (!xmlStrcmp(attr->format, (const xmlChar *)"PCM_FORMAT_S32_LE"))
            config->format = PCM_FORMAT_S32_LE;
        else if (!xmlStrcmp(attr->format, (const xmlChar *)"PCM_FORMAT_S8"))
            config->format = PCM_FORMAT_S8;
        else if (!xmlStrcmp(attr->format, (const xmlChar *)"PCM_FORMAT_S24_LE"))
            config->format = PCM_FORMAT_S24_LE;
        else if (!xmlStrcmp(attr->format, (const xmlChar *)"PCM_FORMAT_S24_3LE"))
            config->format = PCM_FORMAT_S24_3LE;
        else if (!xmlStrcmp(attr->format, (const xmlChar *)"PCM_FORMAT_IEC958_SUBFRAME_LE"))
            config->format = PCM_FORMAT_IEC958_SUBFRAME_LE;
    }

    device->config = config;

    return 0;
}

static int voice_get_device_attr(xmlNodePtr node, struct device_xml *attr)
{
    if (xmlStrcmp(node->name, (const xmlChar *)"device"))
        return -EINVAL;

    memset(attr, 0, sizeof(*attr));
    attr->name = xmlGetProp(node, BAD_CAST "name");
    attr->snd_card = xmlGetProp(node, BAD_CAST "snd_card");
    attr->snd_pcm = xmlGetProp(node, BAD_CAST "snd_pcm");
    attr->type = xmlGetProp(node, BAD_CAST "type");
    attr->backend = xmlGetProp(node, BAD_CAST "backend");
    attr->quirks = xmlGetProp(node, BAD_CAST "quirks");
    attr->channels = xmlGetProp(node, BAD_CAST "channels");
    attr->rate = xmlGetProp(node, BAD_CAST "rate");
    attr->period_size = xmlGetProp(node, BAD_CAST "period_size");
    attr->period_count = xmlGetProp(node, BAD_CAST "period_count");
    attr->format = xmlGetProp(node, BAD_CAST "format");

    return 0;
}

static void voice_put_device_attr(struct device_xml *attr)
{
    if (!attr)
        return;

    if (attr->name)
        xmlFree(attr->name);

    if (attr->snd_card)
        xmlFree(attr->snd_card);

    if (attr->snd_pcm)
        xmlFree(attr->snd_pcm);

    if (attr->type)
        xmlFree(attr->type);

    if (attr->backend)
        xmlFree(attr->backend);

    if (attr->quirks)
        xmlFree(attr->quirks);

    if (attr->channels)
        xmlFree(attr->channels);

    if (attr->rate)
        xmlFree(attr->rate);

    if (attr->period_size)
        xmlFree(attr->period_size);

    if (attr->period_count)
        xmlFree(attr->period_count);

    if (attr->format)
        xmlFree(attr->format);
}

static int voice_validate_device_attr(struct device_xml *attr)
{
    /*
     * NOTE: A backend is necessary to hostless FE, and is optional to
     * other dailink types.
     */
    if (!attr->name || !attr->snd_card || !attr->type)
        return -EINVAL;

    return 0;
}

static int voice_parse_device(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    struct device_xml attr;
    struct voice_device *device;
    int i = 0;
    int ret;

    node = nodes->xmlChildrenNode;
    while (node) {
        ret = voice_get_device_attr(node, &attr);
        if (ret) {
            node = node->next;
            continue;
        }

        ret = voice_validate_device_attr(&attr);
        if (ret) {
            ALOGE("invalid device");
            goto cleanup;
        }

        device = &adev->voice.devices[i];

        ret = voice_parse_device_name(device, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_device_snd_card(device, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_device_snd_pcm(device, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_device_type(device, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_device_quirks(device, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_device_config(device, &attr);
        if (ret)
            goto cleanup;

        voice_put_device_attr(&attr);
        node = node->next;
        i++;
    }

    return 0;

cleanup:
    voice_put_device_attr(&attr);

    return ret;
}

static int voice_parse_backend(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    xmlChar *target_name;
    xmlChar *backend_name;
    struct voice_device *device;
    struct voice_device *target_device;
    struct voice_device *backend_device;
    int i;

    node = nodes->xmlChildrenNode;
    while (node) {
        if (xmlStrcmp(node->name, (const xmlChar *)"device")) {
            node = node->next;
            continue;
        }

        backend_name = xmlGetProp(node, BAD_CAST "backend");
        if (!backend_name) {
            node = node->next;
            continue;
        }

        target_name = xmlGetProp(node, BAD_CAST "name");

        target_device = NULL;
        backend_device = NULL;
        for (i = 0; i < adev->voice.device_num; ++i) {
            device = &adev->voice.devices[i];
            if (!xmlStrcmp(target_name, (const xmlChar *)device->name))
                target_device = device;

            if (!xmlStrcmp(backend_name, (const xmlChar *)device->name))
                backend_device = device;
        }

        if (target_device && backend_device)
            target_device->backend = backend_device;

        xmlFree(target_name);
        xmlFree(backend_name);
        node = node->next;
    }

    return 0;
}

static int voice_create_stream(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    int num = 0;

    adev->voice.stream_num = 0;
    node = nodes->xmlChildrenNode;
    while (node) {
        if (xmlStrcmp(node->name, (const xmlChar *)"stream")) {
            node = node->next;
            continue;
        }

        num++;
        node = node->next;
    }

    if (!num)
        return -EINVAL;

    adev->voice.streams = (struct voice_stream *)malloc((sizeof(struct voice_stream) * num));
    if (!adev->voice.streams)
        return -ENOMEM;

    adev->voice.stream_num = num;
    memset(adev->voice.streams, 0, (sizeof(struct voice_stream) * num));

    return 0;
}

static int voice_find_device(struct audio_device *adev, xmlChar *name,
                             struct voice_device **device)
{
    struct voice_device *tmp;
    bool found = false;
    int i;

    for (i = 0; i < adev->voice.device_num; ++i) {
        tmp = &adev->voice.devices[i];

        if (!xmlStrcmp(name, (const xmlChar *)tmp->name)) {
            found = true;
            *device = tmp;
            break;
        }
    }

    return found ? 0 : -ENODEV;
}

static int voice_parse_stream_sink(struct audio_device *adev, struct voice_stream *stream,
                                   struct stream_xml *attr)
{
    struct voice_device *device;
    int ret;

    if (xmlStrcmp(attr->sink, (const xmlChar *)"none")) {
        ret = voice_find_device(adev, attr->sink, &device);
        if (ret)
            return ret;

        stream->sink = device;
    }

    return 0;
}

static int voice_parse_stream_source(struct audio_device *adev, struct voice_stream *stream,
                                     struct stream_xml *attr)
{
    struct voice_device *device;
    int ret;

    if (xmlStrcmp(attr->source, (const xmlChar *)"none")) {
        ret = voice_find_device(adev, attr->source, &device);
        if (ret)
            return ret;

        stream->source = device;
    }

    return 0;
}

static int voice_parse_stream_route(struct voice_stream *stream, struct stream_xml *attr)
{
    if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_SPEAKER")) {
        stream->route = AUDIO_DEVICE_OUT_SPEAKER;
    } else if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_WIRED_HEADSET")) {
        stream->route = AUDIO_DEVICE_OUT_WIRED_HEADSET;
    } else if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_WIRED_HEADPHONE")) {
        stream->route = AUDIO_DEVICE_OUT_WIRED_HEADPHONE;
    } else if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_ALL_SCO")) {
        stream->route = AUDIO_DEVICE_OUT_ALL_SCO;
    } else if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET")) {
        stream->route = AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET;
    } else if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_BLUETOOTH_SCO")) {
        stream->route = AUDIO_DEVICE_OUT_BLUETOOTH_SCO;
    } else if (!xmlStrcmp(attr->route, (const xmlChar *)"AUDIO_DEVICE_OUT_EARPIECE")) {
        stream->route = AUDIO_DEVICE_OUT_EARPIECE;
    } else {
        ALOGE("unsupported route %s", attr->route);
        return -EINVAL;
    }

    return 0;
}

static int voice_parse_stream_quirks(struct voice_stream *stream, struct stream_xml *attr)
{
    char *token;

    if (!attr->quirks)
        return 0;

    token = strtok((char *)attr->quirks, " ");
    while (token) {
        if (!strcmp(token, "VOICE_STREAM_CHANNEL_MONO_LEFT"))
            stream->quirks |= VOICE_STREAM_CHANNEL_MONO_LEFT;
        else if (!strcmp(token, "VOICE_STREAM_CHANNEL_MONO_RIGHT"))
            stream->quirks |= VOICE_STREAM_CHANNEL_MONO_RIGHT;
        else if (!strcmp(token, "VOICE_STREAM_OUTGOING"))
            stream->quirks |= VOICE_STREAM_OUTGOING;
        else {
            ALOGE("unknown quirks %s", token);
            return -EINVAL;
        }

        token = strtok(NULL, " ");
    }

    return 0;
}

static int voice_get_stream_attr(xmlNodePtr node, struct stream_xml *attr)
{
    if (xmlStrcmp(node->name, (const xmlChar *)"stream"))
        return -EINVAL;

    memset(attr, 0, sizeof(*attr));
    attr->sink = xmlGetProp(node, BAD_CAST "sink");
    attr->source = xmlGetProp(node, BAD_CAST "source");
    attr->route = xmlGetProp(node, BAD_CAST "route");
    attr->quirks = xmlGetProp(node, BAD_CAST "quirks");

    return 0;
}

static void voice_put_stream_attr(struct stream_xml *attr)
{
    if (!attr)
        return;

    if (attr->sink)
        xmlFree(attr->sink);

    if (attr->source)
        xmlFree(attr->source);

    if (attr->route)
        xmlFree(attr->route);

    if (attr->quirks)
        xmlFree(attr->quirks);
}

static int voice_validate_stream_attr(struct stream_xml *attr)
{
    if (!attr->sink || !attr->source || !attr->route)
        return -EINVAL;

    return 0;
}

static int voice_parse_stream(struct audio_device *adev, xmlNodePtr nodes)
{
    xmlNodePtr node;
    struct stream_xml attr;
    struct voice_device *device;
    struct voice_stream *stream;
    int i = 0;
    int ret;

    node = nodes->xmlChildrenNode;
    while (node) {
        ret = voice_get_stream_attr(node, &attr);
        if (ret) {
            node = node->next;
            continue;
        }

        ret = voice_validate_stream_attr(&attr);
        if (ret) {
            ALOGE("invalid stream");
            goto cleanup;
        }

        stream = &adev->voice.streams[i];

        ret = voice_parse_stream_sink(adev, stream, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_stream_source(adev, stream, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_stream_route(stream, &attr);
        if (ret)
            goto cleanup;

        ret = voice_parse_stream_quirks(stream, &attr);
        if (ret)
            goto cleanup;

        voice_put_stream_attr(&attr);
        node = node->next;
        i++;
    }

    return 0;

cleanup:
    voice_put_stream_attr(&attr);

    return ret;
}

static int voice_load_device(struct audio_device *adev, xmlNodePtr nodes)
{
    int ret;

    ret = voice_create_devcie(adev, nodes);
    if (ret)
        return ret;

    ret = voice_parse_device(adev, nodes);
    if (ret)
        return ret;

    ret = voice_parse_backend(adev, nodes);
    if (ret)
        return ret;

    return 0;
}

static int voice_load_stream(struct audio_device *adev, xmlNodePtr nodes)
{
    int ret;

    ret = voice_create_stream(adev, nodes);
    if (ret)
        return ret;

    ret = voice_parse_stream(adev, nodes);
    if (ret)
        return ret;

    return 0;
}

int voice_load_profile(struct audio_device *adev)
{
    xmlDocPtr config;
    xmlNodePtr session;
    xmlNodePtr child;
    xmlNodePtr devices;
    xmlNodePtr streams;
    struct voice_device *device;
    struct voice_stream *stream;
    int i;
    int ret;

    config = xmlParseFile(VOICE_SESSION_XML_PATH);
    if (!config) {
        ALOGE("No profile");
        ret = -EINVAL;
        goto err;
    }

    session = xmlDocGetRootElement(config);
    if (!session) {
        ALOGE("No session configuration");
        ret = -EINVAL;
        goto cleanup_config;
    }

    child = session->xmlChildrenNode;
    while (child) {
        if (!xmlStrcmp(child->name, (const xmlChar *)"devices"))
            devices = child;
        else if (!xmlStrcmp(child->name, (const xmlChar *)"streams"))
            streams = child;

        child = child->next;
    }

    if (!devices || !streams) {
        ALOGE("No devices or streams configuration");
        ret = -EINVAL;
        goto cleanup_config;
    }

    ret = voice_load_device(adev, devices);
    if (ret) {
        ALOGE("failed to load device");
        goto cleanup_device;
    }

    ret = voice_load_stream(adev, streams);
    if (ret) {
        ALOGE("failed to load device");
        goto cleanup_stream;
    }

    voice_dump_device(adev);
    voice_dump_stream(adev);
    xmlFreeDoc(config);

    return 0;

cleanup_stream:
    if (adev->voice.stream_num) {
        free(adev->voice.streams);
        adev->voice.streams = NULL;
        adev->voice.stream_num = 0;
    }

cleanup_device:
    if (adev->voice.device_num) {
        for (i = 0; i < adev->voice.device_num; ++i) {
            device = &adev->voice.devices[i];

            if (device->name) {
                free(device->name);
                device->name = NULL;
            }

            if (device->config) {
                free(device->config);
                device->config = NULL;
            }
        }

        free(adev->voice.devices);
        adev->voice.devices = NULL;
        adev->voice.device_num = 0;
    }

cleanup_config:
    xmlFreeDoc(config);

err:
    return ret;
}
