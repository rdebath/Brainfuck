
#include <unistd.h>
#include <time.h>

#ifdef _WIN32
#define USE_WINDOWS_CLOCKS
#elif defined(_POSIX_MONOTONIC_CLOCK) && defined(_POSIX_TIMERS) && defined(CLOCK_MONOTONIC)
/* Old GCC can't deal with undefined vars in a preproc expression. */
# if _POSIX_TIMERS>0
#  ifndef __GLIBC__
#   define USE_POSIX_TIMERS
#  elif __GLIBC__>2 || (__GLIBC__==2 && __GLIBC_MINOR__>=17)
/* Old Glibc needs -lrt so don't bother. */
#   define USE_POSIX_TIMERS
#  endif
# endif
#endif

#include "clock.h"

#if defined(USE_POSIX_TIMERS)

static struct timespec run_start, paused, run_pause;

void
start_runclock(void)
{
    clock_gettime(CLOCK_MONOTONIC, &run_start);
    paused.tv_sec = 0;
    paused.tv_nsec = 0;
}

void
finish_runclock(double * prun_time, double *pwait_time)
{
    struct timespec run_end;
    clock_gettime(CLOCK_MONOTONIC, &run_end);

    run_end.tv_sec -= run_start.tv_sec;
    run_end.tv_nsec -= run_start.tv_nsec;
    if (run_end.tv_nsec < 0)
        { run_end.tv_nsec += 1000000000; run_end.tv_sec -= 1; }

    run_end.tv_sec -= paused.tv_sec;
    run_end.tv_nsec -= paused.tv_nsec;
    if (run_end.tv_nsec < 0)
        { run_end.tv_nsec += 1000000000; run_end.tv_sec -= 1; }

    if (prun_time)
	*prun_time = (run_end.tv_sec + run_end.tv_nsec/1000000000.0);
    if (pwait_time)
	*pwait_time = (paused.tv_sec + paused.tv_nsec/1000000000.0);
}

void
pause_runclock(void)
{
    clock_gettime(CLOCK_MONOTONIC, &run_pause);
}

void
unpause_runclock(void)
{
    struct timespec run_end;

    clock_gettime(CLOCK_MONOTONIC, &run_end);
    paused.tv_sec += run_end.tv_sec - run_pause.tv_sec;
    paused.tv_nsec += run_end.tv_nsec - run_pause.tv_nsec;

    if (paused.tv_nsec < 0)
        { paused.tv_nsec += 1000000000; paused.tv_sec -= 1; }
    if (paused.tv_nsec >= 1000000000)
        { paused.tv_nsec -= 1000000000; paused.tv_sec += 1; }
}

#elif defined(_WIN32)
#include <windows.h>

/*******************************************************************************
 * Adapted from:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/dn553408%28v=vs.85%29.aspx
 *
 * SEE ALSO: http://mxr.mozilla.org/mozilla-central/source/xpcom/ds/TimeStamp_windows.cpp
 */

static LARGE_INTEGER Frequency;
static LARGE_INTEGER Run_Start;
static LARGE_INTEGER Paused;
static LARGE_INTEGER Run_Pause;
static int failed = 0;

void
start_runclock(void)
{
    Paused.QuadPart = 0;
    failed = !QueryPerformanceFrequency(&Frequency);
    if (Frequency.QuadPart == 0) failed = 1;
    if (!failed)
	QueryPerformanceCounter(&Run_Start);
}

void
finish_runclock(double * prun_time, double *pwait_time)
{
    LARGE_INTEGER EndingTime;
    if(failed) return;

    QueryPerformanceCounter(&EndingTime);
    EndingTime.QuadPart -= Run_Start.QuadPart;
    EndingTime.QuadPart -= Paused.QuadPart;

    if (prun_time)
	*prun_time = (double)EndingTime.QuadPart / Frequency.QuadPart;
    if (pwait_time)
	*pwait_time = (double)Paused.QuadPart / Frequency.QuadPart;
}

void
pause_runclock(void)
{
    if(failed) return;
    QueryPerformanceCounter(&Run_Pause);
}

void
unpause_runclock(void)
{
    LARGE_INTEGER EndingTime;
    if(failed) return;

    QueryPerformanceCounter(&EndingTime);
    Paused.QuadPart += EndingTime.QuadPart - Run_Pause.QuadPart;
}

#else
#include <sys/time.h>

static struct timeval run_start, paused, run_pause;

void
start_runclock(void)
{
    gettimeofday(&run_start, 0);
    paused.tv_sec = 0;
    paused.tv_usec = 0;
}

void
finish_runclock(double * prun_time, double *pwait_time)
{
    struct timeval run_end;
    gettimeofday(&run_end, 0);

    run_end.tv_sec -= run_start.tv_sec;
    run_end.tv_usec -= run_start.tv_usec;
    if (run_end.tv_usec < 0)
        { run_end.tv_usec += 1000000; run_end.tv_sec -= 1; }

    run_end.tv_sec -= paused.tv_sec;
    run_end.tv_usec -= paused.tv_usec;
    if (run_end.tv_usec < 0)
        { run_end.tv_usec += 1000000; run_end.tv_sec -= 1; }

    if (prun_time)
	*prun_time = (run_end.tv_sec + run_end.tv_usec/1000000.0);
    if (pwait_time)
	*pwait_time = (paused.tv_sec + paused.tv_usec/1000000.0);
}

void
pause_runclock(void)
{
    gettimeofday(&run_pause, 0);
}

void
unpause_runclock(void)
{
    struct timeval run_end;

    gettimeofday(&run_end, 0);
    paused.tv_sec += run_end.tv_sec - run_pause.tv_sec;
    paused.tv_usec += run_end.tv_usec - run_pause.tv_usec;

    if (paused.tv_usec < 0)
        { paused.tv_usec += 1000000; paused.tv_sec -= 1; }
    if (paused.tv_usec >= 1000000)
        { paused.tv_usec -= 1000000; paused.tv_sec += 1; }
}
#endif
