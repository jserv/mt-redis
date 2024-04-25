//
// Created by chunlei zhang on 2019/07/18.
//

#include "q_thread.h"
#include "server.h"

int
q_thread_init(q_thread *thread) {
    if (thread == NULL) {
        return C_ERR;
    }

    thread->id = 0;
    thread->thread_id = 0;
    thread->fun_run = NULL;
    thread->data = NULL;

    return C_OK;
}

void
q_thread_deinit(q_thread *thread) {
    if (thread == NULL) {
        return;
    }

    thread->id = 0;
    thread->thread_id = 0;
    thread->fun_run = NULL;
    thread->data = NULL;
}

static void *q_thread_run(void *data) {
    q_thread *thread = data;
    srand(ustime() ^ (int) pthread_self());

    return thread->fun_run(thread->data);
}

int q_thread_start(q_thread *thread) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    if (thread == NULL || thread->fun_run == NULL) {
        return C_ERR;
    }

    pthread_create(&thread->thread_id,
                   &attr, q_thread_run, thread);

    return C_OK;
}
