//
// Created by chunlei zhang on 2018/07/18.
//

#include "q_eventloop.h"
#include "q_thread.h"
#include "server.h"

void resetEventloopStats(q_eventloop_stats *stats) {
    int j;
    stats->stat_numcommands = 0;
    stats->stat_net_input_bytes = 0;
    stats->stat_net_output_bytes = 0;

    for (j = 0; j< STATS_METRIC_COUNT; j++) {
        stats->inst_metric[j].idx = 0;
        stats->inst_metric[j].last_sample_count = 0;
        stats->inst_metric[j].last_sample_time = mstime();
        memset(stats->inst_metric[j].samples, 0, 
                sizeof(stats->inst_metric[j].samples));
    }
}

int q_eventloop_init(q_eventloop *qel, int filelimit) {
    if (qel == NULL || filelimit <= 0) {
        return C_ERR;
    }

    q_thread_init(&qel->thread);
    qel->el = NULL;
    qel->hz = 10;
    qel->cronloops = 0;
    //qel->unixtime = time(NULL);
    //qel->mstime = mstime();
    qel->next_client_id = 1;    /* Client IDs, start from 1 .*/
    qel->current_client = NULL;
    qel->clients = NULL;
    qel->clients_pending_write = NULL;
    qel->clients_to_close = NULL;
    qel->unblocked_clients = NULL;

    qel->el = aeCreateEventLoop(filelimit);
    if (qel->el == NULL) {
        serverLog(LL_WARNING, "q_eventloop_init: create eventloop failed.");
        serverPanic("create eventloop failed.");
        return C_ERR;
    }

    qel->clients = listCreate();
    if (qel->clients == NULL) {
        serverLog(LL_WARNING, "q_eventloop_init: create list failed(out of memory)");
        return C_ERR;
    }

    qel->clients_pending_write = listCreate();
    if (qel->clients_pending_write == NULL) {
        serverLog(LL_WARNING, "q_eventloop_init: create list failed(out of memory)");
        return C_ERR;
    }

    qel->clients_to_close = listCreate();
    if (qel->clients_to_close == NULL) {
        serverLog(LL_WARNING, "q_eventloop_init: create list failed(out of memory)");
        return C_ERR;
    }

    qel->unblocked_clients = listCreate();
    if (qel->unblocked_clients == NULL) {
        serverLog(LL_WARNING, "q_eventloop_init: create list failed(out of memory)");
        return C_ERR;
    }

    resetEventloopStats(&qel->stats);
    return C_OK;
}

void q_eventloop_deinit(q_eventloop *qel) {
    if (qel == NULL) {
        return;
    }

    q_thread_deinit(&qel->thread);

    if (qel->el != NULL) {
        aeDeleteEventLoop(qel->el);
        qel->el = NULL;
    }

    if (qel->clients != NULL) {
        client *c;
        while ((c = listPop(qel->clients))){
            freeClient(c);
        }
        listRelease(qel->clients);
        qel->clients = NULL;
    }

    if (qel->clients_pending_write != NULL) {
        client *c;
        while ((c = listPop(qel->clients_pending_write))) {}
        listRelease(qel->clients_pending_write);
        qel->clients_pending_write = NULL;
    }

    if (qel->clients_to_close != NULL) {
        client *c;
        while ((c = listPop(qel->clients_to_close))) {
            freeClient(c);
        }
        listRelease(qel->clients_to_close);
        qel->clients_to_close = NULL;
    }

    if (qel->unblocked_clients != NULL) {
        client *c;
        while ((c = listPop(qel->unblocked_clients))) {}
        listRelease(qel->unblocked_clients);
        qel->unblocked_clients = NULL;
    }

}
