/*
 * Copyright (C) 2018-2020 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
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
#include <errno.h>
#include <inttypes.h>

#include <hardware/gralloc1.h>

#include "mali_gralloc_private_interface_types.h"
#include "mali_gralloc_buffer.h"
#include "mali_gralloc_formats.h"
#include "mali_gralloc_usages.h"
#include "allocator/allocator.h"
#include "gralloc_helper.h"

namespace legacy
{

enum tx_direction
{
	TX_NONE = 0,
	TX_TO_DEVICE,
	TX_FROM_DEVICE,
	TX_BOTH,
};

static enum tx_direction get_tx_direction(const uint64_t usage)
{
	const bool read = (usage & GRALLOC_USAGE_SW_READ_MASK) ? true : false;
	const bool write = (usage & GRALLOC_USAGE_SW_WRITE_MASK) ? true : false;
	enum tx_direction dir = TX_NONE;

	if (read && write)
	{
		dir = TX_BOTH;
	}
	else if (write)
	{
		dir = TX_TO_DEVICE;
	}
	else if (read)
	{
		dir = TX_FROM_DEVICE;
	}

	return dir;
}

static void buffer_sync(private_handle_t * const hnd,
                        const enum tx_direction direction)
{
	if (hnd->flags & private_handle_t::PRIV_FLAGS_USES_DBH)
	{
		if (direction != TX_NONE)
		{
			hnd->cpu_read = (direction == TX_FROM_DEVICE || direction == TX_BOTH) ? 1 : 0;
			hnd->cpu_write = (direction == TX_TO_DEVICE || direction == TX_BOTH) ? 1 : 0;

			const int status = allocator_sync_start(hnd,
			                                               hnd->cpu_read ? true : false,
			                                               hnd->cpu_write ? true : false);
			if (status < 0)
			{
				return;
			}
		}
		else if (hnd->cpu_read || hnd->cpu_write)
		{
			const int status = allocator_sync_end(hnd,
			                                             hnd->cpu_read ? true : false,
			                                             hnd->cpu_write ? true : false);
			if (status < 0)
			{
				return;
			}

			hnd->cpu_read = 0;
			hnd->cpu_write = 0;
		}
	}
}

int mali_gralloc_lock(buffer_handle_t buffer, uint64_t usage, int l, int t, int w, int h,
                      void **vaddr)
{
	GRALLOC_UNUSED(l);
	GRALLOC_UNUSED(t);
	GRALLOC_UNUSED(w);
	GRALLOC_UNUSED(h);

	if (private_handle_t::validate(buffer) < 0)
	{
		MALI_GRALLOC_LOGE("Locking invalid buffer %p, returning error", buffer);
		return -EINVAL;
	}

	private_handle_t *hnd = static_cast<private_handle_t *>(const_cast<native_handle_t *>(buffer) );

	if (hnd->req_format == HAL_PIXEL_FORMAT_YCbCr_420_888)
	{
		MALI_GRALLOC_LOGE("Buffers with format YCbCr_420_888 must be locked using (*lock_ycbcr)");
		return -EINVAL;
	}

	if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK))
	{
		*vaddr = static_cast<void *>(hnd->base);

		buffer_sync(hnd, get_tx_direction(usage));
	}

	return 0;
}

int mali_gralloc_lock_ycbcr(buffer_handle_t buffer, uint64_t usage, int l, int t, int w,
                            int h, android_ycbcr *ycbcr)
{
	GRALLOC_UNUSED(l);
	GRALLOC_UNUSED(t);
	GRALLOC_UNUSED(w);
	GRALLOC_UNUSED(h);

	if (private_handle_t::validate(buffer) < 0)
	{
		MALI_GRALLOC_LOGE("Locking invalid buffer %p, returning error", buffer);
		return -EINVAL;
	}

	if (NULL == ycbcr)
	{
		return -EINVAL;
	}

	private_handle_t *hnd = static_cast<private_handle_t *>(const_cast<native_handle_t *>(buffer) );

	if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK) &&
	    !(hnd->internal_format & MALI_GRALLOC_INTFMT_EXT_MASK))
	{
		char *base = static_cast<char *>(hnd->base);
		int y_stride = hnd->byte_stride;
		/* Ensure height is aligned for subsampled chroma before calculating buffer parameters */
		int adjusted_height = GRALLOC_ALIGN(hnd->height, 2);
		int y_size = y_stride * adjusted_height;

		int u_offset = 0;
		int v_offset = 0;
		int c_stride = 0;
		int step = 0;

		uint64_t base_format = hnd->internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

		switch (base_format)
		{
		case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
			c_stride = y_stride;
			/* Y plane, UV plane */
			u_offset = y_size;
			v_offset = y_size + 1;
			step = 2;
			break;

		case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
			c_stride = y_stride;
			/* Y plane, UV plane */
			v_offset = y_size;
			u_offset = y_size + 1;
			step = 2;
			break;

		case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
		{
			int c_size;

			/* Stride alignment set to 16 as the SW access flags were set */
			c_stride = GRALLOC_ALIGN(hnd->byte_stride / 2, 16);
			c_size = c_stride * (adjusted_height / 2);
			/* Y plane, V plane, U plane */
			v_offset = y_size;
			u_offset = y_size + c_size;
			step = 1;
			break;
		}

		default:
			MALI_GRALLOC_LOGE("Can't lock buffer %p: wrong format %" PRIx64, hnd, hnd->internal_format);
			return -EINVAL;
		}

		ycbcr->y = base;
		ycbcr->cb = base + u_offset;
		ycbcr->cr = base + v_offset;
		ycbcr->ystride = y_stride;
		ycbcr->cstride = c_stride;
		ycbcr->chroma_step = step;

		buffer_sync(hnd, get_tx_direction(usage));
	}
	else
	{
		MALI_GRALLOC_LOGE("Don't support to lock buffer %p: with format %" PRIx64, hnd, hnd->internal_format);
		return -EINVAL;
	}

	return 0;
}

