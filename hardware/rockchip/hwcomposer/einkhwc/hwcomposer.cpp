/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwcomposer-drm"

#include <stdlib.h>
#include <cinttypes>
#include <map>
#include <vector>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <libsync/sw_sync.h>
#include <sync/sync.h>
#include <utils/Trace.h>
#include <drm_fourcc.h>
#if RK_DRM_GRALLOC
#include "gralloc_drm_handle.h"
#endif
#include <linux/fb.h>
//open header
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//map header
#include <map>
#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/epoll.h>
//gui
#include <ui/Rect.h>
#include <ui/Region.h>
#include <ui/GraphicBufferMapper.h>

#include "hwc_util.h"
#include "hwc_rockchip.h"
#include "vsyncworker.h"
#include "android/configuration.h"
#include "drmhwcomposer.h"
#include "einkcompositorworker.h"
#include "libcfa/libcfa.h"

#define UM_PER_INCH 25400

namespace android {
static int ebc_fd = -1;
static struct ebc_buf_info_t ebc_buf_info;
static int gCurrentEpdMode = EPD_PART_GC16;
static int gOneFullModeTime = 0;
static Mutex mEinkModeLock;
static hwc_context_t* g_ctx = NULL;
static hwc_drm_display_t hwc_info;

struct hwc_context_t {
	// map of display:hwc_drm_display_t
	typedef std::map<int, hwc_drm_display_t> DisplayMap;

	~hwc_context_t() {
	}

	hwc_composer_device_1_t device;
	hwc_procs_t const *procs = NULL;

	DisplayMap displays;
	const gralloc_module_t *gralloc;
	EinkCompositorWorker eink_compositor_worker;
	VSyncWorker primary_vsync_worker;
	VSyncWorker extend_vsync_worker;
};

static void hwc_dump(struct hwc_composer_device_1 *dev, char *buff, int buff_len)
{
	return;
}

static int hwc_prepare(hwc_composer_device_1_t *dev, size_t num_displays, hwc_display_contents_1_t **display_contents)
{
	UN_USED(dev);
	init_log_level();
	for (int i = 0; i < (int)num_displays; ++i) {
		if (!display_contents[i])
			continue;
		int num_layers = display_contents[i]->numHwLayers;
		for (int j = 0; j < num_layers - 1; ++j) {
			hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];
			layer->compositionType = HWC_FRAMEBUFFER;
		}
	}
	return 0;
}

