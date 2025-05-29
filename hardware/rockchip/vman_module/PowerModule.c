#include <hardware/hardware.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <log/log.h>
#include "PowerModule.h"

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "vman_power"
#endif

struct rk_vendor_req {
	__u32 tag;
	__u16 id;
	__u16 len;
	__u8 data[1];
};

#define VENDOR_REQ_TAG   0x56524551
#define VENDOR_READ_IO   _IOW('v', 0x01, struct rk_vendor_req)
#define VENDOR_WRITE_IO  _IOW('v', 0x02, struct rk_vendor_req)

#define VENDOR_POWERMODE_ID 0x22

/*
 * Get power-on mode
 * @return mode, range[ui_power_mode_t]
 */
static ui_power_mode_t get_power_mode(struct power_hal_module *module)
{
	int ret = 0;
	__u8 p_buf[40];
	struct rk_vendor_req *req;

	req = (struct rk_vendor_req *)p_buf;
	int sys_fd = open("/dev/vendor_storage", O_RDWR, 0);
	if(sys_fd < 0) {
		ALOGD("vendor_storage open fail\n");
		return 1;
	}

	req->tag = VENDOR_REQ_TAG;
	req->id = VENDOR_POWERMODE_ID;
	req->len = 1;

	ret = ioctl(sys_fd, VENDOR_READ_IO, req);
	/* return req->len is the real data length stored in the NV-storage */
	if(ret){
		ALOGD("vendor read error n");
		ret = 1;
	}

	close(sys_fd);

	return ret;
}

/*
 * Set power-on mode
 * @param value: mode, range[ui_power_mode_t]
 * @return result [0: successfully, 1: failure]
 * @note default: POWER_MODE_OFF, power-off will save
 */
static int set_power_mode(struct power_hal_module *module, ui_power_mode_t value)
{

	int ret;
	__u8 p_buf[40];
	struct rk_vendor_req *req;

	if (value > POWER_MODE_LAST){
		ALOGD("mode val is invalid!!!\n");
		return 1;
	}

	req = (struct rk_vendor_req *)p_buf;
	int sys_fd = open("/dev/vendor_storage", O_RDWR, 0);
	if(sys_fd < 0) {
		ALOGD("vendor_storage open fail\n");
		return 1;
	}

	req->tag = VENDOR_REQ_TAG;
	req->id = VENDOR_POWERMODE_ID;
	req->len = 1; /* max read length to read*/

	req->data[0] = value;

	ret = ioctl(sys_fd, VENDOR_WRITE_IO, req);
	if(ret){
		ALOGD("vendor read error n");
		ret = 1;
	}

	close(sys_fd);

	return 0;
}

static struct hw_module_methods_t power_hal_module_methods =
{
	.open = NULL,
};

struct power_hal_module HAL_MODULE_INFO_SYM =
{
	.common = {
		.tag = HARDWARE_MODULE_TAG,
		.module_api_version = POWER_HAL_MODULE_API_VERSION,
		.hal_api_version = HARDWARE_HAL_API_VERSION,
		.id = POWER_HAL_HARDWARE_MODULE_ID,
		.name ="Default Power HAL",
		.author = "The Power Project",
		.methods = &power_hal_module_methods,
	},
	.get_power_mode = get_power_mode,
	.set_power_mode = set_power_mode,
};