int mali_gralloc_unlock(buffer_handle_t buffer)
{
	if (private_handle_t::validate(buffer) < 0)
	{
		MALI_GRALLOC_LOGE("Unlocking invalid buffer %p, returning error", buffer);
		return -EINVAL;
	}

	private_handle_t *hnd = static_cast<private_handle_t *>(const_cast<native_handle_t *>(buffer) );
	buffer_sync(hnd, TX_NONE);

	return 0;
}

int mali_gralloc_get_num_flex_planes(buffer_handle_t buffer, uint32_t *num_planes)
{
	if (private_handle_t::validate(buffer) < 0)
	{
		MALI_GRALLOC_LOGE("Invalid buffer %p, returning error", buffer);
		return -EINVAL;
	}

	if (NULL == num_planes)
	{
		return -EINVAL;
	}

	private_handle_t *hnd = static_cast<private_handle_t *>(const_cast<native_handle_t *>(buffer) );
	uint64_t base_format = hnd->internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

	switch (base_format)
	{
	case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
	case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
	case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
		*num_planes = 3;
		break;

	default:
		MALI_GRALLOC_LOGE("Can't get planes number of buffer %p: with format %" PRIx64, hnd, hnd->internal_format);
		return -EINVAL;
	}

	return 0;
}

int mali_gralloc_lock_flex(buffer_handle_t buffer, uint64_t usage, int l, int t, int w, int h,
                           struct android_flex_layout *flex_layout)
{
	GRALLOC_UNUSED(l);
	GRALLOC_UNUSED(t);
	GRALLOC_UNUSED(w);
	GRALLOC_UNUSED(h);

	if (private_handle_t::validate(buffer) < 0)
	{
		MALI_GRALLOC_LOGE("Locking invalid buffer %p, returning error", buffer);
		return -EINVAL;
	}

	if (NULL == flex_layout)
	{
		return -EINVAL;
	}

	private_handle_t *hnd = static_cast<private_handle_t *>(const_cast<native_handle_t *>(buffer) );