#define CLIP(x) (((x) > 255) ? 255 : (x))
void Luma8bit_to_4bit_row_16(int *src, int *dst, short int *res0, short int*res1, int w)
{
	int i;
	int g0, g1, g2, g3, g4, g5, g6, g7, g_temp;
	int e;
	int v0, v1, v2, v3;
	int src_data;
	int src_temp_data;

	v0 = 0;
	for (i = 0; i < w; i += 8) {
		src_data = *src++;
		src_temp_data = src_data & 0xff;
		g_temp = src_temp_data + res0[i] + v0;
		res0[i] = 0;
		g_temp = CLIP(g_temp);
		g0 = g_temp & 0xf0;
		e = g_temp - g0;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;

		if (i==0) {
			res1[i] += v2;
			res1[i + 1] += v3;
		} else {
			res1[i - 1] += v1;
			res1[i] += v2;
			res1[i + 1] += v3;
		}

		src_temp_data = ((src_data & 0x0000ff00) >> 8);
		g_temp = src_temp_data + res0[i + 1] + v0;
		res0[i + 1] = 0;
		g_temp = CLIP(g_temp);
		g1 = g_temp & 0xf0;
		e = g_temp - g1;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i] += v1;
		res1[i + 1] += v2;
		res1[i + 2] += v3;

		src_temp_data = ((src_data & 0x00ff0000) >> 16);
		g_temp = src_temp_data + res0[i + 2] + v0;
		res0[i + 2] = 0;
		g_temp = CLIP(g_temp);
		g2 = g_temp & 0xf0;
		e = g_temp - g2;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 1] += v1;
		res1[i + 2] += v2;
		res1[i + 3] += v3;

		src_temp_data = ((src_data & 0xff000000) >> 24);
		g_temp = src_temp_data + res0[i + 3] + v0;
		res0[i + 3] = 0;
		g_temp = CLIP(g_temp);
		g3 = g_temp & 0xf0;
		e = g_temp - g3;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 2] += v1;
		res1[i + 3] += v2;
		res1[i + 4] += v3;

		src_data = *src++;
		src_temp_data = src_data & 0xff;
		g_temp = src_temp_data + res0[i+4] + v0;
		res0[i + 4] = 0;
		g_temp = CLIP(g_temp);
		g4 = g_temp & 0xf0;
		e = g_temp - g4;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 3] += v1;
		res1[i + 4] += v2;
		res1[i + 5] += v3;

		src_temp_data = ((src_data & 0x0000ff00) >> 8);
		g_temp = src_temp_data + res0[i + 5] + v0;
		res0[i + 5] = 0;
		g_temp = CLIP(g_temp);
		g5 = g_temp & 0xf0;
		e = g_temp - g5;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 4] += v1;
		res1[i + 5] += v2;
		res1[i + 6] += v3;

		src_temp_data = ((src_data & 0x00ff0000) >> 16);
		g_temp = src_temp_data + res0[i + 6] + v0;
		res0[i + 6] = 0;
		g_temp = CLIP(g_temp);
		g6 = g_temp & 0xf0;
		e = g_temp - g6;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 5] += v1;
		res1[i + 6] += v2;
		res1[i + 7] += v3;

		src_temp_data = ((src_data & 0xff000000) >> 24);
		g_temp = src_temp_data + res0[i + 7] + v0;
		res0[i + 7] = 0;
		g_temp = CLIP(g_temp);
		g7 = g_temp & 0xf0;
		e = g_temp - g7;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		if (i == w-8) {
			res1[i + 6] += v1;
			res1[i + 7] += v2;
		} else {
			res1[i + 6] += v1;
			res1[i + 7] += v2;
			res1[i + 8] += v3;
		}

		*dst++ =(g7 << 24) | (g6 << 20) | (g5 << 16) | (g4 << 12)  | (g3 << 8) | (g2 << 4) | g1 | (g0 >> 4);
	}
}

