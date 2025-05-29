#pragma once

#include <rkpq.h>

namespace android {

#define PQ_CONFIG_PATH "/data/vendor/rkalgo/pq_config.json"
#define PQ_DUMP_IN_PATH_PREFIX "/data/vendor/rkalgo/pq_dump_in"
#define PQ_DUMP_OUT_PATH_PREFIX "/data/vendor/rkalgo/pq_dump_out"

class RkPqInterface {
public:
    RkPqInterface();
    virtual ~RkPqInterface();
    virtual bool init(uint32_t src_width, uint32_t src_height, uint32_t* src_width_stride, uint32_t dst_width, uint32_t dst_height, 
        uint32_t alignment, uint32_t src_pix_format, uint32_t src_color_space, uint32_t dst_pix_format, uint32_t dst_color_space, uint32_t flag) = 0;
    char* format_to_str(int format);
    int convert_format(int format);
    void dump_data(char* path_prefix, int width, int height, int format ,char* dst_virtual, size_t size);
};

class RkPqInterface3588 : public RkPqInterface {
public:
    RkPqInterface3588();
    ~RkPqInterface3588();
    bool init(uint32_t src_width, uint32_t src_height, uint32_t* src_width_stride, uint32_t dst_width, uint32_t dst_height, 
        uint32_t alignment, uint32_t src_pix_format, uint32_t src_color_space, uint32_t dst_pix_format, uint32_t dst_color_space, uint32_t flag);
    bool dopq(uint32_t src_fd, uint32_t dst_fd, uint32_t mode);
};

#ifdef USE_LIBPQ_HWPQ
class RkPqInterface3576 : public RkPqInterface {
public:
    RkPqInterface3576();
    ~RkPqInterface3576();
    bool init(uint32_t src_width, uint32_t src_height, uint32_t* src_width_stride, uint32_t dst_width, uint32_t dst_height, 
        uint32_t alignment, uint32_t src_pix_format, uint32_t src_color_space, uint32_t dst_pix_format, uint32_t dst_color_space, uint32_t flag);
    bool dohwpq(HwPqImageInfo src, HwPqImageInfo dst, HwPqPreInfo preInfo);

private:
    rk_hwpq_cfg hwpq_cfg;
    rk_hwpq_reg hwpq_reg;
    uint8_t*    p_hist_buf;
    bool dovdpp(HwPqImageInfo src, HwPqImageInfo dst);
};
#endif
}