	if (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK) &&
	    !(hnd->internal_format & MALI_GRALLOC_INTFMT_EXT_MASK))
	{
		uint8_t *base = static_cast<uint8_t *>(hnd->base);
		int y_stride = hnd->byte_stride;
		/* Ensure height is aligned for subsampled chroma before calculating buffer parameters */
		int adjusted_height = GRALLOC_ALIGN(hnd->height, 2);
		int y_size = y_stride * adjusted_height;

		uint64_t base_format = hnd->internal_format & MALI_GRALLOC_INTFMT_FMT_MASK;

		switch (base_format)
		{
		case MALI_GRALLOC_FORMAT_INTERNAL_NV12:
			flex_layout->format = FLEX_FORMAT_YCbCr;
			flex_layout->num_planes = 3;
			flex_layout->planes[0].top_left = base;
			flex_layout->planes[0].component = FLEX_COMPONENT_Y;
			flex_layout->planes[0].bits_per_component = 8;
			flex_layout->planes[0].bits_used = 8;
			flex_layout->planes[0].h_increment = 1;
			flex_layout->planes[0].v_increment = y_stride;
			flex_layout->planes[0].h_subsampling = 1;
			flex_layout->planes[0].v_subsampling = 1;
			flex_layout->planes[1].top_left = base + y_size;
			flex_layout->planes[1].component = FLEX_COMPONENT_Cb;
			flex_layout->planes[1].bits_per_component = 8;
			flex_layout->planes[1].bits_used = 8;
			flex_layout->planes[1].h_increment = 2;
			flex_layout->planes[1].v_increment = y_stride;
			flex_layout->planes[1].h_subsampling = 2;
			flex_layout->planes[1].v_subsampling = 2;
			flex_layout->planes[2].top_left = flex_layout->planes[1].top_left + 1;
			flex_layout->planes[2].component = FLEX_COMPONENT_Cr;
			flex_layout->planes[2].bits_per_component = 8;
			flex_layout->planes[2].bits_used = 8;
			flex_layout->planes[2].h_increment = 2;
			flex_layout->planes[2].v_increment = y_stride;
			flex_layout->planes[2].h_subsampling = 2;
			flex_layout->planes[2].v_subsampling = 2;
			break;

		case MALI_GRALLOC_FORMAT_INTERNAL_NV21:
			/*
			 * NV21: YCrCb/YVU ordering. The flex format
			 * plane order must still follow YCbCr order
			 * (as defined by 'android_flex_component_t').
			 */
			flex_layout->format = FLEX_FORMAT_YCbCr;
			flex_layout->num_planes = 3;
			flex_layout->planes[0].top_left = base;
			flex_layout->planes[0].component = FLEX_COMPONENT_Y;
			flex_layout->planes[0].bits_per_component = 8;
			flex_layout->planes[0].bits_used = 8;
			flex_layout->planes[0].h_increment = 1;
			flex_layout->planes[0].v_increment = y_stride;
			flex_layout->planes[0].h_subsampling = 1;
			flex_layout->planes[0].v_subsampling = 1;
			flex_layout->planes[1].top_left = base + y_size + 1;
			flex_layout->planes[1].component = FLEX_COMPONENT_Cb;
			flex_layout->planes[1].bits_per_component = 8;
			flex_layout->planes[1].bits_used = 8;
			flex_layout->planes[1].h_increment = 2;
			flex_layout->planes[1].v_increment = y_stride;
			flex_layout->planes[1].h_subsampling = 2;
			flex_layout->planes[1].v_subsampling = 2;
			flex_layout->planes[2].top_left = base + y_size;
			flex_layout->planes[2].component = FLEX_COMPONENT_Cr;
			flex_layout->planes[2].bits_per_component = 8;
			flex_layout->planes[2].bits_used = 8;
			flex_layout->planes[2].h_increment = 2;
			flex_layout->planes[2].v_increment = y_stride;
			flex_layout->planes[2].h_subsampling = 2;
			flex_layout->planes[2].v_subsampling = 2;
			break;

		case MALI_GRALLOC_FORMAT_INTERNAL_YV12:
		{
			int c_size;
			int c_stride;
			/* Stride alignment set to 16 as the SW access flags were set */
			c_stride = GRALLOC_ALIGN(hnd->byte_stride / 2, 16);
			c_size = c_stride * (adjusted_height / 2);

			/*
			 * YV12: YCrCb/YVU ordering. The flex format
			 * plane order must still follow YCbCr order
			 * (as defined by 'android_flex_component_t').
			 */
			flex_layout->format = FLEX_FORMAT_YCbCr;
			flex_layout->num_planes = 3;
			flex_layout->planes[0].top_left = base;
			flex_layout->planes[0].component = FLEX_COMPONENT_Y;
			flex_layout->planes[0].bits_per_component = 8;
			flex_layout->planes[0].bits_used = 8;
			flex_layout->planes[0].h_increment = 1;
			flex_layout->planes[0].v_increment = y_stride;
			flex_layout->planes[0].h_subsampling = 1;
			flex_layout->planes[0].v_subsampling = 1;
			flex_layout->planes[1].top_left = base + y_size + c_size;
			flex_layout->planes[1].component = FLEX_COMPONENT_Cb;
			flex_layout->planes[1].bits_per_component = 8;
			flex_layout->planes[1].bits_used = 8;
			flex_layout->planes[1].h_increment = 1;
			flex_layout->planes[1].v_increment = c_stride;
			flex_layout->planes[1].h_subsampling = 2;
			flex_layout->planes[1].v_subsampling = 2;
			flex_layout->planes[2].top_left = base + y_size;
			flex_layout->planes[2].component = FLEX_COMPONENT_Cr;
			flex_layout->planes[2].bits_per_component = 8;
			flex_layout->planes[2].bits_used = 8;
			flex_layout->planes[2].h_increment = 1;
			flex_layout->planes[2].v_increment = c_stride;
			flex_layout->planes[2].h_subsampling = 2;
			flex_layout->planes[2].v_subsampling = 2;
			break;
		}

		default:
			MALI_GRALLOC_LOGE("Can't lock buffer %p: wrong format %" PRIx64, hnd, hnd->internal_format);
			return -EINVAL;
		}

		buffer_sync(hnd, get_tx_direction(usage));
	}
	else
	{
		MALI_GRALLOC_LOGE("Don't support to lock buffer %p: with format %" PRIx64, hnd, hnd->internal_format);
		return -EINVAL;
	}

	return 0;
}

} /* namespace legacy */
