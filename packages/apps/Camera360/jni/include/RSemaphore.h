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
#ifndef _RSEMAPHORE_H_
#define _RSEMAPHORE_H_

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>

namespace android {

class RSemaphore
{
public:

    RSemaphore();
    ~RSemaphore();

    //Release semaphore
    int Release();

    ///Create the semaphore with initial count value
    int Create(int count=0);

    ///Wait operation
    int Wait();

    ///Signal operation
    int Signal();

    ///Current semaphore count
    int Count();

    ///Wait operation with a timeout
    int WaitTimeout(int timeoutMicroSecs);

private:
    sem_t *mSemaphore;

};

};

#endif