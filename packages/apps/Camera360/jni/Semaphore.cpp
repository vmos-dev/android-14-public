/******************************************************************************
 *
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co., Ltd.
 * Modification based on code covered by the License (the "License").
 * You may not use this software except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED TO YOU ON AN "AS IS" BASIS and ROCKCHIP DISCLAIMS 
 * ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, 
 * WHETHER EXPRESS,IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION,
 * ANY IMPLIED WARRANTIES OF TITLE, NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY
 * QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR PURPOSE. 
 * Rockchip shall not be liable to make any corrections to this software or to 
 * provide any support or assistance with respect to it.
 *
 *****************************************************************************/



#include "RSemaphore.h"
#include <time.h>
#include <utils/Log.h>
namespace android {

/**
   @brief Constructor for the semaphore class

   @param none
   @return none
 */
RSemaphore::RSemaphore()
{
    ///Initialize the semaphore to NULL
    mSemaphore = NULL;
}

/**
   @brief Destructor of the semaphore class

   @param none
   @return none

 */
RSemaphore::~RSemaphore()
{
    Release();
}

/**
   @brief: Releases semaphore

   @param count >=0
   @return NO_ERROR On Success
   @return One of the android error codes based on semaphore de-initialization
 */

int RSemaphore::Release()
{
    int status = 0;

    ///Destroy only if the semaphore has been created
    if(mSemaphore)
    {
        status = sem_destroy(mSemaphore);

        free(mSemaphore);

        mSemaphore = NULL;
    }

    ///Initialize the semaphore and return the status
    return (status);

}

/**
   @brief Create the semaphore with initial count value

   @param count >=0
   @return NO_ERROR On Success
   @return NO_MEMORY If unable to allocate memory for the RSemaphore
   @return -1 If an invalid count value is passed (<0)
   @return One of the android error codes based on RSemaphore initialization
 */

int RSemaphore::Create(int count)
{
    int ret = 0;

    ///count cannot be less than zero
    if(count<0)
    {
        return -1;
	}

    ret = Release();
    if ( 0 != ret )
    {
        return ret;
    }

    ///allocate memory for the semaphore
    mSemaphore = (sem_t*)malloc(sizeof(sem_t)) ;

    ///if memory is unavailable, return error
    if(!mSemaphore)
    {
        printf("%s(%d):failed to alloc mem",__FUNCTION__,__LINE__);
        return -1;
    }

    ///Initialize the semaphore and return the status
    return sem_init(mSemaphore, 0x00, count);

}

/**
   @brief Wait operation

   @param none
   @return -1 if the semaphore is not initialized
   @return NO_ERROR On success
   @return One of the android error codes based on semaphore wait operation
 */
int RSemaphore::Wait()
{
    ///semaphore should have been created first
    if(!mSemaphore)
    {
        return -1;
    }

    ///Wait and return the status after signalling
    return sem_wait(mSemaphore);


}


/**
   @brief Signal operation

   @param none
     @return -1 if the semaphore is not initialized
     @return NO_ERROR On success
     @return One of the android error codes based on semaphore signal operation
   */

int RSemaphore::Signal()
{
    ///semaphore should have been created first
    if(!mSemaphore)
    {
        return -1;
    }

    ///Post to the semaphore
    return sem_post(mSemaphore);

}

/**
   @brief Current semaphore count

   @param none
   @return Current count value of the semaphore
 */
int RSemaphore::Count()
{
    int val;

    ///semaphore should have been created first
    if(!mSemaphore)
    {
        return -1;
    }

    ///get the value of the semaphore
    sem_getvalue(mSemaphore, &val);

    return val;
}

/**
   @brief Wait operation with a timeout

     @param timeoutMicroSecs The timeout period in micro seconds
     @return -1(-1) if the semaphore is not initialized
     @return NO_ERROR(0) On success
     @return One of the android error codes based on semaphore wait operation
   */

int RSemaphore::WaitTimeout(int timeoutMicroSecs)
{
    int ret = 0;

    struct timespec timeSpec;
    struct timeval currentTime;

    ///semaphore should have been created first
    if( NULL == mSemaphore)
    {
        ret = -1;
    }

    if ( 0 == ret )
    {

        ///setup the timeout values - timeout is specified in seconds and nanoseconds
        gettimeofday(&currentTime, NULL);
        timeSpec.tv_sec = currentTime.tv_sec;
        timeSpec.tv_nsec = currentTime.tv_usec * 1000;
        timeSpec.tv_sec += ( timeoutMicroSecs / 1000000 );
        timeSpec.tv_nsec += ( timeoutMicroSecs % 1000000) * 1000;

        ///Wait for the timeout or signal and return the result based on whichever event occurred first
        ret = sem_timedwait(mSemaphore, &timeSpec);
    }

    if ( 0 != ret )
    {
        Signal();
        Create(0);
    }

    return ret;
}


};


