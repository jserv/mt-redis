//
// Created by chunlei zhang on 2019/07/18.
//

#ifndef Q_REDIS_Q_THREAD_H
#define Q_REDIS_Q_THREAD_H

#include <pthread.h>

typedef void *(*q_thread_func_t)(void *data);

typedef struct q_thread {
    int id;
    pthread_t thread_id;
    q_thread_func_t fun_run;
    void *data;
} q_thread;

int q_thread_init(q_thread *thread);
void q_thread_deinit(q_thread *thread);
int q_thread_start(q_thread *thread);

#endif //Q_REDIS_Q_THREAD_H
