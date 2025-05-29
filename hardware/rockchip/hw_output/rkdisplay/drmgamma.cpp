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
#include "drmgamma.h"
#include "math.h"

namespace android {

#define RKAG_GAMMA_TAB_LENGTH 1024
#define	MAX(a, b)			    		                ( (a) > (b) ? (a) : (b) )
#define	MIN(a, b)			    		                ( (a) < (b) ? (a) : (b) )
#define	CLIP(x,min_v,max_v)						        MIN(MAX(x,min_v),max_v)

DrmGamma::DrmGamma(){
}

DrmGamma::~DrmGamma(){
}

static uint32_t get_property_id(int fd, drmModeObjectProperties *props,
				const char *name)
{
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

int DrmGamma::get_cubic_lut_size(int fd, unsigned crtc_id) {
	int cubic_lut_size = 0;
	drmModeObjectProperties *props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	for (int i = 0; i < props->count_props; i++) {
		drmModePropertyPtr prop;
		prop = drmModeGetProperty(fd, props->props[i]);
		if (prop) {
			if (strcmp(prop->name, "CUBIC_LUT_SIZE") == 0) {
				cubic_lut_size = props->prop_values[i];
			}
			drmModeFreeProperty(prop);
		}
	}
	drmModeFreeObjectProperties(props);
	return cubic_lut_size;
}

int DrmGamma::convert_3d_lut_data(int src_size, int dst_size, uint16_t* src_red,
	uint16_t* src_green, uint16_t* src_blue, uint16_t* dst_red, uint16_t* dst_green, uint16_t* dst_blue) {
	if (src_size == dst_size) {
		for (int i = 0; i < dst_size; i++) {
			dst_red[i] = src_red[i];
			dst_green[i] = src_green[i];
			dst_blue[i] = src_blue[i];
		}
		return 0;
	}

	src_size = (int)pow((double)src_size, 1.0/3.0) + 1;
	dst_size = (int)pow((double)dst_size, 1.0/3.0) + 1;

	uint16_t pix;
	int src_chl = 3;
	int ds_rate = src_size / dst_size + 1;
	int src_std0 = src_size * src_size * src_chl;
	int src_std1 = src_size * src_chl;
	int dst_std0 = dst_size * dst_size * src_chl;
	int dst_std1 = dst_size * src_chl;
	uint16_t src_lut[src_size * src_size * src_size * 3];
	uint16_t dst_lut[dst_size * dst_size * dst_size * 3];
	for (int i = 0; i < src_size * src_size * src_size; i++) {
		src_lut[3 * i] = src_blue[i];
		src_lut[3 * i + 1] = src_green[i];
		src_lut[3 * i + 2] = src_red[i];
	}

	for (int i = 0; i < dst_size; i++) {
		for (int j = 0; j < dst_size; j++) {
			for (int k = 0; k < dst_size; k++) {
				for (int c = 0; c < src_chl; c++) {
					int ii = i * ds_rate;
					int jj = j * ds_rate;
					int kk = k * ds_rate;
					pix = src_lut[ii * src_std0 + jj * src_std1 + kk * src_chl + c];
					dst_lut[i * dst_std0 + j * dst_std1 + k * src_chl + c] = pix;
				}
			}
		}
	}

	for (int i = 0; i < dst_size * dst_size * dst_size; i++) {
		dst_blue[i] = dst_lut[3 * i];
		dst_green[i] = dst_lut[3 * i + 1];
		dst_red[i] = dst_lut[3 * i + 2];
	}

	return 0;
}

int DrmGamma::set_3x1d_gamma(int fd, unsigned crtc_id, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
	unsigned blob_id = 0;
	drmModeObjectProperties *props;
	struct drm_color_lut gamma_lut[size];
	int i, ret;
	for (i = 0; i < size; i++) {
		gamma_lut[i].red = r[i];
		gamma_lut[i].green = g[i];
		gamma_lut[i].blue = b[i];
	}
	props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	uint32_t property_id = get_property_id(fd, props, "GAMMA_LUT");
	if(property_id == 0){
		ALOGE("can't find GAMMA_LUT");
	}
	drmModeCreatePropertyBlob(fd, gamma_lut, sizeof(gamma_lut), &blob_id);
	ret = drmModeObjectSetProperty(fd, crtc_id, DRM_MODE_OBJECT_CRTC, property_id, blob_id);
	drmModeDestroyPropertyBlob(fd, blob_id);
	drmModeFreeObjectProperties(props);
	return ret;
}

int DrmGamma::set_cubic_lut(int fd, unsigned crtc_id, uint32_t size, uint16_t* r, uint16_t* g, uint16_t* b)
{
	unsigned blob_id = 0;
	drmModeObjectProperties *props;
	struct drm_color_lut cubic_lut[size];
	int i, ret;
	for (i = 0; i < size; i++) {
		cubic_lut[i].red = r[i];
		cubic_lut[i].green = g[i];
		cubic_lut[i].blue = b[i];
	}
	props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	uint32_t property_id = get_property_id(fd, props, "CUBIC_LUT");
	if(property_id == 0){
		ALOGE("can't find CUBIC_LUT");
	}
	drmModeCreatePropertyBlob(fd, cubic_lut, sizeof(cubic_lut), &blob_id);
	ret = drmModeObjectSetProperty(fd, crtc_id, DRM_MODE_OBJECT_CRTC, property_id, blob_id);
	drmModeDestroyPropertyBlob(fd, blob_id);
	drmModeFreeObjectProperties(props);
	return ret;
}

int DrmGamma::gamma_color_temp_adjust(uint16_t* r, uint16_t* g, uint16_t* b, uint32_t rgain, uint32_t ggain, uint32_t bgain)
{
	const unsigned short max_vl = 65535;
	const int gain_bit = 8;

	for (int i = 0; i < RKAG_GAMMA_TAB_LENGTH; i++)
	{
		r[i] = CLIP((r[i] * rgain + (1 << (gain_bit - 1))) >> (gain_bit), 0, max_vl);
		g[i] = CLIP((g[i] * ggain + (1 << (gain_bit - 1))) >> (gain_bit), 0, max_vl);
		b[i] = CLIP((b[i] * bgain + (1 << (gain_bit - 1))) >> (gain_bit), 0, max_vl);
	}
	return 0;
}

}

