#include <stdarg.h>

#include "exceptions.h"
#include "bridge.h"
#include "mailbox.h"
#include "messages.h"
#include "rtio.h"
#include "dds.h"

/* for the prototypes for watchdog_set() and watchdog_clear() */
#include "clock.h"
/* for the prototype for rpc() */
#include "session.h"
/* for the prototype for log() */
#include "log.h"

void exception_handler(unsigned long vect, unsigned long *sp);
void exception_handler(unsigned long vect, unsigned long *sp)
{
    struct msg_exception msg;
    int i;

    msg.type = MESSAGE_TYPE_EXCEPTION;
    msg.eid = EID_INTERNAL_ERROR;
    for(i=0;i<3;i++)
        msg.eparams[i] = 0;
    mailbox_send_and_wait(&msg);
    while(1);
}

typedef void (*kernel_function)(void);

int main(void);
int main(void)
{
    kernel_function k;
    void *jb;

    k = mailbox_receive();

    if(k == NULL)
        bridge_main();
    else {
        jb = exception_push();
        if(exception_setjmp(jb)) {
            struct msg_exception msg;

            msg.type = MESSAGE_TYPE_EXCEPTION;
            msg.eid = exception_getid(msg.eparams);
            mailbox_send_and_wait(&msg);
        } else {
            struct msg_base msg;

            dds_init();
            rtio_init();
            k();
            exception_pop(1);

            msg.type = MESSAGE_TYPE_FINISHED;
            mailbox_send_and_wait(&msg);
        }
    }
    while(1);
}

int watchdog_set(int ms)
{
    struct msg_watchdog_set_request request;
    struct msg_watchdog_set_reply *reply;
    int id;

    request.type = MESSAGE_TYPE_WATCHDOG_SET_REQUEST;
    request.ms = ms;
    mailbox_send_and_wait(&request);

    reply = mailbox_wait_and_receive();
    if(reply->type != MESSAGE_TYPE_WATCHDOG_SET_REPLY)
        exception_raise(EID_INTERNAL_ERROR);
    id = reply->id;
    mailbox_acknowledge();

    return id;
}

void watchdog_clear(int id)
{
    struct msg_watchdog_clear request;

    request.type = MESSAGE_TYPE_WATCHDOG_CLEAR;
    request.id = id;
    mailbox_send_and_wait(&request);
}

int rpc(int rpc_num, ...)
{
    struct msg_rpc_request request;
    struct msg_rpc_reply *reply;
    int eid, retval;

    request.type = MESSAGE_TYPE_RPC_REQUEST;
    request.rpc_num = rpc_num;
    va_start(request.args, rpc_num);
    mailbox_send_and_wait(&request);
    va_end(request.args);

    reply = mailbox_wait_and_receive();
    if(reply->type != MESSAGE_TYPE_RPC_REPLY)
        exception_raise(EID_INTERNAL_ERROR);
    eid = reply->eid;
    retval = reply->retval;
    mailbox_acknowledge();

    if(eid != EID_NONE)
        exception_raise(eid);
    return retval;
}

void log(const char *fmt, ...)
{
    struct msg_log request;

    request.type = MESSAGE_TYPE_LOG;
    request.fmt = fmt;
    va_start(request.args, fmt);
    mailbox_send_and_wait(&request);
    va_end(request.args);
}
