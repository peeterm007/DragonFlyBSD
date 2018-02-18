/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
 * Copyright (c) 2006 David Xu <davidxu@freebsd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "namespace.h"
#include <machine/tls.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

#ifdef _PTHREADS_DEBUGGING

#include <stdio.h>
#include <stdarg.h>
#include <sys/file.h>

#endif

#if defined(_PTHREADS_INVARIANTS)
#define MUTEX_INIT_LINK(m)		do {		\
	(m)->m_qe.tqe_prev = NULL;			\
	(m)->m_qe.tqe_next = NULL;			\
} while (0)
#define MUTEX_ASSERT_IS_OWNED(m)	do {		\
	if ((m)->m_qe.tqe_prev == NULL)			\
		PANIC("mutex is not on list");		\
} while (0)
#define MUTEX_ASSERT_NOT_OWNED(m)	do {		\
	if (((m)->m_qe.tqe_prev != NULL) ||		\
	    ((m)->m_qe.tqe_next != NULL))		\
		PANIC("mutex is on list");		\
} while (0)
#define	THR_ASSERT_NOT_IN_SYNCQ(thr)	do {		\
	THR_ASSERT(((thr)->sflags & THR_FLAGS_IN_SYNCQ) == 0, \
	    "thread in syncq when it shouldn't be.");	\
} while (0);
#else
#define MUTEX_INIT_LINK(m)
#define MUTEX_ASSERT_IS_OWNED(m)
#define MUTEX_ASSERT_NOT_OWNED(m)
#define	THR_ASSERT_NOT_IN_SYNCQ(thr)
#endif

#define THR_IN_MUTEXQ(thr)	(((thr)->sflags & THR_FLAGS_IN_SYNCQ) != 0)
#define	MUTEX_DESTROY(m) do {		\
	free(m);			\
} while (0)

umtx_t	_mutex_static_lock;

#ifdef _PTHREADS_DEBUGGING

static
void
mutex_log(const char *ctl, ...)
{
	char buf[256];
	va_list va;
	size_t len;

	va_start(va, ctl);
	len = vsnprintf(buf, sizeof(buf), ctl, va);
	va_end(va);
	_thr_log(buf, len);
}

#else

static __inline
void
mutex_log(const char *ctl __unused, ...)
{
}

#endif

#ifdef _PTHREADS_DEBUGGING2

static void
mutex_log2(struct pthread *curthread, struct pthread_mutex *m, int op)
{
	if (curthread) {
		if (curthread->tid < 32)
			m->m_lastop[curthread->tid] =
				(__sys_getpid() << 16) | op;
	} else {
			m->m_lastop[0] =
				(__sys_getpid() << 16) | op;
	}
}

#else

static __inline
void
mutex_log2(struct pthread *curthread __unused,
	   struct pthread_mutex *m __unused, int op __unused)
{
}

#endif

/*
 * Prototypes
 */
static int	mutex_self_trylock(pthread_mutex_t);
static int	mutex_self_lock(pthread_mutex_t,
			const struct timespec *abstime);
static int	mutex_unlock_common(pthread_mutex_t *);

int __pthread_mutex_init(pthread_mutex_t *mutex,
	const pthread_mutexattr_t *mutex_attr);
int __pthread_mutex_trylock(pthread_mutex_t *mutex);
int __pthread_mutex_lock(pthread_mutex_t *mutex);
int __pthread_mutex_timedlock(pthread_mutex_t *mutex,
	const struct timespec *abs_timeout);

static int
mutex_check_attr(const struct pthread_mutex_attr *attr)
{
	if (attr->m_type < PTHREAD_MUTEX_ERRORCHECK ||
	    attr->m_type >= PTHREAD_MUTEX_TYPE_MAX)
		return (EINVAL);
	if (attr->m_protocol < PTHREAD_PRIO_NONE ||
	    attr->m_protocol > PTHREAD_PRIO_PROTECT)
		return (EINVAL);
	return (0);
}

