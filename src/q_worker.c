//
// Created by chunlei zhang on 2018/11/28.
//
#include <urcu.h>

#include "q_worker.h"
#include "q_eventloop.h"
#include "server.h"

#define worker_run_with_period(_ms_) if ((_ms_ <= 1000/qel->hz) || !(cron_loops%((_ms_)/(1000/qel->hz))))

/* Which thread we assigned a connection to most recently. */
static int last_worker_thread = -1;
static int num_worker_threads;
struct darray workers;

static void *worker_thread_run(void *args);

struct connswapunit *
csul_pop(q_worker *worker) {
    struct connswapunit *su = NULL;
    struct cds_wfcq_node *qnode;

    qnode = __cds_wfcq_dequeue_blocking(&worker->q_head, &worker->q_tail);
    if (!qnode) {
        return NULL;
    }
    su = caa_container_of(qnode, struct connswapunit, q_node);
    return su;
}

void csul_push(q_worker *worker, struct connswapunit *su) {
    if (su == NULL || worker == NULL) {
        return;
    }
    
    cds_wfcq_enqueue(&worker->q_head, &worker->q_tail, &su->q_node);
    return;
}

struct connswapunit* csul_server_pop(q_worker *worker) 
{
    struct connswapunit *su = NULL;
    struct cds_wfcq_node *qnode = NULL;
    //pop from the replication queue (head, tail)
    qnode = __cds_wfcq_dequeue_blocking(&worker->r_head, &worker->r_tail);

    if (!qnode) {
        return NULL;
    }

    su = caa_container_of(qnode, struct connswapunit, q_node);
    return su;
}

void csul_server_push(q_worker *worker, struct connswapunit *su) 
{
    if (su == NULL || worker == NULL) {
        return;
    }

    //Qredis: forget to intialize q_node, will caues panic for pop
    // put the initialization code to csui_new() by cds_wfcq_node_init(&su->q_node);
    cds_wfcq_enqueue(&worker->r_head, &worker->r_tail, &su->q_node);
    return;
}

void q_worker_init_stats(q_worker_stats *stats) {
    stats->bib = 0;
    stats->connected_clients = 0;
    stats->lol = 0;
}

int
q_worker_init(q_worker *worker) {
    int status;

    if (worker == NULL) {
        return C_ERR;
    }

    q_worker_init_stats(&worker->stats);
    worker->id = 0;
    worker->socketpairs[0] = -1;
    worker->socketpairs[1] = -1;
    cds_wfcq_init(&worker->q_head, &worker->q_tail);
    cds_wfcq_init(&worker->r_head, &worker->r_tail);

    adjustOpenFilesLimit();
    q_eventloop_init(&worker->qel, server.maxclients);
    worker->qel.thread.fun_run = worker_thread_run;
    worker->qel.thread.data = worker;

    status = socketpair(AF_LOCAL, SOCK_STREAM, 0, worker->socketpairs);
    if (status < 0) {
        serverLog(LL_WARNING, "create socketpairs failed: %s", strerror(errno));
        return C_ERR;
    }
    status = anetNonBlock(NULL, worker->socketpairs[0]);
    if (status < 0) {
        serverLog(LL_WARNING, "set socketpairs[0] %d nonblocking failed: %s",
                  worker->socketpairs[0], strerror(errno));
        close(worker->socketpairs[0]);
        worker->socketpairs[0] = -1;
        close(worker->socketpairs[1]);
        worker->socketpairs[1] = -1;
        return C_ERR;
    }
    status = anetNonBlock(NULL, worker->socketpairs[1]);
    if (status < 0) {
        serverLog(LL_WARNING, "set socketpairs[1] %d nonblocking failed: %s",
                  worker->socketpairs[1], strerror(errno));
        close(worker->socketpairs[0]);
        worker->socketpairs[0] = -1;
        close(worker->socketpairs[1]);
        worker->socketpairs[1] = -1;
        return C_ERR;
    }

    return C_OK;
}