void Luma8bit_to_8bit_row_16(int *src,  int *dst, short int *res0,  short int *res1, int w)
{
	int i;
	int g0, g1, g2, g3, g4, g5, g6, g7, g_temp;
	int e;
	int v0, v1, v2, v3;
	int src_data;
	int src_temp_data;

	v0 = 0;
	for (i = 0; i < w; i += 8) {
		src_data = *src++;
		src_temp_data = src_data & 0xff;
		g_temp = src_temp_data + res0[i] + v0;
		res0[i] = 0;
		g_temp = CLIP(g_temp);
		g0 = g_temp & 0xf0;
		e = g_temp - g0;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		if (i == 0) {
			res1[i] += v2;
			res1[i + 1] += v3;
		} else {
			res1[i - 1] += v1;
			res1[i] += v2;
			res1[i + 1] += v3;
		}

		src_temp_data = ((src_data & 0x0000ff00) >> 8);
		g_temp = src_temp_data + res0[i + 1] + v0;
		res0[i + 1] = 0;
		g_temp = CLIP(g_temp);
		g1 = g_temp & 0xf0;
		e = g_temp - g1;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i] += v1;
		res1[i + 1] += v2;
		res1[i + 2] += v3;

		src_temp_data = ((src_data & 0x00ff0000) >> 16);
		g_temp = src_temp_data + res0[i + 2] + v0;
		res0[i + 2] = 0;
		g_temp = CLIP(g_temp);
		g2 = g_temp & 0xf0;
		e = g_temp - g2;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i+1] += v1;
		res1[i+2] += v2;
		res1[i+3] += v3;

		src_temp_data = ((src_data & 0xff000000) >> 24);
		g_temp = src_temp_data + res0[i + 3] + v0;
		res0[i + 3] = 0;
		g_temp = CLIP(g_temp);
		g3 = g_temp & 0xf0;
		e = g_temp - g3;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 2] += v1;
		res1[i + 3] += v2;
		res1[i + 4] += v3;

		*dst++ = (g3 << 24) | (g2 << 16) | (g1 << 8) | g0;

		src_data = *src++;
		src_temp_data = src_data & 0xff;
		g_temp = src_temp_data + res0[i + 4] + v0;
		res0[i + 4] = 0;
		g_temp = CLIP(g_temp);
		g4 = g_temp & 0xf0;
		e = g_temp - g4;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i+3] += v1;
		res1[i+4] += v2;
		res1[i+5] += v3;

		src_temp_data = ((src_data & 0x0000ff00) >> 8);
		g_temp = src_temp_data + res0[i + 5] + v0;
		res0[i + 5] = 0;
		g_temp = CLIP(g_temp);
		g5 = g_temp & 0xf0;
		e = g_temp - g5;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 4] += v1;
		res1[i + 5] += v2;
		res1[i + 6] += v3;

		src_temp_data = ((src_data & 0x00ff0000) >> 16);
		g_temp = src_temp_data + res0[i + 6] + v0;
		res0[i + 6] = 0;
		g_temp = CLIP(g_temp);
		g6 = g_temp & 0xf0;
		e = g_temp - g6;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		res1[i + 5] += v1;
		res1[i + 6] += v2;
		res1[i + 7] += v3;

		src_temp_data = ((src_data & 0xff000000) >> 24);
		g_temp = src_temp_data + res0[i + 7] + v0;
		res0[i + 7] = 0;
		g_temp = CLIP(g_temp);
		g7 = g_temp & 0xf0;
		e = g_temp - g7;
		v0 = (e * 7) >> 4;
		v1 = (e * 3) >> 4;
		v2 = (e * 5) >> 4;
		v3 = (e * 1) >> 4;
		if (i == w-8) {
			res1[i + 6] += v1;
			res1[i + 7] += v2;
		} else {
			res1[i + 6] += v1;
			res1[i + 7] += v2;
			res1[i + 8] += v3;
		}

		*dst++ = (g7 << 24) | (g6 << 16) | (g5 << 8) | g4;
	}
}

int gray256_to_gray16_dither_y8(char *gray256_addr, char *gray16_buffer, int  panel_h, int panel_w, int vir_width)
{
	ATRACE_CALL();
	UN_USED(vir_width);
	int h;
	int w;
	short int *line_buffer[2];
	char *src_buffer;

	line_buffer[0] =(short int *) malloc(panel_w * 2);
	line_buffer[1] =(short int *) malloc(panel_w * 2);
	memset(line_buffer[0], 0, panel_w * 2);
	memset(line_buffer[1], 0, panel_w * 2);

	for (h = 0; h < panel_h; h++) {
		Luma8bit_to_8bit_row_16((int*)gray256_addr, (int *)gray16_buffer, line_buffer[h & 1], line_buffer[!(h & 1)], panel_w);
		gray16_buffer = (char *)(gray16_buffer + panel_w);
		gray256_addr = (char *)(gray256_addr + panel_w);
	}
	free(line_buffer[0]);
	free(line_buffer[1]);

	return 0;
}

int gray256_to_gray16_dither(char *gray256_addr, int *gray16_buffer, int  panel_h, int panel_w, int vir_width)
{
	ATRACE_CALL();
	UN_USED(vir_width);
	int h;
	int w;
	short int *line_buffer[2];
	char *src_buffer;

	line_buffer[0] =(short int *) malloc(panel_w * 2);
	line_buffer[1] =(short int *) malloc(panel_w * 2);
	memset(line_buffer[0], 0, panel_w * 2);
	memset(line_buffer[1], 0, panel_w * 2);

	for(h = 0; h<panel_h; h++) {
		Luma8bit_to_4bit_row_16((int*)gray256_addr, gray16_buffer, line_buffer[h & 1], line_buffer[!(h & 1)], panel_w);
		gray16_buffer = gray16_buffer + panel_w / 8;
		gray256_addr = (char*)(gray256_addr + panel_w);
	}
	free(line_buffer[0]);
	free(line_buffer[1]);

	return 0;
}

