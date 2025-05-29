#include <jni.h>
#include <functional>
#include <string>
#include <time.h>
typedef std::function<void(std::string id)> bridge_on_connect;
typedef std::function<void(std::string id)> bridge_on_disconnect;
typedef std::function<void(std::string id,int width,int height)> bridge_on_format_change;
struct bridge_callback{
    bridge_on_connect connect;
    bridge_on_disconnect disconnect;
    bridge_on_format_change format_change;
};

void init();
void deinit();
void set_rgba_1080x1920(uint8_t* rgba,int size);
void set_nv12_1920x1080(uint8_t* nv12,int size);
void set_nv12_3840x2160(uint8_t* nv12,int size);
void set_nv12_1792x3040(uint8_t* nv12,int size);

void* get_nv12_1920x1080();
void* get_nv12_3840x2160();
void rga_proc();
void rga_proc(JNIEnv *env, jobject hardware_buffer);
void rga_proc_fill_in(JNIEnv *env, jobject hardware_buffer,int index);
void get_mipi_status();
void get_hdmirx_status();
void set_hdmi_callback(struct bridge_callback *callback);


#define MS_PER_SEC 1000ULL
#define NS_PER_SEC              1000000000ULL

#define NS_PER_MS (NS_PER_SEC /MS_PER_SEC)

static inline long get_time_diff_ms(struct timespec *from,
                                    struct timespec *to) {
    return (to->tv_sec - from->tv_sec) * (long)MS_PER_SEC +
           (to->tv_nsec - from->tv_nsec) / (long)NS_PER_MS;
}
