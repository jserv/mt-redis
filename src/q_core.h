//
// Created by chunlei zhang on 2019/07/19.
//

#ifndef Q_REDIS_Q_CORE_H
#define Q_REDIS_Q_CORE_H

/* Error codes */
#define C_OK    0
#define C_ERR   -1
#define C_SCHED -2

/* Anti-warning macro... */
#define UNUSED(V) ((void) V)


/* Log levels */
#define LL_DEBUG 0
#define LL_VERBOSE 1
#define LL_NOTICE 2
#define LL_WARNING 3
#define LL_RAW (1<<10) /* Modifier to log without timestamp */
#define CONFIG_DEFAULT_VERBOSITY LL_NOTICE


#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */
#define NET_PEER_ID_LEN (NET_IP_STR_LEN+32) /* Must be enough for ip:port */

#ifdef __GNUC__
void serverLog(int level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
void serverLog(int level, const char *fmt, ...);
#endif
void serverLogRaw(int level, const char *msg);
void serverLogFromHandler(int level, const char *msg);

#endif

