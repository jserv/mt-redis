//
// Created by chunlei zhang on 2019/07/18.
//

#ifndef Q_REDIS_Q_WORKER_H
#define Q_REDIS_Q_WORKER_H

#include <urcu/rculfqueue.h>    /* RCU Lock-free queue */
#include <urcu/wfcqueue.h>

#include "darray.h"
#include "q_eventloop.h"
#include "ae.h"

typedef struct q_worker_stats {
    unsigned long lol; //longest output_list
    unsigned long bib; //biggest_input_buffer
    unsigned long connected_clients;
    //long long stat_net_input_bytes;
} q_worker_stats;

typedef struct q_worker {

    int id;
    q_eventloop qel;

    int socketpairs[2];   /*0: used by master/server thread, 1: belong to worker thread.*/

    /* queue for replication thread to switch the client back to this worker's thread.*/
    struct cds_wfcq_head r_head;
    struct cds_wfcq_tail r_tail;

    struct cds_wfcq_tail q_tail;
    struct cds_wfcq_head q_head;

    q_worker_stats stats;
} q_worker;

struct connswapunit {
    int num;
    void *data;

    struct cds_wfcq_node q_node;
};

//command forwarded from worker thread to server thread.
typedef struct q_command_request {
    int type;
    struct client* c;
    struct cds_wfcq_node q_node;
} q_command_request;

extern struct darray workers;

int q_workers_init(uint32_t worker_count);
int q_workers_run(void);
int q_workers_wait(void);
void q_workers_deinit(void);

struct connswapunit *csui_new(void);
void csui_free(struct connswapunit *item);
void csul_push(q_worker *worker, struct connswapunit *su);
struct connswapunit *csul_pop(q_worker *worker);
int worker_get_next_idx(int curidx);
void dispatch_conn_new(int sd);
//struct q_eventloop *get_dispatched_worker_eventloop(void);
void worker_before_sleep(struct aeEventLoop *eventLoop, void *private_data);
int worker_cron(struct aeEventLoop *eventLoop, long long id, void *clientData);
struct connswapunit* csul_server_pop(q_worker *worker);
void csul_server_push(q_worker *worker, struct connswapunit *su);

struct q_command_request* q_createCommandRequest(struct client* c);
void q_freeCommandRequest(struct q_command_request* r);

#endif //Q_REDIS_Q_WORKER_H
