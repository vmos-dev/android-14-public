#define LOG_TAG "bes_mmap_write"
#include <utils/Log.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <linux/ashmem.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stddef.h>
#include <linux/ipc.h>
#include <linux/shm.h>

#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <binder/Parcel.h>
#include <binder/IInterface.h>
#include "bes_mmap.h"

using namespace android;
#include "bes_mmap_utils.h"

extern "C" int bes_mmap_write_init(bes_mmap_context_st* context)
{
    Parcel data, reply;
    int ret = -1;

    bes_mmap_ibinder_st * mmap_write_binder = NULL;
    
    if(!context || (strlen((const char *)context->mmap_path) <= 0) \
        || (strlen((const char *)context->mmap_name) <= 0)){
        ALOGE("%s failed", __func__);
        return -1;
    }

    context->fd = open((const char*)context->mmap_path, O_RDWR);
    if( context->fd < 0){
        ALOGE("%s create mmap:%s failed err:%d", __func__,context->mmap_path,  context->fd);
        return -1;
    }
    
    ret = ioctl( context->fd, ASHMEM_SET_NAME, context->mmap_name);
    if(ret < 0){
        ALOGE("%s ASHMEM_SET_NAME failed err:%d", __func__, ret);
        goto exit;
    }

    ret = ioctl( context->fd, ASHMEM_SET_SIZE, MAXBUFSIZE);
    if(ret < 0){
        ALOGE("%s ASHMEM_SET_SIZE failed err:%d", __func__, ret);
        goto exit;
    }

    context->mmap_address = (unsigned char*)mmap(NULL, MAXBUFSIZE , PROT_READ | PROT_WRITE, MAP_SHARED,  context->fd, 0);
    if(context->mmap_address == NULL)
    {
        ALOGE("%s create mmap failed", __func__);
        goto exit;
    }

    mmap_write_binder = (bes_mmap_ibinder_st*)context->reserved;
    mmap_write_binder->binder = defaultServiceManager()->checkService(String16((const char *)context->mmap_name));
    if(!mmap_write_binder->binder){
        munmap((void*)context->mmap_address, MAXBUFSIZE);
        ALOGE("%s checkService failed", __func__);
        goto exit;
    }
    
    data.writeDupFileDescriptor( context->fd);
    mmap_write_binder->binder->transact(MMAP_TRANSFD_OP, data, &reply);
    return 0;
    
exit:
    close(context->fd);
    context->fd = -1;
    return -1;
}

extern "C" int bes_mmap_write_start(bes_mmap_context_st* context, unsigned char * ram_data, unsigned int length)
{
    Parcel data, reply;
    bes_mmap_ibinder_st * mmap_write_binder = NULL;
    
    if(!context || !context->mmap_address || (length > MAXBUFSIZE)){
        ALOGE("%s not init", __func__);
        return -1;
    }

    mmap_write_binder = (bes_mmap_ibinder_st*)context->reserved;
    if(!mmap_write_binder->binder){
        ALOGE("%s service not found", __func__);
        return -1;
    }
    
    if(ram_data){
        data.writeInt32(length);
        memcpy(context->mmap_address, ram_data, length);
    }else{
        ALOGE("%s ram_data == NULL", __func__);
        return -1;
    }

    
    mmap_write_binder->binder->transact(MMAP_WRITEDATA_OP, data, &reply);
    return 0;
}

extern "C" void bes_mmap_write_close(bes_mmap_context_st* context)
{
    if(!context)
        return;

    if(context->mmap_address){
        munmap((void*)context->mmap_address, MAXBUFSIZE);
        context->mmap_address = NULL;
    }

    if(context->fd >= 0){
        close(context->fd);
        context->fd = -1;
    }

    return;
}

