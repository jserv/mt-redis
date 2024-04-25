//
// Created by chunlei zhang on 2019/07/19.
//

#ifndef Q_REDIS_Q_MASTER_H
#define Q_REDIS_Q_MASTER_H

#include "q_eventloop.h"

typedef struct q_master {
    q_eventloop qel;
} q_master;

extern q_master master;

int q_master_init(void);
void q_master_deinit(void);
void dispatch_conn_exist(struct client *c, int tid);
int q_master_run(void);

#endif
