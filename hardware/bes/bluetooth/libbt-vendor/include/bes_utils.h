#ifndef __BES_UTILS_H__
#define __BES_UTILS_H__
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

typedef void (*tTIMER_HANDLE_CBACK)(union sigval sigval_value);

timer_t OsAllocateTimer(tTIMER_HANDLE_CBACK timer_callback, void *data);
int OsFreeTimer(timer_t timerid);
int OsStartTimer(timer_t timerid, int msec, int mode);
int OsStopTimer(timer_t timerid);
void OsTimerDelay (uint32_t timeout);

static inline void
set_bit(unsigned long nr, volatile void * addr)
{
	int *m = ((int *) addr) + (nr >> 5);

	*m |= 1 << (nr & 31);
}

static inline void
clear_bit(unsigned long nr, volatile void * addr)
{
	int *m = ((int *) addr) + (nr >> 5);

	*m &= ~(1 << (nr & 31));
}

static inline int
test_bit(unsigned long nr, volatile void * addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	int *m = ((int *) addr) + (nr >> 5);

	return ((*m) & mask) != 0;
}

static inline int
test_and_set_bit(unsigned long nr, volatile void * addr)
{
    unsigned long mask = 1 << (nr & 0x1f);
	int *m = ((int *) addr) + (nr >> 5);
	int old = *m;

	*m = old | mask;
	return (old & mask) != 0;
}

static inline int
test_and_clear_bit(unsigned long nr, volatile void * addr)
{
	unsigned long mask = 1 << (nr & 0x1f);
	int *m = ((int *) addr) + (nr >> 5);
	int old = *m;

	*m = old & ~mask;
	return (old & mask) != 0;
}

#endif

