#ifndef _URCU_QSBR_H
#define _URCU_QSBR_H

/*
 * urcu-qsbr.h
 *
 * Userspace RCU QSBR header.
 *
 * LGPL-compatible code should include this header with :
 *
 * #define _LGPL_SOURCE
 * #include <urcu.h>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * IBM's contributions to this file may be relicensed under LGPLv2 or later.
 */

#include <stdlib.h>
#include <pthread.h>

/*
 * See urcu/pointer.h and urcu/static/pointer.h for pointer
 * publication headers.
 */
#include <urcu/pointer.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <urcu/map/urcu-qsbr.h>

#ifdef RCU_DEBUG	/* For backward compatibility */
#define DEBUG_RCU
#endif

/*
 * Important !
 *
 * Each thread containing read-side critical sections must be registered
 * with rcu_register_thread() before calling rcu_read_lock().
 * rcu_unregister_thread() should be called before the thread exits.
 */

#ifdef _LGPL_SOURCE

#include <urcu/static/urcu-qsbr.h>

/*
 * Mappings for static use of the userspace RCU library.
 * Should only be used in LGPL-compatible code.
 */

/*
 * rcu_read_lock()
 * rcu_read_unlock()
 *
 * Mark the beginning and end of a read-side critical section.
 * DON'T FORGET TO USE rcu_register_thread/rcu_unregister_thread()
 * FOR EACH THREAD WITH READ-SIDE CRITICAL SECTION.
 */
#define urcu_qsbr_read_lock		_urcu_qsbr_read_lock
#define urcu_qsbr_read_unlock		_urcu_qsbr_read_unlock
#define urcu_qsbr_read_ongoing		_urcu_qsbr_read_ongoing

#define urcu_qsbr_quiescent_state	_urcu_qsbr_quiescent_state
#define urcu_qsbr_thread_offline	_urcu_qsbr_thread_offline
#define urcu_qsbr_thread_online		_urcu_qsbr_thread_online

#else /* !_LGPL_SOURCE */

/*
 * library wrappers to be used by non-LGPL compatible source code.
 */

/*
 * QSBR read lock/unlock are guaranteed to be no-ops. Therefore, we expose them
 * in the LGPL header for any code to use. However, the debug version is not
 * nops and may contain sanity checks. To activate it, applications must be
 * recompiled with -DDEBUG_RCU (even non-LGPL/GPL applications), or
 * compiled against a urcu/config.h that has CONFIG_RCU_DEBUG defined.
 * This is the best trade-off between license/performance/code
 * triviality and library debugging & tracing features we could come up
 * with.
 */

#if (!defined(BUILD_QSBR_LIB) && !defined(DEBUG_RCU) && !defined(CONFIG_RCU_DEBUG))

static inline void urcu_qsbr_read_lock(void)
{
}

static inline void urcu_qsbr_read_unlock(void)
{
}

#else /* #if (!defined(BUILD_QSBR_LIB) && !defined(DEBUG_RCU) && !defined(CONFIG_RCU_DEBUG)) */

extern void urcu_qsbr_read_lock(void);
extern void urcu_qsbr_read_unlock(void);

#endif /* #else #if (!defined(BUILD_QSBR_LIB) && !defined(DEBUG_RCU) && !defined(CONFIG_RCU_DEBUG)) */

extern int urcu_qsbr_read_ongoing(void);
extern void urcu_qsbr_quiescent_state(void);
extern void urcu_qsbr_thread_offline(void);
extern void urcu_qsbr_thread_online(void);

#endif /* !_LGPL_SOURCE */

extern void urcu_qsbr_synchronize_rcu(void);

/*
 * Reader thread registration.
 */
extern void urcu_qsbr_register_thread(void);
extern void urcu_qsbr_unregister_thread(void);

#ifdef __cplusplus
}
#endif

#include <urcu/call-rcu.h>
#include <urcu/defer.h>
#include <urcu/flavor.h>

#ifndef URCU_API_MAP
#include <urcu/map/clear.h>
#endif

#endif /* _URCU_QSBR_H */