#define CORRECTION_VALUE 16
void Luma8bit_to_4bit_row_2(short int *src, char *dst, int8_t *correct, short int *res0, short int *res1, int w, int threshold)
{
	int i;
	int g0, g1, g2, g3, g4, g5, g6, g7, g_temp;
	int e;
	int v0, v1, v2, v3;
	int src_data;
	int src_temp_data;

	v0 = 0;
	for (i = 0; i < w; i += 2) {
		src_data =  *src++;
		src_temp_data = src_data & 0xff;
		g_temp = src_temp_data + res0[i] + v0 + *correct;
		res0[i] = 0;
		g_temp = CLIP(g_temp);
		if (g_temp >= threshold) {
			g0 = 0x0f;
			e = g_temp - 0xf0;
			*correct = CORRECTION_VALUE;
		} else {
			g0 = 0x00;
			e = g_temp;
			*correct = -CORRECTION_VALUE;
		}
		++correct;
		// 6/2/4/1
		v0 = (e * 6) >> 4;
		v1 = e >> 3;
		v2 = e >> 2;
		v3 = e >> 4;
		if (i == 0) {
			res1[i] += v2;
			res1[i + 1] += v3;
		} else {
			res1[i - 1] += v1;
			res1[i]   += v2;
			res1[i + 1] += v3;
		}

		src_temp_data = ((src_data & 0x0000ff00) >> 8);
		g_temp = src_temp_data + res0[i + 1] + v0 + *correct;
		res0[i + 1] = 0;
		g_temp = CLIP(g_temp);
		if (g_temp >= threshold) {
			g1 = 0x0f;
			e = g_temp - 0xf0;
			*correct = CORRECTION_VALUE;
		} else {
			g1 = 0x00;
			e = g_temp;
			*correct = -CORRECTION_VALUE;
		}
		++correct;
		// 6/2/4/1
		v0 = (e * 6) >> 4;
		v1 = e >> 3;
		v2 = e >> 2;
		v3 = e >> 4;
		res1[i] += v1;
		res1[i + 1] += v2;
		res1[i + 2] += v3;

		*dst++ = (g1 << 4) | (g0);
	}
}

void Luma8bit_to_8bit_row_2(short int *src, short int *dst, int8_t *correct, short int *res0, short int *res1, int w, int threshold)
{
	int i;
	int g0, g1, g2, g3, g4, g5, g6, g7, g_temp;
	int e;
	int v0, v1, v2, v3;
	int src_data;
	int src_temp_data;

	v0 = 0;
	for (i = 0; i < w; i += 2) {
		src_data = *src++;
		src_temp_data = src_data & 0xff;
		g_temp = src_temp_data + res0[i] + v0 + *correct;
		res0[i] = 0;
		g_temp = CLIP(g_temp);
		if (g_temp >= threshold) {
			g0 = 0xf0;
			e = g_temp - 0xf0;
			*correct = CORRECTION_VALUE;
		} else {
			g0 = 0x00;
			e = g_temp;
			*correct = -CORRECTION_VALUE;
		}
		++correct;
		// 6/2/4/1
		v0 = (e * 6) >> 4;
		v1 = e >> 3;
		v2 = e >> 2;
		v3 = e >> 4;
		if (i == 0) {
			res1[i] += v2;
			res1[i + 1] += v3;
		} else {
			res1[i - 1] += v1;
			res1[i] += v2;
			res1[i + 1] += v3;
		}

		src_temp_data = ((src_data & 0x0000ff00) >> 8);
		g_temp = src_temp_data + res0[i + 1] + v0 + *correct;
		res0[i + 1] = 0;
		g_temp = CLIP(g_temp);
		if (g_temp >= threshold) {
			g1 = 0xf000;
			e = g_temp - 0xf0;
			*correct = CORRECTION_VALUE;
		} else {
			g1 = 0x00;
			e = g_temp;
			*correct = -CORRECTION_VALUE;
		}
		++correct;
		// 6/2/4/1
		v0 = (e * 6) >> 4;
		v1 = e >> 3;
		v2 = e >> 2;
		v3 = e >> 4;
		res1[i] += v1;
		res1[i + 1] += v2;
		res1[i + 2] += v3;

		*dst++ = g1 | g0;
	}
}

