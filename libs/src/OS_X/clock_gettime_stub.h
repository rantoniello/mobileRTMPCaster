/*
 * clock_gettime_stub.h
 */

#ifndef CLOCK_GETTIME_STUB_OS_X_H_
#define CLOCK_GETTIME_STUB_OS_X_H_

#ifdef __APPLE__

typedef enum {
        CLOCK_REALTIME,
        CLOCK_MONOTONIC,
        CLOCK_PROCESS_CPUTIME_ID,
        CLOCK_THREAD_CPUTIME_ID
} clockid_t;

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif

#endif /* CLOCK_GETTIME_STUB_OS_X_H_ */

