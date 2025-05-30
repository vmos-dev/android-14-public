/*
 *
 * Copyright 2021 Rockchip Electronics S.LSI Co. LTD
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

#include "baseparameter_api.h"

#define BASEPARAMETER_IMAGE_SIZE 1024*1024
#define BACKUP_OFFSET 512*1024
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "baseparameter_api", __VA_ARGS__)

static const unsigned int crc32_table[] =
{
 0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
 0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
 0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
 0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
 0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
 0x14015c4fL, 0x63066cd9L, 0xfa0f3d63L, 0x8d080df5L,
 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
 0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
 0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
 0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
 0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
 0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
 0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
 0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
 0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
 0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
 0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
 0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
 0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
 0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
 0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
 0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
 0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
 0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
 0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
 0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
 0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
 0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
 0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
 0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
 0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
 0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
 0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
 0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
 0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
 0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
 0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
 0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
 0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
 0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
 0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
 0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
 0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

pthread_rwlock_t rwlock;

baseparameter_api::baseparameter_api() {
    pthread_rwlock_init(&rwlock, NULL);
}

baseparameter_api::~baseparameter_api() {
    pthread_rwlock_destroy(&rwlock);
}

bool baseparameter_api::have_baseparameter() {
    if(get_baseparameter_file() == NULL){
        return false;
    }else {
        return true;
    }
}

int baseparameter_api::dump_baseparameter(const char *file_path) {
    int file;
    int ret;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_rdlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    char *data = (char *)malloc(BASEPARAMETER_IMAGE_SIZE);
    lseek(file, 0L, SEEK_SET);
    ret = read(file, data, BASEPARAMETER_IMAGE_SIZE);
    if (ret < 0) {
        LOGD("fail to read");
        close(file);
        free(data);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    close(file);
    pthread_rwlock_unlock(&rwlock);
    file = open(file_path, O_CREAT | O_WRONLY, 0666);
    if (file < 0) {
        LOGD("fail to open");
        free(data);
        return -EIO;
    }
    lseek(file, BASEPARAMETER_IMAGE_SIZE-1, SEEK_SET);
    ret = write(file, "\0", 1);
    if (ret < 0) {
        LOGD("fail to write");
        close(file);
        free(data);
        return -EIO;
    }
    lseek(file, 0L, SEEK_SET);
    ret = write(file, (char*)data, BASEPARAMETER_IMAGE_SIZE);
    fsync(file);
    close(file);
    free(data);
    LOGD("dump_baseparameter %s success\n", file_path);

    return 0;
}

const char* baseparameter_api::get_baseparameter_file() {
    int i = 0;
    while (device_template[i]) {
        if (!access(device_template[i], R_OK | W_OK))
            return device_template[i];
        i++;
    }
    return NULL;
}

int baseparameter_api::get_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info) {
    struct disp_header headers[8];
    int file, ret, i;
    bool found = false;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_rdlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }

    lseek(file, 8, SEEK_SET);
    ret = read(file, &headers, sizeof(disp_header) * 8);
    if(ret < 0){
        sync();
        close(file);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    for(i = 0; i < 8; i++) {
        if(connector_id == headers[i].connector_id && connector_type == headers[i].connector_type){
            found = true;
            break;
        }
    }
    if(found){
        lseek(file, headers[i].offset, SEEK_SET);
        ret = read(file, info, sizeof(disp_info));
        if(ret < 0){
            pthread_rwlock_unlock(&rwlock);
            return -EIO;
        }
        u32 crc = get_crc32((unsigned char *)info, sizeof(disp_info) - sizeof(u32));
        if(crc != info->crc){
            LOGD("crc32 error");
            pthread_rwlock_unlock(&rwlock);
            return -EPERM;
        }
    } else {
        LOGD("no connector_type and connector_id found");
        pthread_rwlock_unlock(&rwlock);
        return -EINVAL;
    }
    sync();
    close(file);
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

int baseparameter_api::set_disp_info(unsigned int connector_type, unsigned int connector_id, struct disp_info *info) {
    struct disp_header headers[8];
    int file, ret, i;
    bool found = false;
    u32 crc32 = get_crc32((unsigned char *)info, sizeof(disp_info) - sizeof(u32));
    info->crc = crc32;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_wrlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }

    lseek(file, 8, SEEK_SET);
    ret = read(file, &headers, sizeof(disp_header) * 8);
    if(ret < 0){
        LOGD("fail to read");
        sync();
        close(file);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }

    for(i = 0; i < 8; i++) {
        if(connector_id == headers[i].connector_id && connector_type == headers[i].connector_type){
            found = true;
            break;
        }
    }
    if(found){
        lseek(file, headers[i].offset, SEEK_SET);
        ret = write(file, (char*)(info), sizeof(disp_info));
        if (ret < 0) {
             LOGD("fail to write");
             sync();
             close(file);
             pthread_rwlock_unlock(&rwlock);
             return -EIO;
        }
    }else {
        LOGD("no connector_type and connector_id found");
        pthread_rwlock_unlock(&rwlock);
        return -EINVAL;
    }
    fsync(file);
    close(file);
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

unsigned short baseparameter_api::get_brightness(unsigned int connector_type, unsigned int connector_id) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return DEFAULT_BRIGHTNESS;
    }else {
        return info.bcsh_info.brightness;
    }
}

unsigned short baseparameter_api::get_contrast(unsigned int connector_type, unsigned int connector_id) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return DEFAULT_CONTRAST;
    }else {
        return info.bcsh_info.contrast;
    }
}

unsigned short baseparameter_api::get_saturation(unsigned int connector_type, unsigned int connector_id) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return DEFAULT_SATURATION;
    }else {
        return info.bcsh_info.saturation;
    }
}

unsigned short baseparameter_api::get_hue(unsigned int connector_type, unsigned int connector_id) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return DEFAULT_HUE;
    }else {
        return info.bcsh_info.hue;
    }
}

int baseparameter_api::set_brightness(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return ret;
    }else {
        info.bcsh_info.brightness = value;
        return set_disp_info(connector_type, connector_id, &info);
    }
}

int baseparameter_api::set_contrast(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return ret;
    }else {
        info.bcsh_info.contrast = value;
        return set_disp_info(connector_type, connector_id, &info);
    }
}

int baseparameter_api::set_saturation(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return ret;
    }else {
        info.bcsh_info.saturation = value;
        return set_disp_info(connector_type, connector_id, &info);
    }
}

int baseparameter_api::set_hue(unsigned int connector_type, unsigned int connector_id, unsigned short value) {
    struct disp_info info;
    int ret = get_disp_info(connector_type, connector_id, &info);
    if(ret < 0){
        return ret;
    }else {
        info.bcsh_info.hue = value;
        return set_disp_info(connector_type, connector_id, &info);
    }
}

int baseparameter_api::get_screen_info(unsigned int connector_type, unsigned int connector_id, int index, struct screen_info *info) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(info, &(disp.screen_info[index]) ,sizeof(screen_info));
        return 0;
    }
}

int baseparameter_api::set_screen_info(unsigned int connector_type, unsigned int connector_id, int index, screen_info *info) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(disp.screen_info[index]), info, sizeof(screen_info));
        return set_disp_info(connector_type, connector_id, &disp);
    }
}

int baseparameter_api::get_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *info) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(info, &(disp.overscan_info) ,sizeof(overscan_info));
        return 0;
    }
}

int baseparameter_api::set_overscan_info(unsigned int connector_type, unsigned int connector_id, struct overscan_info *info) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(disp.overscan_info), info, sizeof(overscan_info));
        return set_disp_info(connector_type, connector_id, &disp);
    }
}

int baseparameter_api::get_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(data, &(disp.gamma_lut_data) ,sizeof(gamma_lut_data));
        return 0;
    }
}
int baseparameter_api::set_gamma_lut_data(unsigned int connector_type, unsigned int connector_id, struct gamma_lut_data *data) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(disp.gamma_lut_data), data, sizeof(gamma_lut_data));
        return set_disp_info(connector_type, connector_id, &disp);
    }
}

int baseparameter_api::get_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(data, &(disp.cubic_lut_data) ,sizeof(cubic_lut_data));
        return 0;
    }
}
int baseparameter_api::set_cubic_lut_data(unsigned int connector_type, unsigned int connector_id, struct cubic_lut_data *data) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(disp.cubic_lut_data), data, sizeof(cubic_lut_data));
        return set_disp_info(connector_type, connector_id, &disp);
    }
}

int baseparameter_api::set_disp_header(unsigned int index, unsigned int connector_type, unsigned int connector_id) {
    struct disp_header header;
    int ret;
    int file;
    uint64_t offset;

    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_wrlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("baseparamter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }

    offset = 8 + index * sizeof(disp_header);
    lseek(file, offset, SEEK_SET);
    read(file, &header, sizeof(disp_header));
    header.connector_id = connector_id;
    header.connector_type = connector_type;
    lseek(file, offset, SEEK_SET);
    ret = write(file, (char*)&(header), sizeof(disp_header));
    if (ret < 0) {
        LOGD("fail to write");
        close(file);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    fsync(file);
    close(file);
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

bool baseparameter_api::validate() {
    int file;
    char head_flag[5];
    memset(head_flag, 0, 5);
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return false;
    }
    pthread_rwlock_rdlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("baseparamter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return false;
    }
    lseek(file, 0, SEEK_SET);
    read(file, head_flag, 4);
    sync();
    close(file);
    pthread_rwlock_unlock(&rwlock);
    LOGD("validate head_flag %s", head_flag);
    if (memcmp(head_flag, "BASP", 4) == 0) { 
        return true;
    } else {
        return false;
    }
}

u32 baseparameter_api::get_crc32(unsigned char *buf, unsigned int size) {
    u32 i, crc;
    crc = 0xFFFFFFFF;
    for (i = 0; i < size; i++)
        crc = crc32_table[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);

    return crc^0xFFFFFFFF;
}

int baseparameter_api::get_framebuffer_info(unsigned int connector_type, unsigned int connector_id, framebuffer_info *info) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(info, &(disp.framebuffer_info) ,sizeof(framebuffer_info));
        return 0;
    }
}

int baseparameter_api::set_framebuffer_info(unsigned int connector_type, unsigned int connector_id, framebuffer_info *info) {
    struct disp_info disp;
    int ret = get_disp_info(connector_type, connector_id, &disp);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&(disp.framebuffer_info), info, sizeof(framebuffer_info));
        return set_disp_info(connector_type, connector_id, &disp);
    }
}

int baseparameter_api::get_all_disp_header(struct disp_header *headers) {
    int file, ret;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_rdlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }

    lseek(file, 8, SEEK_SET);
    ret = read(file, headers, sizeof(disp_header) * 8);
    if(ret < 0){
        sync();
        close(file);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    sync();
    close(file);
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

int baseparameter_api::get_baseparameter_info(unsigned int index, baseparameter_info *info) {
    int file, ret, offset;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_rdlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    if(index == BASE_PARAMETER){
        offset = 0;
    } else {
        offset = BACKUP_OFFSET;
    }
    lseek(file, offset, SEEK_SET);
    ret = read(file, info, sizeof(baseparameter_info));
    if(ret < 0){
        sync();
        close(file);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    sync();
    close(file);
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

int baseparameter_api::set_baseparameter_info(unsigned int index, baseparameter_info *info) {
    int file, ret, offset;
    const char *baseparameterfile = get_baseparameter_file();
    if (!baseparameterfile) {
        sync();
        return -ENOENT;
    }
    pthread_rwlock_wrlock(&rwlock);
    file = open(baseparameterfile, O_RDWR);
    if (file < 0) {
        LOGD("base paramter file can not be opened \n");
        sync();
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    if(index == BASE_PARAMETER){
        offset = 0;
     } else {
        offset = BACKUP_OFFSET;
    }
    lseek(file, offset, SEEK_SET);
    ret = write(file, (char*)(info), sizeof(baseparameter_info));
    if (ret < 0) {
        LOGD("fail to write");
        sync();
        close(file);
        pthread_rwlock_unlock(&rwlock);
        return -EIO;
    }
    fsync(file);
    close(file);
    pthread_rwlock_unlock(&rwlock);
    return 0;
}

int baseparameter_api::set_pq_tuning_info(struct pq_tuning_info *info) {
    int ret;
    u32 crc32 = get_crc32((unsigned char *)info, sizeof(pq_tuning_info) - sizeof(u32));
    info->crc = crc32;
    baseparameter_info base;
    ret = get_baseparameter_info(0, &base);
    if(ret != 0) {
        return ret;
    }
    memcpy(&base.pq_tuning_info, info, sizeof(pq_tuning_info));
    ret = set_baseparameter_info(0, &base);
    return ret;
}

int baseparameter_api::get_pq_tuning_info(struct pq_tuning_info *info) {
    baseparameter_info base;
    int ret = get_baseparameter_info(0, &base);
    if (ret == 0) {
        memcpy(info, &base.pq_tuning_info, sizeof(pq_tuning_info));
    }
    return ret;
}

int baseparameter_api::get_csc_info(struct csc_info *csc) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(csc, &param.csc ,sizeof(csc_info));
        return 0;
    }
}

int baseparameter_api::set_csc_info(struct csc_info *csc) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&param.csc, csc, sizeof(csc_info));
        return set_pq_tuning_info(&param);
    }
}

int baseparameter_api::get_dci_info(struct dci_info *dci) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(dci, &param.dci ,sizeof(dci_info));
        return 0;
    }
}

int baseparameter_api::set_dci_info(struct dci_info *dci) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&param.dci, dci, sizeof(dci_info));
        return set_pq_tuning_info(&param);
    }
}

int baseparameter_api::get_acm_info(struct acm_info *acm) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(acm, &param.acm ,sizeof(acm_info));
        return 0;
    }
}

int baseparameter_api::set_acm_info(struct acm_info *acm) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&param.acm, acm, sizeof(acm_info));
        return set_pq_tuning_info(&param);
    }
}

int baseparameter_api::get_pq_tuning_gamma(struct gamma_lut_data *data) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(data, &param.gamma ,sizeof(gamma_lut_data));
        return 0;
    }
}

int baseparameter_api::set_pq_tuning_gamma(struct gamma_lut_data *data) {
    struct pq_tuning_info param;
    int ret = get_pq_tuning_info(&param);
    if(ret < 0){
        return ret;
    }else {
        memcpy(&param.gamma, data, sizeof(gamma_lut_data));
        return set_pq_tuning_info(&param);
    }
}

int baseparameter_api::get_pq_factory_info(struct pq_factory_info *info) {
    baseparameter_info base;
    int ret = get_baseparameter_info(0, &base);
    if (ret == 0) {
        memcpy(info, &base.pq_factory_info, sizeof(pq_factory_info));
    }
    return ret;
}

int baseparameter_api::set_pq_factory_info(struct pq_factory_info *info) {
    int ret;
    u32 crc32 = get_crc32((unsigned char *)info, sizeof(pq_factory_info) - sizeof(u32));
    info->crc = crc32;
    baseparameter_info base;
    ret = get_baseparameter_info(0, &base);
    if(ret != 0) {
        return ret;
    }
    memcpy(&base.pq_factory_info, info, sizeof(pq_factory_info));
    ret = set_baseparameter_info(0, &base);
    return ret;
}

int baseparameter_api::get_pq_sharp_info(struct pq_sharp_info *info) {
    baseparameter_info base;
    int ret = get_baseparameter_info(0, &base);
    if (ret == 0) {
        memcpy(info, &base.pq_sharp_info, sizeof(pq_sharp_info));
    }
    return ret;
}

int baseparameter_api::set_pq_sharp_info(struct pq_sharp_info *info) {
    int ret;
    u32 crc32 = get_crc32((unsigned char *)info, sizeof(pq_sharp_info) - sizeof(u32));
    info->crc = crc32;
    baseparameter_info base;
    ret = get_baseparameter_info(0, &base);
    if(ret != 0) {
        return ret;
    }
    memcpy(&base.pq_sharp_info, info, sizeof(pq_sharp_info));
    ret = set_baseparameter_info(0, &base);
    return ret;
}

int baseparameter_api::get_version(unsigned short* major_version, unsigned short* minor_version) {
    baseparameter_info info;
    int ret = get_baseparameter_info(0, &info);
    *major_version = info.major_version;
    *minor_version = info.minor_version;
    return ret;
}

int baseparameter_api::get_aipq_info(struct aipq_info *info) {
    baseparameter_info base;
    int ret = get_baseparameter_info(0, &base);
    if (ret == 0) {
        memcpy(info, &base.aipq_info, sizeof(aipq_info));
    }
    return ret;
}

int baseparameter_api::set_aipq_info(struct aipq_info *info) {
    int ret;
    u32 crc32 = get_crc32((unsigned char *)info, sizeof(aipq_info) - sizeof(u32));
    info->crc = crc32;
    baseparameter_info base;
    ret = get_baseparameter_info(0, &base);
    if(ret != 0) {
        return ret;
    }
    memcpy(&base.aipq_info, info, sizeof(aipq_info));
    ret = set_baseparameter_info(0, &base);
    return ret;
}