static int8_t *g_correct = NULL;
int gray256_to_gray2_dither(char *gray256_addr, char *gray2_buffer, int panel_h, int panel_w, int vir_width, Region region)
{
	ATRACE_CALL();

	if (g_correct == NULL) {
		g_correct = (int8_t*)malloc(panel_w * panel_h);
		memset(g_correct, 0, panel_w * panel_h);
	}

	//do dither
	short int *line_buffer[2];
	line_buffer[0] =(short int *)malloc(panel_w << 1);
	line_buffer[1] =(short int *)malloc(panel_w << 1);

	size_t count = 0;
	const Rect* rects = region.getArray(&count);
	for (size_t i = 0;i < (int)count;i++) {
		memset(line_buffer[0], 0, panel_w << 1);
		memset(line_buffer[1], 0, panel_w << 1);

		int w = rects[i].right - rects[i].left;
		int offset = rects[i].top * panel_w + rects[i].left;
		int offset_dst = rects[i].top * vir_width + rects[i].left;
		if (offset_dst % 2) {
			offset_dst += (2 - offset_dst % 2);
		}
		if (offset % 2) {
			offset += (2 - offset % 2);
		}
		if ((offset_dst + w) % 2) {
			w -= (offset_dst + w) % 2;
		}
		for (int h = rects[i].top; h <= rects[i].bottom && h < panel_h; h++) {
			//ALOGD("DEBUG_lb Luma8bit_to_4bit_row_2, w:%d, offset:%d, offset_dst:%d", w, offset, offset_dst);
			Luma8bit_to_4bit_row_2((short int*)(gray256_addr + offset), (char *)(gray2_buffer + (offset_dst >> 1)), g_correct + offset, line_buffer[h & 1], line_buffer[!(h & 1)], w, 0x80);
			offset += panel_w;
			offset_dst += vir_width;
		}
	}
	free(line_buffer[0]);
	free(line_buffer[1]);

	return 0;
}

int gray256_to_gray2_dither_y8(char *gray256_addr, char *gray2_buffer, int  panel_h, int panel_w, int vir_width, Region region)
{
	ATRACE_CALL();

	if (g_correct == NULL) {
		g_correct = (int8_t*)malloc(panel_w * panel_h);
		memset(g_correct, 0, panel_w * panel_h);
	}

	//do dither
	short int *line_buffer[2];
	line_buffer[0] =(short int *)malloc(panel_w << 1);
	line_buffer[1] =(short int *)malloc(panel_w << 1);

	size_t count = 0;
	const Rect* rects = region.getArray(&count);
	for (size_t i = 0;i < (int)count;i++) {
		memset(line_buffer[0], 0, panel_w << 1);
		memset(line_buffer[1], 0, panel_w << 1);

		int w = rects[i].right - rects[i].left;
		int offset = rects[i].top * panel_w + rects[i].left;
		int offset_dst = rects[i].top * vir_width + rects[i].left;
		if (offset_dst % 2) {
			offset_dst += (2 - offset_dst % 2);
		}
		if (offset % 2) {
			offset += (2 - offset % 2);
		}
		if ((offset_dst + w) % 2) {
			w -= (offset_dst + w) % 2;
		}
		for (int h = rects[i].top; h <= rects[i].bottom && h < panel_h; h++) {
			//ALOGD("DEBUG_lb Luma8bit_to_4bit_row_2, w:%d, offset:%d, offset_dst:%d", w, offset, offset_dst);
			Luma8bit_to_8bit_row_2((short int*)(gray256_addr + offset), (short int*)(gray2_buffer + offset_dst), g_correct + offset, line_buffer[h & 1], line_buffer[!(h & 1)], w, 0x80);
			offset += panel_w;
			offset_dst += vir_width;
		}
	}
	free(line_buffer[0]);
	free(line_buffer[1]);

	return 0;
}

void do_gray256_buffer(uint32_t *buffer_in, uint32_t *buffer_out, int width, int height)
{
	uint32_t src_data;
	uint32_t *src = buffer_in;
	uint32_t *dst = buffer_out;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j< width / 4; j++) {
			src_data = *src++;
			src_data &= 0xf0f0f0f0;
			*dst++ = src_data;
		}
	}
}

