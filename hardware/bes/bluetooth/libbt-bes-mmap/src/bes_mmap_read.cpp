#define LOG_TAG "bes_mmap_read"
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

class MMapBinderService : public BBinder{
private:
    bes_mmap_context_st * m_context = NULL;

public:
    MMapBinderService(bes_mmap_context_st* mmap_conext)
    {
        m_context = mmap_conext;
    }
    
    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
    {
        int ret = -1;
        int get_mmap_file_size = 0;

        if(!m_context){
            ALOGE("m_context==NULL");
            return NO_ERROR;
        }

        switch(code)
        {
            case MMAP_TRANSFD_OP:
                m_context->fd = data.readFileDescriptor();
                m_context->mmap_file_max_size = ioctl(m_context->fd, ASHMEM_GET_SIZE,NULL);
                ALOGD("onTransact fd:%d size:%d", m_context->fd, m_context->mmap_file_max_size);
            break;

            case MMAP_WRITEDATA_OP:
                get_mmap_file_size = data.readInt32();
                if(get_mmap_file_size > 0)
                { 
                    m_context->mmap_address = (unsigned char*)mmap(NULL, m_context->mmap_file_max_size,
                                                                    PROT_READ | PROT_WRITE, MAP_SHARED, m_context->fd, 0);
                    if(m_context->readCb)
                        m_context->readCb(m_context->mmap_address, get_mmap_file_size);
                }
                else
                {
                    ALOGE("mmap failed %d %d\n", m_context->mmap_file_max_size , get_mmap_file_size);
                    return -1;
                }
                ALOGD("get the share memory: %p\n", m_context->mmap_address);
            break;

            default:
            break;
        }
        return NO_ERROR;
    }
};

extern "C" void bes_mmap_read_register_cb(bes_mmap_context_st* context, t_bes_mmap_read_cb func)
{
    context->readCb = func;
}

static void* bes_mmap_read_service_thread_handler(void *arg)
{
    bes_mmap_context_st* context = (bes_mmap_context_st*)arg;
    if(!context || (strlen((const char *)context->mmap_name) <= 0)){
       ALOGE("%s failed", __func__);
       return NULL;
    }

    ALOGD("%s start create read service:%s", __func__, context->mmap_name);
    if(!context->service_started){
       context->service_started = true;
       defaultServiceManager()->addService(String16((const char *)context->mmap_name), new MMapBinderService(context));
       sp<ProcessState> proc(ProcessState::self());
       ProcessState::self()->startThreadPool();
       IPCThreadState::self()->joinThreadPool();
    }

    ALOGD("start create read service:%s success", context->mmap_name);
    return NULL;
}

extern "C" int bes_mmap_read_init(bes_mmap_context_st* context)
{
    if(!context->thread_id){
        pthread_attr_t thread_attr;
        pthread_attr_init(&thread_attr);
        pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

        if(pthread_create((pthread_t*)(&context->thread_id), &thread_attr, bes_mmap_read_service_thread_handler, context)!= 0 )
        {
            ALOGE("pthread_create : %s", strerror(errno));
        }
    }
    ALOGD("%s start create read service thread", __func__);
    return 0;
}

extern "C" int bes_mmap_read_close(bes_mmap_context_st* context)
{
    if(!context->thread_id){
        pthread_join((pthread_t)context->thread_id, NULL);
        context->thread_id = 0;
    }
    return 0;
}