// worker threads to master/server thread communication handler function
static void
worker_thread_event_process(aeEventLoop *el, int fd, void *privdata, int mask) {
    int status;
    int sd;
    int res = C_OK;
    q_worker *worker = privdata;
    char buf[1];
    struct connswapunit *csu;
    client *c;
    q_eventloop *qel=NULL;

    UNUSED(mask);

    serverAssert(el == worker->qel.el);
    serverAssert(fd == worker->socketpairs[1]);

    if (read(fd, buf, 1) != 1) {
        serverLog(LL_WARNING, "Can't read for worker(id:%d) socketpairs[1](%d)",
                 worker->qel.thread.id, fd);
        buf[0] = 'c';
    }

    switch (buf[0]) {
         case 'c':
             csu = csul_pop(worker);
             if (csu == NULL) {
                 return;
             }
             sd = csu->num;
             zfree(csu);
             status = anetNonBlock(NULL, sd);
             if (status < 0) {
                 serverLog(LL_WARNING, "set nonblock on c %d failed: %s",
                           sd, strerror(errno));
                 close(sd);
                 return;
             }
 
             // we only deal with AF_INET and AF_INET6 connection
             //if (qlisten->info.family == AF_INET || qlisten->info.family == AF_INET6) {
                 status = anetEnableTcpNoDelay(NULL, sd);
                 if (status < 0) {
                     serverLog(LL_WARNING, "set tcpnodelay on c %d failed, ignored: %s",
                              sd, strerror(errno));
                 }
             //}
 
             c = createClient(&worker->qel, sd);
             if (c == NULL) {
                 serverLog(LL_WARNING, "Create client failed");
                 close(sd);
                 return;
             }
             c->curidx = worker->id;
             //  sd should already be added to worker's eventloop inside createClient function.
             /*
              * status = aeCreateFileEvent(worker->qel.el, sd, AE_READABLE,
              *                            readQueryFromClient, c);
              * if (status == AE_ERR) {
              *     serverLog(LL_WARNING, "Unrecoverable error creating worker ipfd file event.");
              *     return;
              * }
              */
 
             break;
        case 'b':
            /* receive back the client from server thread. */
           csu = csul_server_pop(worker);
           if (csu == NULL) {
               return;
           }
           c = rcu_dereference(csu->data);
           //ToDo: make use csui_free instead;
           zfree(csu);
           //csui_free(csu);
           //call_rcu(&csu->rcu_head, q_free_connswapunit);
           qel = &worker->qel;
           c->qel = &worker->qel;
           c->curidx = worker->id;
           
           //the client may has pending reply from replication slaveofcommand, 
           //so relink to worker's event loop
           c->flags &= ~CLIENT_JUMP;
           resetClient(c);

           listAddNodeTail(qel->clients, c);
           c->qel = qel;

           if (c->flags & CLIENT_CLOSE_ASAP) {
               //Leave the serverCron to free the client. We can do nothing here and just return
               //listAddNodeTail(qel->clients_to_close, c);
               //freeClient(c);
               return;
           }

           //ToDo: the CLIENT_PENDING_WRITE flag has been cleared by server thread.
           if (c->flags & CLIENT_PENDING_WRITE) {
               listAddNodeTail(qel->clients_pending_write, c);
               if (aeCreateFileEvent(qel->el, c->fd, AE_WRITABLE,
                           sendReplyToClient, c) == AE_ERR) {
                   freeClient(c);
                   return;
               }
           } else if(clientHasPendingReplies(c)){
               if(aeCreateFileEvent(qel->el, c->fd, AE_WRITABLE, 
                           sendReplyToClient, c) == AE_ERR) {
                   freeClient(c);
                   return;
               }
           }

           if (sdslen(c->querybuf) > 0 ) {
               // if we still have command to be processed inside querybuf, process it first.
               res = worker_processInputBuffer(c);
           } 
           if (res == C_OK) {
               if (aeCreateFileEvent(qel->el, c->fd, AE_READABLE, 
                           worker_readQueryFromClient,c) == AE_ERR) {

                   freeClient(c);
                   serverPanic("adding worker_readQueryFromClient to event loop panic");
                   return;
                }
           }

           break;
        default:
           serverLog(LL_WARNING, "read error char '%c' for worker(id:%d) socketpairs[1](%d)",
                   buf[0], worker->qel.thread.id, worker->socketpairs[1]);
           break;
    }
}

/* This function gets called every time Redis is entering the
 * main loop of the event driven library, that is, before to sleep
 * for ready file descriptors. */
void
worker_before_sleep(struct aeEventLoop *eventLoop, void *private_data) {
    q_worker *worker = private_data;

    UNUSED(eventLoop);
    UNUSED(private_data);

    serverAssert(eventLoop == worker->qel.el);

    /* Handle writes with pending output buffers. */
    handleClientsWithPendingWrites(&worker->qel);
    //activeExpireCycle(worker, ACTIVE_EXPIRE_CYCLE_FAST);
}

int worker_cron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    static long long cron_loops = 0;
    q_worker *worker = (q_worker *) clientData;
    q_eventloop *qel = &worker->qel;
    serverAssert(worker->qel.el == eventLoop);

    UNUSED(id);

    qel->unixtime = time(NULL);
    qel->mstime = mstime();

    handleClientsWithPendingWrites(&worker->qel);

    worker_run_with_period(100) {
        trackInstantaneousMetric(STATS_METRIC_COMMAND, qel, 
                qel->stats.stat_numcommands);
        trackInstantaneousMetric(STATS_METRIC_NET_INPUT, qel, 
                qel->stats.stat_net_input_bytes);
        trackInstantaneousMetric(STATS_METRIC_NET_OUTPUT, qel,
                qel->stats.stat_net_output_bytes);
    }

    freeClientsInAsyncFreeQueue(&worker->qel);

    //to collect stats for info Command
    //if (!(cron_loops%((5000)/(1000/worker->qel.hz)))) {
    worker_run_with_period(5000) {
        client *c;
        listNode *ln;
        listIter li;
        listRewind(worker->qel.clients, &li);
        while((ln = listNext(&li)) != NULL) {
            c = listNodeValue(ln);
            if (listLength(c->reply) > worker->stats.lol) {
                worker->stats.lol = listLength(c->reply);
            }
            if (sdslen(c->querybuf) > worker->stats.bib) {
                worker->stats.bib = sdslen(c->querybuf);
            }
        }
        worker->stats.connected_clients = listLength(worker->qel.clients);
    }

    cron_loops++;
    return 1000/worker->qel.hz;
}