static void
mutex_init_body(struct pthread_mutex *pmutex,
    const struct pthread_mutex_attr *attr, int private)
{
	_thr_umtx_init(&pmutex->m_lock);
	pmutex->m_type = attr->m_type;
	pmutex->m_protocol = attr->m_protocol;
	TAILQ_INIT(&pmutex->m_queue);
	mutex_log2(tls_get_curthread(), pmutex, 32);
	pmutex->m_owner = NULL;
	pmutex->m_flags = attr->m_flags | MUTEX_FLAGS_INITED;
	if (private)
		pmutex->m_flags |= MUTEX_FLAGS_PRIVATE;
	pmutex->m_count = 0;
	pmutex->m_refcount = 0;
	if (attr->m_protocol == PTHREAD_PRIO_PROTECT)
		pmutex->m_prio = attr->m_ceiling;
	else
		pmutex->m_prio = -1;
	pmutex->m_saved_prio = 0;
	MUTEX_INIT_LINK(pmutex);
}

static int
mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr, int private)
{
	const struct pthread_mutex_attr *attr;
	struct pthread_mutex *pmutex;
	int error;

	if (mutex_attr == NULL) {
		attr = &_pthread_mutexattr_default;
	} else {
		attr = *mutex_attr;
		error = mutex_check_attr(attr);
		if (error != 0)
			return (error);
	}
	if ((pmutex = (pthread_mutex_t)
		malloc(sizeof(struct pthread_mutex))) == NULL)
		return (ENOMEM);
	mutex_init_body(pmutex, attr, private);
	*mutex = pmutex;
	return (0);
}

static int
init_static(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == NULL)
		ret = mutex_init(mutex, NULL, 0);
	else
		ret = 0;
	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}

static int
init_static_private(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == NULL)
		ret = mutex_init(mutex, NULL, 1);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}

int
_pthread_mutex_init(pthread_mutex_t * __restrict mutex,
    const pthread_mutexattr_t * __restrict mutex_attr)
{
	return mutex_init(mutex, mutex_attr, 1);
}

int
__pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr, 0);
}

#if 0
int
_mutex_reinit(pthread_mutex_t *mutexp)
{
	pthread_mutex_t mutex = *mutexp;

	_thr_umtx_init(&mutex->m_lock);
	TAILQ_INIT(&mutex->m_queue);
	MUTEX_INIT_LINK(mutex);
	mutex_log2(tls_get_curthread(), mutex, 33);
	mutex->m_owner = NULL;
	mutex->m_count = 0;
	mutex->m_refcount = 0;
	mutex->m_prio = 0;
	mutex->m_saved_prio = 0;

	return (0);
}
#endif

void
_mutex_fork(struct pthread *curthread)
{
	struct pthread_mutex *m;

	TAILQ_FOREACH(m, &curthread->mutexq, m_qe)
		m->m_lock = UMTX_LOCKED;
}

int
_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	struct pthread *curthread = tls_get_curthread();
	pthread_mutex_t m;
	int ret = 0;

	if (mutex == NULL) {
		ret = EINVAL;
	} else if (*mutex == NULL) {
		ret = 0;
	} else {
		/*
		 * Try to lock the mutex structure, we only need to
		 * try once, if failed, the mutex is in use.
		 */
		ret = THR_UMTX_TRYLOCK(curthread, &(*mutex)->m_lock);
		if (ret)
			return (ret);

		/*
		 * Check mutex other fields to see if this mutex is
		 * in use. Mostly for prority mutex types, or there
		 * are condition variables referencing it.
		 */
		if (((*mutex)->m_owner != NULL) ||
		    (TAILQ_FIRST(&(*mutex)->m_queue) != NULL) ||
		    ((*mutex)->m_refcount != 0)) {
			THR_UMTX_UNLOCK(curthread, &(*mutex)->m_lock);
			ret = EBUSY;
		} else {
			/*
			 * Save a pointer to the mutex so it can be free'd
			 * and set the caller's pointer to NULL:
			 */
			m = *mutex;
			*mutex = NULL;

			/* Unlock the mutex structure: */
			THR_UMTX_UNLOCK(curthread, &m->m_lock);

			/*
			 * Free the memory allocated for the mutex
			 * structure:
			 */
			MUTEX_ASSERT_NOT_OWNED(m);
			MUTEX_DESTROY(m);
		}
	}

	/* Return the completion status: */
	return (ret);
}

