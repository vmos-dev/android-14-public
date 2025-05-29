#include <string.h>
#include <errno.h>
#include "bes_utils.h"

#define LOG_TAG "Os_Timer"
#include <utils/Log.h>

timer_t OsAllocateTimer(tTIMER_HANDLE_CBACK timer_callback, void *data)
{
    struct sigevent sigev;
    timer_t timerid;

    memset(&sigev, 0, sizeof(struct sigevent));
    // Create the POSIX timer to generate signo
    sigev.sigev_notify = SIGEV_THREAD;
    //sigev.sigev_notify_thread_id = syscall(__NR_gettid);
    sigev.sigev_notify_function = timer_callback;
    if(!data)
        sigev.sigev_value.sival_ptr = &timerid;
    else
        sigev.sigev_value.sival_ptr = data;

    //Create the Timer using timer_create signal

    if (timer_create(CLOCK_REALTIME, &sigev, &timerid) == 0)
    {
        return timerid;
    }
    else
    {
        ALOGE("timer_create error!");
        return (timer_t)-1;
    }
}

int OsFreeTimer(timer_t timerid)
{
    int ret = 0;
    ret = timer_delete(timerid);
    if(ret != 0)
        ALOGE("timer_delete fail with errno(%d)", errno);

    return ret;
}


int OsStartTimer(timer_t timerid, int msec, int mode)
{
    struct itimerspec itval;

    itval.it_value.tv_sec = msec / 1000;
    itval.it_value.tv_nsec = (long)(msec % 1000) * (1000000L);

    if (mode == 1)

    {
        itval.it_interval.tv_sec    = itval.it_value.tv_sec;
        itval.it_interval.tv_nsec = itval.it_value.tv_nsec;
    }
    else
    {
        itval.it_interval.tv_sec = 0;
        itval.it_interval.tv_nsec = 0;
    }

    //Set the Timer when to expire through timer_settime

    if (timer_settime(timerid, 0, &itval, NULL) != 0)
    {
        ALOGE("time_settime error!");
        return -1;
    }

    return 0;

}

int OsStopTimer(timer_t timerid)
{
    return OsStartTimer(timerid, 0, 0);
}

//input ms
void OsTimerDelay (uint32_t timeout)
{
    struct timespec delay;
    int err;

    if (timeout == 0)
        return;

    delay.tv_sec = timeout / 1000;
    delay.tv_nsec = 1000 * 1000 * (timeout%1000);

    /* [u]sleep can't be used because it uses SIGALRM */
    do {
        err = nanosleep(&delay, &delay);
    } while (err < 0 && errno ==EINTR);
}