void  change_4bit_to_8bit(unsigned char *in_buffer, unsigned char *out_buffer, int size)
{
	int i;
	unsigned char buffer_in;

	for (i = 0; i < size; i++) {
		buffer_in = *in_buffer++;
		*out_buffer++ = (buffer_in & 0x0f) << 4;
		*out_buffer++ = (buffer_in & 0xf0);
	}
}

static int hwc_handle_eink_mode(void)
{
	char value[PROPERTY_VALUE_MAX];

	property_get("sys.eink.one_full_mode_timeline", value, "0");
	int one_full_mode_timeline = atoi(value);

	if (gOneFullModeTime != one_full_mode_timeline) {
		gOneFullModeTime = one_full_mode_timeline;
		gCurrentEpdMode = EPD_FORCE_FULL;
	}

	return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays, hwc_display_contents_1_t **sf_display_contents)
{
	ATRACE_CALL();
	Mutex::Autolock lock(mEinkModeLock);
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	int ret = 0;
	static int isPoweroff = 0;

	inc_frame();
	char value[PROPERTY_VALUE_MAX];
	property_get("sys.eink.mode", value, "9");
	gCurrentEpdMode = atoi(value);

	//Handle eink mode.
	ret = hwc_handle_eink_mode();

	if (!isPoweroff) {
		for (size_t i = 0; i < num_displays; ++i) {
			hwc_display_contents_1_t *dc = sf_display_contents[i];

			if (!sf_display_contents[i])
				continue;

			size_t num_dc_layers = dc->numHwLayers;
			for (size_t j = 0; j < num_dc_layers; ++j) {
				hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
				if (sf_layer != NULL && sf_layer->handle != NULL) {
					char layername[100];
					hwc_get_handle_layername(ctx->gralloc, sf_layer->handle, layername, 100);
					if (strstr(layername, "EBOOK_FULL")) {
						ALOGD("EBOOK FULL\n");
						gCurrentEpdMode = EPD_FORCE_FULL;
					} else if (strstr(layername, "EBOOK_POWEROFF")) {
						ALOGD("EBOOK POWEROFF\n");
						gCurrentEpdMode = EPD_FORCE_FULL;
						isPoweroff = 1;
					}
					if (sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET)
						ctx->eink_compositor_worker.QueueComposite(dc, gCurrentEpdMode);
				}
			}
		}
	} else {
		ALOGD("system poweroff, skip this frame\n");
		for (size_t i = 0; i < num_displays; ++i) {
			hwc_display_contents_1_t *dc = sf_display_contents[i];

			if (!sf_display_contents[i])
				continue;

			size_t num_dc_layers = dc->numHwLayers;
			for (size_t j = 0; j < num_dc_layers; ++j) {
				hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
				dump_layer(ctx->gralloc, false, sf_layer, j);
				if (sf_layer != NULL && sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
					if (sf_layer->acquireFenceFd > 0) {
						sync_wait(sf_layer->acquireFenceFd, -1);
						close(sf_layer->acquireFenceFd);
						sf_layer->acquireFenceFd = -1;
					}
				}
			}
		}
	}

	return 0;
}

static int hwc_event_control(struct hwc_composer_device_1 *dev, int display, int event, int enabled)
{
	if (event != HWC_EVENT_VSYNC || (enabled != 0 && enabled != 1))
		return -EINVAL;

	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	if (display == HWC_DISPLAY_PRIMARY)
		return ctx->primary_vsync_worker.VSyncControl(enabled);
	else if (display == HWC_DISPLAY_EXTERNAL)
		return ctx->extend_vsync_worker.VSyncControl(enabled);

	ALOGE("Can't support vsync control for display %d\n", display);

	return -EINVAL;
}

