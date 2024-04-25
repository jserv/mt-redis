#include <urcu.h>
#include <errno.h>

#include "q_master.h"
#include "q_worker.h"
#include "server.h"

q_master master;
static int setup_master(void);
static void *master_thread_run(void *args);

int q_master_init(void) {
    int filelimit;
    
    filelimit = server.threads_num * 2  + 128;
    q_eventloop_init(&master.qel, filelimit);
    master.qel.thread.fun_run = master_thread_run;
    setup_master();
    return C_OK;
}


void q_master_deinit(void) {
    q_eventloop_deinit(&master.qel);
}

#define MAX_ACCEPTS_PER_CALL 1000
static void acceptCommonHandler(int fd, int flags, char *ip) 
{
    /* If the server is running in protected mode (the default) and there
     * is no password set, nor a specific interface is bound, we don't accept
     * requests from non loopback interfaces. Instead we try to explain the
     * user what to do to fix it if needed. */
    if (server.protected_mode &&
            server.bindaddr_count == 0 &&
            server.requirepass == NULL &&
            !(flags & CLIENT_UNIX_SOCKET) &&
            ip != NULL)
    {
        if (strcmp(ip,"127.0.0.1") && strcmp(ip,"::1")) {
            char *err =
                "-DENIED Redis is running in protected mode because protected "
                "mode is enabled, no bind address was specified, no "
                "authentication password is requested to clients. In this mode "
                "connections are only accepted from the loopback interface. "
                "If you want to connect from external computers to Redis you "
                "may adopt one of the following solutions: "
                "1) Just disable protected mode sending the command "
                "'CONFIG SET protected-mode no' from the loopback interface "
                "by connecting to Redis from the same host the server is "
                "running, however MAKE SURE Redis is not publicly accessible "
                "from internet if you do so. Use CONFIG REWRITE to make this "
                "change permanent. "
                "2) Alternatively you can just disable the protected mode by "
                "editing the Redis configuration file, and setting the protected "
                "mode option to 'no', and then restarting the server. "
                "3) If you started the server manually just for testing, restart "
                "it with the '--protected-mode no' option. "
                "4) Setup a bind address or an authentication password. "
                "NOTE: You only need to do one of the above things in order for "
                "the server to start accepting connections from the outside.\r\n";
            if (write(fd,err,strlen(err)) == -1) {
                /* Nothing to do, Just to avoid the warning... */
            }
            server.stat_rejected_conn++;
            //freeClient(c);
            return;
        }
    }

    /*
     * if ((c = createClient(get_dispatched_worker_eventloop(), fd)) == NULL) {
     *     serverLog(LL_WARNING,
     *             "Error registering fd event for the new client: %s (fd=%d)",
     *             strerror(errno),fd);
     *     close(fd); [> May be already closed, just ignore errors <]
     *     return;
     * }
     */
    dispatch_conn_new(fd);

    //ToDo: collect the total clients from all worker and server thread and 
    //do the rejection as necessary.
    /* If maxclient directive is set and this is one client more... close the
     * connection. Note that we create the client instead to check before
     * for this condition, since now the socket is already set in non-blocking
     * mode and we can send an error for free using the Kernel I/O */

    /* ToDo: we does not have central place to keep clients list.
     * collect each worker thread's clients length and do the check. */
    /*
     *     if (listLength(server.clients) > server.maxclients) {
     *         char *err = "-ERR max number of clients reached\r\n";
     *
     *         [> That's a best effort error message, don't check write errors <]
     *         if (write(c->fd,err,strlen(err)) == -1) {
     *             [> Nothing to do, Just to avoid the warning... <]
     *         }
     *         server.stat_rejected_conn++;
     *         freeClient(c);
     *         return;
     *     }
     */


    server.stat_numconnections++;
}

void acceptTcpHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cport, cfd, max = MAX_ACCEPTS_PER_CALL;
    char cip[NET_IP_STR_LEN];
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);

    while(max--) {
        cfd = anetTcpAccept(server.neterr, fd, cip, sizeof(cip), &cport);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                serverLog(LL_WARNING,
                        "Accepting client connection: %s", server.neterr);
            return;
        }
        serverLog(LL_DEBUG,"Accepted %s:%d", cip, cport);
        acceptCommonHandler(cfd,0,cip);
    }
}

void acceptUnixHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
    int cfd, max = MAX_ACCEPTS_PER_CALL;
    UNUSED(el);
    UNUSED(mask);
    UNUSED(privdata);

    while(max--) {
        cfd = anetUnixAccept(server.neterr, fd);
        if (cfd == ANET_ERR) {
            if (errno != EWOULDBLOCK)
                serverLog(LL_WARNING,
                        "Accepting client connection: %s", server.neterr);
            return;
        }
        serverLog(LL_VERBOSE,"Accepted connection to %s", server.unixsocket);
        //acceptCommonHandler(cfd,CLIENT_UNIX_SOCKET,NULL);
        //ToDo: skip handling unix socket for now
        close(cfd);
    }
}
static int 
setup_master(void) {
    int j;
    for (j = 0; j < server.ipfd_count; j++) {
        if (aeCreateFileEvent(master.qel.el, server.ipfd[j], AE_READABLE,
                    acceptTcpHandler,NULL) == AE_ERR)
        {
            serverPanic(
                    "Unrecoverable error creating server.ipfd file event.");
        }
    }
    if (server.sofd > 0 && aeCreateFileEvent(master.qel.el,server.sofd,AE_READABLE,
                acceptUnixHandler,NULL) == AE_ERR) 
        serverPanic("Unrecoverable error creating server.sofd file event.");
    return C_OK;
}
static void *master_thread_run(void *args) {
    UNUSED(args);

    rcu_register_thread();
    aeMain(master.qel.el);
    rcu_unregister_thread();
    return NULL;
}

int q_master_run(void) {
    q_thread_start(&master.qel.thread);
    return C_OK;
}
