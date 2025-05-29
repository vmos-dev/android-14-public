/**
  * Copyright (c) 2023 Rockchip Electronic Co.,Ltd
  * kenvenis.chen@rock-chips.com
  */

#ifndef _RK_AVM_API_H_
#define _RK_AVM_API_H_

#include <string>
#include <fstream>
#include <iomanip>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <utils/Log.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <ui/GraphicBuffer.h>

#define USING_DMAHEAP_FOR_EVS 1

enum RK_AVM_CMD
{
    rk_cmd_render_front_wide = 0x0,     /* ??????? */
    rk_cmd_render_front_top_fish_eye,   /* ????? + ???? */
    rk_cmd_render_front_top_far,        /* ????? + ???? */
    rk_cmd_render_front_3D,             /* ???3D */

    rk_cmd_render_back_wide,            /* ???????? */
    rk_cmd_render_back_top_fish_eye,    /* ?????? + ???? */
    rk_cmd_render_back_top_far,         /* ?????? + ???? */
    rk_cmd_render_back_3D,              /* ????3D */

    rk_cmd_render_left_top_2D,          /* ?????? + 2D */
    rk_cmd_render_left_top_3D,          /* ?????? + 3D */
    rk_cmd_render_right_top_2D = 0xB,           /* ?????? + 2D */
    rk_cmd_render_right_top_3D,         /* ?????? + 2D */

    rk_cmd_render_left_right_top_2D = 0xE,  /* ???????? + 2D */
    rk_cmd_render_left_right_top_3D,

    rk_cmd_render_3d_view_adjuest,      /* ?????3D */
    rk_cmd_render_top_3d_view_adjuest,/* ?????3D */
    rk_cmd_render_set_screen_offset,
    rm_cmd_render_set_vehicle_enable,
    rm_cmd_render_set_vehicle_disable,

    rk_cmd_calib_init = 0x40,
    rk_cmd_calib_defish,
    rk_cmd_calib_search_count,
    rk_cmd_calib_grid,
    rk_cmd_calib_deinit,

    rk_cmd_avm_control_init = 0x60,
    rk_cmd_avm_control_render_fd,
    rk_cmd_avm_control_render_viraddr,
    rk_cmd_avm_control_single_copy,
    rk_cmd_avm_control_deinit,
};

typedef struct InputBufDec
{
    void *addr;
    int w;
    int h;
    int cam_id;
    int pixformat;
    bool flag;
} InputBufDec_t;

int rk_avm_rev_cmd_proc(int cmd_type, void *data);

void rkavm_sync();

int rk_avm_get_surround_type();

void rk_avm_set_state_stop();

int get_rk_avm_framebuffer_fd(int index);

void rk_avm_egl_init(int egl_type, int dwidth, int dheight);

void rk_avm_set_extern_buf_enable(bool ext_enable);

void rk_avm_set_ctx(EGLContext *ctx);

void rk_avm_set_face(EGLSurface *sface);

void rk_avm_set_dis(EGLDisplay *sdis);

int rk_avm_get_share_tex(int index);

void rk_avm_set_cam_input_type_bpp(int type_bpp);

int rk_avm_init_set_output_size(int width, int height);

void rk_avm_cam_param_init(int cam_num, int width,
                           int height, int cam_type);

void rk_avm_set_model_scale_factor(float x, float y, float z);

void rk_avm_set_calib_data_path(std::string calib_result);

void rk_avm_set_base_data_path(std::string data_base);

int rk_avm_get_camera_id(int index);

int rk_avm_init(const char *xml_setting, bool is_using_xml_init);

void rk_avm_deinit();

void rk_avm_send_share_fd(int *fd, int camera_id);

void rk_avm_set_data_fd(int *fd, int *frame_index, unsigned char *disp_buf);

void rk_avm_set_input_buf_single(void *src_addr, int w, int h, int cam_id, int pixel_format, bool is_src_fd);

void rk_avm_render_buf_single_tran(uint32_t src_buffer_handle, int w, int h, int cam_id, int pixel_format);

void rk_avm_gl_render(void *addr, bool is_dst_fd);

int rga_simple_transfer(uint32_t src_handle, 
						uint32_t dst_handle, 
						int swidth, int sheight,
						int dwidth, int dheight,
						int sformat, int dformat);
int rga_crop_transfer_raw(uint32_t src_handle, 
						uint32_t dst_handle, 
						int swidth, int sheight,
						int swstride, int shstride,
						int dwidth, int dheight,
						int dwstride, int dhstride,
						int dwtop, int dhtop,
						int sformat, int dformat);
void rk_avm_render_update(int *src_addr_array, long dst_addr, int buffer_num, int pixel_format, bool is_src_fd, bool is_dst_fd);

void rk_avm_render_update_test(int *src_addr_array, long dst_addr, int buffer_num, int pixel_format,
                               int src_w, int src_h, int dst_w, int dst_h, bool is_src_fd, bool is_dst_fd);
void rk_avm_split_transfer_input_data(void *input_daddr, int iw, int ih, int camId);
bool rkavm_get_init_state(void);
void rkavm_control_init(void);
void rkavm_control_exit(void);
void rkavm_set_evs_apk_show(bool flag);
void rk_avm_destroy_img(int camId);
bool rk_avm_render_update_android_tex(int camId, const native_handle_t *inHandle, android::GraphicBuffer::HandleWrapMethod method, uint32_t inWidth,
                                      uint32_t inHeight, int32_t inFormat, uint32_t inLayerCount, uint64_t inUsage,
                                      uint32_t inStride);
bool rk_avm_render_update_android_hardwarebuffer(int camId, const AHardwareBuffer* ahardwarebuffer);
void rk_avm_set_input_tex_single(int camID, unsigned int texid);
#if USING_DMAHEAP_FOR_EVS
void *render_dma_heap_buf(int *export_fd, int width, int height, int bpp);
bool rkavm_get_int_state();
uint32_t rk_avm_get_evs_handle(int index);
int rk_avm_get_evs_width();
int rk_avm_get_evs_height();
int rk_avm_get_evs_fd(int index);
void rk_avm_set_display(int dw, int dh);
void rk_avm_destroy_shader();
uint32_t rk_avm_get_fbo_handle(int index);
void render_get_avm_display(EGLDisplay *renderPlay);
void render_get_avm_context(EGLContext *renderFace);
#endif

#endif