static int hwc_set_power_mode(struct hwc_composer_device_1 *dev, int display, int mode)
{
	Mutex::Autolock lock(mEinkModeLock);
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d , display = %d ,mode = %d",__FUNCTION__,__LINE__,display,mode);

	switch (mode) {
		case HWC_POWER_MODE_OFF:
			ioctl(ebc_fd, EBC_SET_FB_BLANK, NULL);
			break;
		case HWC_POWER_MODE_DOZE_SUSPEND:
		case HWC_POWER_MODE_NORMAL:
			ioctl(ebc_fd, EBC_SET_FB_UNBLANK, NULL);
			break;
	}
	return 0;
}

static int hwc_query(struct hwc_composer_device_1 *, int what, int *value)
{
	switch (what) {
		case HWC_BACKGROUND_LAYER_SUPPORTED:
			*value = 0; /* TODO: We should do this */
			break;
		case HWC_VSYNC_PERIOD:
			ALOGW("Query for deprecated vsync value, returning 20Hz");
			*value = 1000 * 1000 * 1000 / 20;
			break;
		case HWC_DISPLAY_TYPES_SUPPORTED:
			*value = HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_EXTERNAL_BIT | HWC_DISPLAY_VIRTUAL_BIT;
			break;
	}
	return 0;
}

static void hwc_register_procs(struct hwc_composer_device_1 *dev, hwc_procs_t const *procs)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	UN_USED(dev);

	ctx->procs = procs;

	ctx->primary_vsync_worker.SetProcs(procs);
	ctx->extend_vsync_worker.SetProcs(procs);
}

static int hwc_get_display_configs(struct hwc_composer_device_1 *dev, int display, uint32_t *configs, size_t *num_configs)
{
	UN_USED(dev);
	UN_USED(display);

	if (!num_configs)
		return 0;

	uint32_t width = 0, height = 0 , vrefresh = 0 ;
	width = ebc_buf_info.width - (ebc_buf_info.width % 8);
	height = ebc_buf_info.height - (ebc_buf_info.height % 2);
	hwc_info.framebuffer_width = width;
	hwc_info.framebuffer_height = height;
	hwc_info.vrefresh = vrefresh ? vrefresh : 20;
	*num_configs = 1;
	for (int i = 0; i < static_cast<int>(*num_configs); i++)
		configs[i] = i;

	return 0;
}

static float getDefaultDensity(uint32_t width, uint32_t height)
{
	uint32_t h = width < height ? width : height;

	if (h >= 1080)
		return ACONFIGURATION_DENSITY_XHIGH;
	else
		return ACONFIGURATION_DENSITY_TV;
}

static int hwc_get_display_attributes(struct hwc_composer_device_1 *dev, int display, uint32_t config, const uint32_t *attributes, int32_t *values)
{
	UN_USED(config);
	UN_USED(display);
	UN_USED(dev);

	uint32_t mm_width = ebc_buf_info.width_mm;
	uint32_t mm_height = ebc_buf_info.height_mm;
	int w = hwc_info.framebuffer_width;
	int h = hwc_info.framebuffer_height;
	int vrefresh = hwc_info.vrefresh;

	for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; ++i) {
		switch (attributes[i]) {
			case HWC_DISPLAY_VSYNC_PERIOD:
			values[i] = 1000 * 1000 * 1000 / vrefresh;
			break;
			case HWC_DISPLAY_WIDTH:
			values[i] = w;
			break;
			case HWC_DISPLAY_HEIGHT:
			values[i] = h;
			break;
			case HWC_DISPLAY_DPI_X:
			/* Dots per 1000 inches */
			values[i] = mm_width ? (w * UM_PER_INCH) / mm_width : getDefaultDensity(w, h) * 1000;
			break;
			case HWC_DISPLAY_DPI_Y:
			/* Dots per 1000 inches */
			values[i] = mm_height ? (h * UM_PER_INCH) / mm_height : getDefaultDensity(w, h) * 1000;
			break;
		}
	}
	return 0;
}

static int hwc_get_active_config(struct hwc_composer_device_1 *dev, int display)
{
	UN_USED(dev);
	UN_USED(display);
	ALOGD_IF(log_level(DBG_DEBUG), "DEBUG_lb getActiveConfig mode = %d", 0);
	return 0;
}

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display, int index)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
	UN_USED(display);
	ALOGD_IF(log_level(DBG_DEBUG), "%s,line = %d mode = %d", __FUNCTION__, __LINE__, index);
	return 0;
}

