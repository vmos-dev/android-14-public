#include "autopq.h"

namespace android {

#define UN_USED(x) (void)(x)

AutoPQ::AutoPQ(){
}

AutoPQ::~AutoPQ(){
}

static int get_auto_pq(auto_pq_info* auto_pq) {
    int file, ret;
    file = open(AUTOPQ_PATH, O_RDWR);
    if (file < 0) {
        ALOGE("auto can not be opened \n");
        sync();
        return -EIO;
    }
    lseek(file, 0, SEEK_SET);
    ret = read(file, auto_pq, sizeof(auto_pq_info));
    if(ret < 0){
        ALOGE("fail to read");
        sync();
        close(file);
        return -EIO;
    }

    if (memcmp(auto_pq->head_flag, "AUTOPQ", 6) != 0) {
        ALOGE("head flag no fount");
        sync();
        close(file);
        return -EPERM;
    }
    sync();
    close(file);
    return 0;
}

static int set_auto_pq(auto_pq_info* auto_pq) {
    int file, ret;
    file = open(AUTOPQ_PATH, O_RDWR);
    if (file < 0) {
        ALOGE("auto can not be opened \n");
        sync();
        return -EIO;
    }
    lseek(file, 0, SEEK_SET);
    ret = write(file, (char*)auto_pq, sizeof(auto_pq_info));
    if(ret < 0){
        ALOGE("fail to write");
        sync();
        close(file);
        return -EIO;
    }
    sync();
    close(file);
    return 0;
}

int AutoPQ::get_auto_white_balance(unsigned short index, struct white_balance_data *data) {
    struct auto_pq_info auto_pq;
    int ret = get_auto_pq(&auto_pq);
    if(ret < 0){
        return ret;
    }else {
        memcpy(data, &(auto_pq.white_balance[index]) ,sizeof(white_balance_data));
        return 0;
    }
}

int AutoPQ::set_auto_white_balance(unsigned short index, struct white_balance_data *data) {
    struct auto_pq_info auto_pq;
    int ret = get_auto_pq(&auto_pq);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(auto_pq.white_balance[index]), data, sizeof(white_balance_data));
        return set_auto_pq(&auto_pq);
    }
}

int AutoPQ::get_auto_gamma(unsigned short index, struct gamma_lut_data *data) {
    struct auto_pq_info auto_pq;
    int ret = get_auto_pq(&auto_pq);
    if(ret < 0){
        return ret;
    }else {
        memcpy(data, &(auto_pq.gamma[index]) ,sizeof(gamma_lut_data));
        return 0;
    }
}

int AutoPQ::set_auto_gamma(unsigned short index, struct gamma_lut_data *data) {
    struct auto_pq_info auto_pq;
    int ret = get_auto_pq(&auto_pq);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(auto_pq.gamma[index]), data, sizeof(gamma_lut_data));
        return set_auto_pq(&auto_pq);
    }
}

int AutoPQ::get_auto_3d_lut(unsigned short index, struct cubic_lut_data *data) {
    struct auto_pq_info auto_pq;
    int ret = get_auto_pq(&auto_pq);
    if(ret < 0){
        return ret;
    }else {
        memcpy(data, &(auto_pq.cubic[index]) ,sizeof(cubic_lut_data));
        return 0;
    }
}

int AutoPQ::set_auto_3d_lut(unsigned short index, struct cubic_lut_data *data) {
    struct auto_pq_info auto_pq;
    int ret = get_auto_pq(&auto_pq);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(auto_pq.cubic[index]), data, sizeof(cubic_lut_data));
        return set_auto_pq(&auto_pq);
    }
}
}
