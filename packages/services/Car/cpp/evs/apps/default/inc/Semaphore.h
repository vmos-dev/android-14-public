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



#include <semaphore.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utils/Errors.h>

namespace android {

class Semaphore
{
public:

    Semaphore();
    ~Semaphore();

    //Release semaphore
    status_t Release();

    ///Create the semaphore with initial count value
    status_t Create(int count=0);

    ///Wait operation
    status_t Wait();

    ///Signal operation
    status_t Signal();

    ///Current semaphore count
    int Count();

    ///Wait operation with a timeout
    status_t WaitTimeout(int timeoutMicroSecs);

private:
    sem_t *mSemaphore;

};

};