static int
mutex_trylock_common(struct pthread *curthread, pthread_mutex_t *mutex)
{
	struct pthread_mutex *m;
	int ret;

	m = *mutex;
	mutex_log("mutex_lock_trylock_common %p\n", m);
	ret = THR_UMTX_TRYLOCK(curthread, &m->m_lock);
	if (ret == 0) {
		mutex_log2(curthread, m, 1);
		m->m_owner = curthread;
		/* Add to the list of owned mutexes: */
		MUTEX_ASSERT_NOT_OWNED(m);
		TAILQ_INSERT_TAIL(&curthread->mutexq, m, m_qe);
	} else if (m->m_owner == curthread) {
		mutex_log2(curthread, m, 2);
		ret = mutex_self_trylock(m);
	} /* else {} */
	mutex_log("mutex_lock_trylock_common %p (returns %d)\n", m, ret);

	return (ret);
}

int
__pthread_mutex_trylock(pthread_mutex_t *m)
{
	struct pthread *curthread = tls_get_curthread();
	int ret;

	if (__predict_false(m == NULL))
		return(EINVAL);
	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static(curthread, m);
		if (__predict_false(ret != 0))
			return (ret);
	}
	return (mutex_trylock_common(curthread, m));
}

int
_pthread_mutex_trylock(pthread_mutex_t *m)
{
	struct pthread	*curthread = tls_get_curthread();
	int	ret = 0;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking the mutex private (delete safe):
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static_private(curthread, m);
		if (__predict_false(ret != 0))
			return (ret);
	}
	return (mutex_trylock_common(curthread, m));
}

static int
mutex_lock_common(struct pthread *curthread, pthread_mutex_t *mutex,
	const struct timespec * abstime)
{
	struct  timespec ts, ts2;
	struct  pthread_mutex *m;
	int	ret = 0;

	m = *mutex;
	mutex_log("mutex_lock_common %p\n", m);
	ret = THR_UMTX_TRYLOCK(curthread, &m->m_lock);
	if (ret == 0) {
		mutex_log2(curthread, m, 3);
		m->m_owner = curthread;
		/* Add to the list of owned mutexes: */
		MUTEX_ASSERT_NOT_OWNED(m);
		TAILQ_INSERT_TAIL(&curthread->mutexq, m, m_qe);
	} else if (m->m_owner == curthread) {
		ret = mutex_self_lock(m, abstime);
	} else {
		if (abstime == NULL) {
			THR_UMTX_LOCK(curthread, &m->m_lock);
			ret = 0;
		} else if (__predict_false(
			abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			abstime->tv_nsec >= 1000000000)) {
				ret = EINVAL;
		} else {
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			ret = THR_UMTX_TIMEDLOCK(curthread, &m->m_lock, &ts2);
		}
		if (ret == 0) {
			mutex_log2(curthread, m, 4);
			m->m_owner = curthread;
			/* Add to the list of owned mutexes: */
			MUTEX_ASSERT_NOT_OWNED(m);
			TAILQ_INSERT_TAIL(&curthread->mutexq, m, m_qe);
		}
	}
	mutex_log("mutex_lock_common %p (returns %d) lock %d,%d\n",
		  m, ret, m->m_lock, m->m_count);
	return (ret);
}