static int
setup_worker(q_worker *worker) {
    int status;

    status = aeCreateFileEvent(worker->qel.el, worker->socketpairs[1], AE_READABLE,
            worker_thread_event_process, worker);
    if (status == AE_ERR) {
        serverLog(LL_WARNING, "Unrecoverable error creating worker ipfd file event.");
        return C_ERR;
    }

    aeSetBeforeSleepProc(worker->qel.el, worker_before_sleep, worker);

    /* Create the serverCron() time event, that's our main way to process
     * background operations. */
    if (aeCreateTimeEvent(worker->qel.el, 1, worker_cron, worker, NULL) == AE_ERR) {
        serverPanic("Can't create the serverCron time event.");
        return C_ERR;
    }

    return C_OK;
}


static void *
worker_thread_run(void *args) {
    q_worker *worker = args;

    rcu_register_thread();
    /* vire worker run */
    aeMain(worker->qel.el);
    rcu_unregister_thread();
    return NULL;
}

int
q_workers_init(uint32_t worker_count) {
    int status;
    uint32_t idx;
    q_worker *worker;

    darray_init(&workers, worker_count, sizeof(q_worker));

    for (idx = 0; idx < worker_count; idx++) {
        worker = darray_push(&workers);
        q_worker_init(worker);
        worker->id = idx;
        worker->qel.id = idx;
        status = setup_worker(worker);
        if (status != C_OK) {
            exit(1);
        }
    }

    num_worker_threads = (int) darray_n(&workers);

    return C_OK;
}

void
q_worker_deinit(q_worker *worker) {
    if (worker == NULL) {
        return;
    }

    q_eventloop_deinit(&worker->qel);

    if (worker->socketpairs[0] > 0) {
        close(worker->socketpairs[0]);
        worker->socketpairs[0] = -1;
    }
    if (worker->socketpairs[1] > 0) {
        close(worker->socketpairs[1]);
        worker->socketpairs[1] = -1;
    }

    // destroy the lfqueue
    //cds_lfq_destroy_rcu(&worker->csul);
}

void q_workers_deinit(void) {
    q_worker *worker;
    while (darray_n(&workers)) {
        worker = darray_pop(&workers);
        q_worker_deinit(worker);
    }
}

int
q_workers_run(void) {
    uint32_t i, thread_count;
    q_worker *worker;

    thread_count = (uint32_t) num_worker_threads;
    serverLog(LL_NOTICE, "fn: q_workers_run, start %d worker thread", thread_count);

    for (i = 0; i < thread_count; i++) {
        worker = darray_get(&workers, i);
        q_thread_start(&worker->qel.thread);
    }

    return C_OK;
}


// server main thread will create worker and master thread, 
// main thread will enter it's own eventloop, so there is no need
// to call q_workers_wait() as previous implementation did
int
q_workers_wait(void) {
    uint32_t i, thread_count;
    q_worker *worker;

    thread_count = (uint32_t) num_worker_threads;

    for (i = 0; i < thread_count; i++) {
        worker = darray_get(&workers, i);
        pthread_join(worker->qel.thread.thread_id, NULL);
    }

    return C_OK;
}

void
dispatch_conn_new(int sd) {
    struct connswapunit *su = csui_new();
    char buf[1];
    q_worker *worker;

    if (su == NULL) {
        close(sd);
        /* given that malloc failed this may also fail, but let's try */
        serverLog(LL_WARNING, "Failed to allocate memory for connection swap object\n");
        return;
    }

    int tid = (last_worker_thread + 1) % server.threads_num;
    worker = darray_get(&workers, (uint32_t) tid);
    last_worker_thread = tid;

    su->num = sd;
    su->data = NULL;
    csul_push(worker, su);

    buf[0] = 'c';
    // to be handled by worker's worker_thread_event_process loop
    if (write(worker->socketpairs[0], buf, 1) != 1) {
        serverLog(LL_WARNING, "Notice the worker failed.");
    }
}

/* dispatch new connection client to worker's eventloop. */
/*
 * q_eventloop *get_dispatched_worker_eventloop(void) {
 *     q_worker *worker;
 * 
 *     int tid = (last_worker_thread + 1) % server.threads_num;
 *     worker = darray_get(&workers, (uint32_t) tid);
 *     last_worker_thread = tid;
 *     return &worker->qel;
 * }
 */

struct connswapunit *
csui_new(void) {
    struct connswapunit *item = NULL;

    item = zmalloc(sizeof(struct connswapunit));
    cds_wfcq_node_init(&item->q_node);
    return item;
}

struct q_command_request* q_createCommandRequest(client* c){
    struct q_command_request *r = zmalloc(sizeof(*r));
    //r->type = type;
    r->c = c;
    cds_wfcq_node_init(&r->q_node);
    return r;
}

void q_freeCommandRequest(struct q_command_request* r) {
    r->c = NULL;
    zfree(r);
}
