//
// Created by chunlei zhang on 2019/07/18.
//
//
#ifndef Q_REDIS_Q_EVENTLOOP_H
#define Q_REDIS_Q_EVENTLOOP_H

#include "ae.h"
#include "adlist.h"
#include "q_thread.h"

/* Instantaneous metrics tracking. */
#define STATS_METRIC_SAMPLES 16     /* Number of samples per metric. */
#define STATS_METRIC_COMMAND 0      /* Number of commands executed. */
#define STATS_METRIC_NET_INPUT 1    /* Bytes read to network .*/
#define STATS_METRIC_NET_OUTPUT 2   /* Bytes written to network. */
#define STATS_METRIC_COUNT 3

typedef struct q_eventloop_stats {
    long long stat_numcommands;
    long long stat_net_input_bytes;
    long long stat_net_output_bytes; /* Bytes written to network. */

    /* The following two are used to track instantaneous metrics, like
     * number of operations per second, network traffic. */
    struct {
        long long last_sample_time; /* Timestamp of last sample in ms */
        long long last_sample_count;/* Count in last sample */
        long long samples[STATS_METRIC_SAMPLES];
        int idx;
    } inst_metric[STATS_METRIC_COUNT];
} q_eventloop_stats;


// q_eventloop for worker threads
typedef struct q_eventloop {
    q_thread thread;
    aeEventLoop *el;
    int hz;
    int id; //corresponding worker thread's id. For server thread, set the id to -1
    int cronloops;

    time_t  unixtime;
    long long mstime;

    q_eventloop_stats stats;

    long long  next_client_id;            /* Next client unique ID. Incremental. */
    struct client *current_client;      /* Current client, only used on crash report */
    list *clients;                      /* List of active clients */
    list *clients_pending_write;        /* There is to write or install handler. */
    list *clients_to_close;             /* Clients to close asynchronously */

    /* Blocked clients */
    list *unblocked_clients;            /* list of clients to unblock before next loop */
} q_eventloop;

int q_eventloop_init(q_eventloop *qel, int filelimit);                                           
void q_eventloop_deinit(q_eventloop *qel);
void trackInstantaneousMetric(int metric, q_eventloop *qel, long long current_reading);
void resetEventloopStats(q_eventloop_stats *stats);
#endif