int
__pthread_mutex_lock(pthread_mutex_t *m)
{
	struct pthread *curthread;
	int	ret;

	if (__predict_false(m == NULL))
		return(EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	curthread = tls_get_curthread();
	if (__predict_false(*m == NULL)) {
		ret = init_static(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, NULL));
}

int
_pthread_mutex_lock(pthread_mutex_t *m)
{
	struct pthread *curthread;
	int	ret;

	if (__predict_false(m == NULL))
		return(EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	curthread = tls_get_curthread();
	if (__predict_false(*m == NULL)) {
		ret = init_static_private(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, NULL));
}

int
__pthread_mutex_timedlock(pthread_mutex_t * __restrict m,
    const struct timespec * __restrict abs_timeout)
{
	struct pthread *curthread;
	int	ret;

	if (__predict_false(m == NULL))
		return(EINVAL);

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	curthread = tls_get_curthread();
	if (__predict_false(*m == NULL)) {
		ret = init_static(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, abs_timeout));
}

int
_pthread_mutex_timedlock(pthread_mutex_t *m,
	const struct timespec *abs_timeout)
{
	struct pthread *curthread;
	int	ret;

	if (__predict_false(m == NULL))
		return(EINVAL);

	curthread = tls_get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static_private(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, abs_timeout));
}

int
_pthread_mutex_unlock(pthread_mutex_t *m)
{
	if (__predict_false(m == NULL))
		return(EINVAL);
	return (mutex_unlock_common(m));
}

static int
mutex_self_trylock(pthread_mutex_t m)
{
	int	ret;

	switch (m->m_type) {
	/* case PTHREAD_MUTEX_DEFAULT: */
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_NORMAL:
		ret = EBUSY;
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (m->m_count + 1 > 0) {
			m->m_count++;
			ret = 0;
		} else
			ret = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static int
mutex_self_lock(pthread_mutex_t m, const struct timespec *abstime)
{
	struct timespec ts1, ts2;
	int ret;

	switch (m->m_type) {
	/* case PTHREAD_MUTEX_DEFAULT: */
	case PTHREAD_MUTEX_ERRORCHECK:
		if (abstime) {
			clock_gettime(CLOCK_REALTIME, &ts1);
			TIMESPEC_SUB(&ts2, abstime, &ts1);
			__sys_nanosleep(&ts2, NULL);
			ret = ETIMEDOUT;
		} else {
			/*
			 * POSIX specifies that mutexes should return
			 * EDEADLK if a recursive lock is detected.
			 */
			ret = EDEADLK;
		}
		break;

	case PTHREAD_MUTEX_NORMAL:
		/*
		 * What SS2 define as a 'normal' mutex.  Intentionally
		 * deadlock on attempts to get a lock you already own.
		 */
		ret = 0;
		if (abstime) {
			clock_gettime(CLOCK_REALTIME, &ts1);
			TIMESPEC_SUB(&ts2, abstime, &ts1);
			__sys_nanosleep(&ts2, NULL);
			ret = ETIMEDOUT;
		} else {
			ts1.tv_sec = 30;
			ts1.tv_nsec = 0;
			for (;;)
				__sys_nanosleep(&ts1, NULL);
		}
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (m->m_count + 1 > 0) {
			m->m_count++;
			ret = 0;
		} else
			ret = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static int
mutex_unlock_common(pthread_mutex_t *mutex)
{
	struct pthread *curthread = tls_get_curthread();
	struct pthread_mutex *m;

	if (__predict_false((m = *mutex) == NULL)) {
		mutex_log2(curthread, m, 252);
		return (EINVAL);
	}
	mutex_log("mutex_unlock_common %p\n", m);
	if (__predict_false(m->m_owner != curthread)) {
		mutex_log("mutex_unlock_common %p (failedA)\n", m);
		mutex_log2(curthread, m, 253);
		return (EPERM);
	}

	if (__predict_false(m->m_type == PTHREAD_MUTEX_RECURSIVE &&
			    m->m_count > 0)) {
		m->m_count--;
		mutex_log("mutex_unlock_common %p (returns 0, partial)\n", m);
		mutex_log2(curthread, m, 254);
	} else {
		/*
		 * Clear the count in case this is a recursive mutex.
		 */
		m->m_count = 0;
		m->m_owner = NULL;
		/* Remove the mutex from the threads queue. */
		MUTEX_ASSERT_IS_OWNED(m);
		TAILQ_REMOVE(&curthread->mutexq, m, m_qe);
		mutex_log2(tls_get_curthread(), m, 35);
		MUTEX_INIT_LINK(m);
		mutex_log2(tls_get_curthread(), m, 36);
		/*
		 * Hand off the mutex to the next waiting thread.
		 */
		mutex_log("mutex_unlock_common %p (returns 0) lock %d\n",
			  m, m->m_lock);
		THR_UMTX_UNLOCK(curthread, &m->m_lock);
		mutex_log2(tls_get_curthread(), m, 37);
		mutex_log2(curthread, m, 255);
	}
	return (0);
}

int
_pthread_mutex_getprioceiling(pthread_mutex_t * __restrict mutex,
    int * __restrict prioceiling)
{
	if ((mutex == NULL) || (*mutex == NULL))
		return (EINVAL);
	if ((*mutex)->m_protocol != PTHREAD_PRIO_PROTECT)
		return (EINVAL);
	*prioceiling = (*mutex)->m_prio;
	return (0);
}

int
_pthread_mutex_setprioceiling(pthread_mutex_t * __restrict mutex,
    int prioceiling, int * __restrict old_ceiling)
{
	int ret = 0;
	int tmp;

	if ((mutex == NULL) || (*mutex == NULL))
		ret = EINVAL;
	else if ((*mutex)->m_protocol != PTHREAD_PRIO_PROTECT)
		ret = EINVAL;
	else if ((ret = _pthread_mutex_lock(mutex)) == 0) {
		tmp = (*mutex)->m_prio;
		(*mutex)->m_prio = prioceiling;
		ret = _pthread_mutex_unlock(mutex);
		*old_ceiling = tmp;
	}
	return(ret);
}

int
_mutex_cv_lock(pthread_mutex_t *m, int count)
{
	int	ret;

	if ((ret = _pthread_mutex_lock(m)) == 0) {
		(*m)->m_refcount--;
		(*m)->m_count += count;
	}
	return (ret);
}

int
_mutex_cv_unlock(pthread_mutex_t *mutex, int *count)
{
	struct pthread *curthread = tls_get_curthread();
	struct pthread_mutex *m;

	if (__predict_false(mutex == NULL))
		return (EINVAL);
	if (__predict_false((m = *mutex) == NULL))
		return (EINVAL);
	if (__predict_false(m->m_owner != curthread))
		return (EPERM);

	*count = m->m_count;
	m->m_count = 0;
	m->m_refcount++;
	mutex_log2(tls_get_curthread(), m, 45);
	m->m_owner = NULL;
	/* Remove the mutex from the threads queue. */
	MUTEX_ASSERT_IS_OWNED(m);
	TAILQ_REMOVE(&curthread->mutexq, m, m_qe);
	MUTEX_INIT_LINK(m);
	THR_UMTX_UNLOCK(curthread, &m->m_lock);
	mutex_log2(curthread, m, 250);
	return (0);
}

void
_mutex_unlock_private(pthread_t pthread)
{
	struct pthread_mutex	*m, *m_next;

	for (m = TAILQ_FIRST(&pthread->mutexq); m != NULL; m = m_next) {
		m_next = TAILQ_NEXT(m, m_qe);
		if ((m->m_flags & MUTEX_FLAGS_PRIVATE) != 0)
			_pthread_mutex_unlock(&m);
	}
}

__strong_reference(__pthread_mutex_init, pthread_mutex_init);
__strong_reference(__pthread_mutex_lock, pthread_mutex_lock);
__strong_reference(__pthread_mutex_timedlock, pthread_mutex_timedlock);
__strong_reference(__pthread_mutex_trylock, pthread_mutex_trylock);

/* Single underscore versions provided for libc internal usage: */
/* No difference between libc and application usage of these: */
__strong_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__strong_reference(_pthread_mutex_unlock, pthread_mutex_unlock);
__strong_reference(_pthread_mutex_getprioceiling, pthread_mutex_getprioceiling);
__strong_reference(_pthread_mutex_setprioceiling, pthread_mutex_setprioceiling);
