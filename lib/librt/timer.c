/*
 * Copyright (c) 2016 The DragonFly Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This is somewhat based on
 * $FreeBSD: head/lib/librt/timer.c 227661 2011-11-18 09:56:40Z kib $
 *
 */

#include <sys/syscall.h>

#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

extern int __sys_timer_create(clockid_t, struct sigevent *, int *);
extern int __sys_timer_delete(int);
extern int __sys_timer_gettime(int, struct itimerspec *);
extern int __sys_timer_settime(int, int, const struct itimerspec *,
			       struct itimerspec *);
extern int __sys_timer_getoverrun(int);

#if 0 // XXX
struct __timer {
	int handle;
};
#endif

int
__timer_create(clockid_t clockid, struct sigevent *evp, timer_t *timerid)
{
	int res;

	if (evp == NULL || evp->sigev_notify != SIGEV_THREAD) {
		res = __sys_timer_create(clockid, evp, timerid);
		if (res == -1) {
			return res;
		}
		return 0;
	}
	/* TODO: Deliver by callback to thread */
	return ENOSYS;
}

int
__timer_delete(timer_t timerid)
{
	return __sys_timer_delete(timerid);
}

int
__timer_gettime(timer_t timerid, struct itimerspec *value)
{
	return __sys_timer_gettime(timerid, value);
}

int
__timer_settime(timer_t timerid, int flags,
                const struct itimerspec *value,
                struct itimerspec *ovalue)
{
	return __sys_timer_settime(timerid, flags, value, ovalue);
}

int
__timer_getoverrun(timer_t timerid)
{
	return __sys_timer_getoverrun(timerid);
}

__weak_reference(__timer_create, timer_create);
__weak_reference(__timer_create, _timer_create);
__weak_reference(__timer_delete, timer_delete);
__weak_reference(__timer_delete, _timer_delete);
__weak_reference(__timer_gettime, timer_gettime);
__weak_reference(__timer_gettime, _timer_gettime);
__weak_reference(__timer_settime, timer_settime);
__weak_reference(__timer_settime, _timer_settime);
__weak_reference(__timer_getoverrun, timer_getoverrun);
__weak_reference(__timer_getoverrun, _timer_getoverrun);



