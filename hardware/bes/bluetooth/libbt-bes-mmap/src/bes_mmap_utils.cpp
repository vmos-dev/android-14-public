#define LOG_TAG "bes_mmap_utils"
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

extern "C" int bes_mmap_set_path(bes_mmap_context_st * context,unsigned char * str, unsigned short length)
{
    if(!context || (length > (sizeof(context->mmap_path) - 1))){
        ALOGE("%s invalid length:%d", __func__, length);
        return -1;
    }

    memcpy(context->mmap_path, str, length);
    return 0;
}

extern "C" unsigned char* bes_mmap_get_path(bes_mmap_context_st* context)
{
    if(!context)
        return NULL;
    
    return context->mmap_path;
}

extern "C" int bes_mmap_set_name(bes_mmap_context_st* context, unsigned char * str, unsigned short length)
{
    if(!context || (length > (sizeof(context->mmap_name) - 1))){
        ALOGE("%s invalid length:%d", __func__, length);
        return -1;
    }

    memcpy(context->mmap_name, str, length);
    return 0;
}

extern "C" unsigned char* bes_mmap_get_name(bes_mmap_context_st* context)
{
    if(!context)
        return NULL;
    
    return context->mmap_name;
}