static int hwc_device_close(struct hw_device_t *dev)
{
	struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
	delete ctx;
	return 0;
}

static int hwc_initialize_display(struct hwc_context_t *ctx, int display)
{
	hwc_drm_display_t *hd = &ctx->displays[display];
	hd->ctx = ctx;
	hd->gralloc = ctx->gralloc;
	hd->framebuffer_width = 0;
	hd->framebuffer_height = 0;

#if RK_RGA_PREPARE_ASYNC
	hd->rgaBuffer_index = 0;
	hd->mUseRga = false;
#endif
	return 0;
}

static int hwc_enumerate_displays(struct hwc_context_t *ctx)
{
	int ret, num_connectors = 0;
	ret = ctx->eink_compositor_worker.Init(ctx);
	if (ret) {
		ALOGE("Failed to initialize virtual compositor worker");
		return ret;
	}
	ret = hwc_initialize_display(ctx, 0);
	if (ret) {
		ALOGE("Failed to initialize display %d", 0);
		return ret;
	}

	ret = ctx->primary_vsync_worker.Init(HWC_DISPLAY_PRIMARY);
	if (ret) {
		ALOGE("Failed to create event worker for primary display %d\n", ret);
		return ret;
	}

	return 0;
}

static int hwc_device_open(const struct hw_module_t *module, const char *name, struct hw_device_t **dev)
{
	if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
		ALOGE("Invalid module name- %s", name);
		return -EINVAL;
	}

	init_rk_debug();

	property_set("vendor.gralloc.no_afbc_for_fb_target_layer","1");

	std::unique_ptr<hwc_context_t> ctx(new hwc_context_t());
	if (!ctx) {
		ALOGE("Failed to allocate hwc context");
		return -ENOMEM;
	}

	int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&ctx->gralloc);
	if (ret) {
		ALOGE("Failed to open gralloc module %d", ret);
		return ret;
	}

	ret = hwc_enumerate_displays(ctx.get());
	if (ret) {
		ALOGE("Failed to enumerate displays: %s", strerror(ret));
		return ret;
	}

	ctx->device.common.tag = HARDWARE_DEVICE_TAG;
	ctx->device.common.version = HWC_DEVICE_API_VERSION_1_4;
	ctx->device.common.module = const_cast<hw_module_t *>(module);
	ctx->device.common.close = hwc_device_close;

	ctx->device.dump = hwc_dump;
	ctx->device.prepare = hwc_prepare;
	ctx->device.set = hwc_set;
	ctx->device.eventControl = hwc_event_control;
	ctx->device.setPowerMode = hwc_set_power_mode;
	ctx->device.query = hwc_query;
	ctx->device.registerProcs = hwc_register_procs;
	ctx->device.getDisplayConfigs = hwc_get_display_configs;
	ctx->device.getDisplayAttributes = hwc_get_display_attributes;
	ctx->device.getActiveConfig = hwc_get_active_config;
	ctx->device.setActiveConfig = hwc_set_active_config;
	ctx->device.setCursorPositionAsync = NULL; /* TODO: Add cursor */

	g_ctx = ctx.get();

	ebc_fd = open("/dev/ebc", O_RDWR, 0);
	if (ebc_fd < 0) {
		ALOGE("open /dev/ebc failed\n");
		return -1;
	}

	if (ioctl(ebc_fd, EBC_GET_BUFFER_INFO, &ebc_buf_info) != 0) {
		ALOGE("EBC_GET_BUFFER_INFO failed\n");
		close(ebc_fd);
		return -1;
	}

	hwc_init_version();

	*dev = &ctx->device.common;
	ctx.release();

	return 0;
}
}

static struct hw_module_methods_t hwc_module_methods = {
	.open = android::hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.version_major = 1,
		.version_minor = 0,
		.id = HWC_HARDWARE_MODULE_ID,
		.name = "DRM hwcomposer module",
		.author = "The Android Open Source Project",
		.methods = &hwc_module_methods,
		.dso = NULL,
		.reserved = {0},
	}
};
