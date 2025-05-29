#ifndef RK_HDMI_CEC_9302093883_H
#define RK_HDMI_CEC_9302093883_H

#include <hardware/hdmi_cec.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "hdmicec"
#endif
struct hdmi_cec_context_t {
    //hdmi_cec_device_t device;
    /* our private state goes below here */
	event_callback_t event_callback;
	void* cec_arg;
	struct hdmi_port_info* port;
	int fd;
	int en_mask;
	bool enable;
	bool system_control;
	int phy_addr;
	bool hotplug;
	bool cec_init;
};
void init_uevent_thread(struct hdmi_cec_context_t* ctx);

int rk_hdmi_cec_init(struct hdmi_cec_context_t *dev);
int rk_hdmi_cec_destroy(struct hdmi_cec_context_t *ctx);


int hdmi_cec_add_logical_address(struct hdmi_cec_context_t* ctx ,cec_logical_address_t addr);
void hdmi_cec_clear_logical_address(struct hdmi_cec_context_t* ctx);
int hdmi_cec_get_physical_address(struct hdmi_cec_context_t* ctx, uint16_t* addr);
int hdmi_cec_send_message(struct hdmi_cec_context_t* ctx, const cec_message_t* message);
void hdmi_cec_register_event_callback(struct hdmi_cec_context_t* ctx, event_callback_t callback, void* arg);
void hdmi_cec_get_version(struct hdmi_cec_context_t* ctx, int* version);
void hdmi_cec_get_vendor_id(struct hdmi_cec_context_t* ctx, uint32_t* vendor_id);
void hdmi_cec_set_option(struct hdmi_cec_context_t* ctx, int flag, int value);
void hdmi_cec_set_audio_return_channel(struct hdmi_cec_context_t* ctx, int port_id, int flag);
int hdmi_cec_is_connected(struct hdmi_cec_context_t* ctx, int port_id);



#endif
