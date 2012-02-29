#ifndef _MAC_OSX_FORK_H_
#define _MAC_OSX_FORK_H_

#include <mach/mach.h>
#include <mach/task.h>
#include <mach/mach_init.h>
#include <mach/task_info.h>
#include <mach/mach_port.h>

#define CHECK_MACH_ERROR( err, msg ) { if(err) { fprintf(stderr, msg); } }

class memtime_fork : public memtime_fork_base {

public:

static task_t &get_child_task() { static task_t child_task = MACH_PORT_NULL; return child_task; }

static int
setup_recv_port (mach_port_t *recv_port)
{
    kern_return_t       err;
    mach_port_t         port = MACH_PORT_NULL;
    err = mach_port_allocate (mach_task_self (),
                              MACH_PORT_RIGHT_RECEIVE, &port);
    CHECK_MACH_ERROR (err, "mach_port_allocate failed:");

    err = mach_port_insert_right (mach_task_self (),
                                  port,
                                  port,
                                  MACH_MSG_TYPE_MAKE_SEND);
    CHECK_MACH_ERROR (err, "mach_port_insert_right failed:");

    *recv_port = port;
    return 0;
}

static int
send_port (mach_port_t remote_port, mach_port_t port)
{
    kern_return_t       err;

    struct {
        mach_msg_header_t          header;
        mach_msg_body_t            body;
        mach_msg_port_descriptor_t task_port;
    } msg;

    msg.header.msgh_remote_port = remote_port;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_COPY_SEND, 0) |
        MACH_MSGH_BITS_COMPLEX;
    msg.header.msgh_size = sizeof msg;

    msg.body.msgh_descriptor_count = 1;
    msg.task_port.name = port;
    msg.task_port.disposition = MACH_MSG_TYPE_COPY_SEND;
    msg.task_port.type = MACH_MSG_PORT_DESCRIPTOR;

    err = mach_msg_send (&msg.header);
    CHECK_MACH_ERROR (err, "mach_msg_send failed:");

    return 0;
}

static int
recv_port (mach_port_t recv_port, mach_port_t *port)
{
    kern_return_t       err;
    struct {
        mach_msg_header_t          header;
        mach_msg_body_t            body;
        mach_msg_port_descriptor_t task_port;
        mach_msg_trailer_t         trailer;
    } msg;

    err = mach_msg (&msg.header, MACH_RCV_MSG,
                    0, sizeof msg, recv_port,
                    MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    CHECK_MACH_ERROR (err, "mach_msg failed:");

    *port = msg.task_port.name;
    return 0;
}

pid_t native_fork ()
{
    kern_return_t       err;
    mach_port_t         parent_recv_port = MACH_PORT_NULL;
    mach_port_t         child_recv_port = MACH_PORT_NULL;

    if (setup_recv_port (&parent_recv_port) != 0)
        return -1;
    err = task_set_bootstrap_port (mach_task_self (), parent_recv_port);
    CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");

    pid_t               pid;
    switch (pid = fork ()) {
    case -1:
        err = mach_port_deallocate (mach_task_self(), parent_recv_port);
        CHECK_MACH_ERROR (err, "mach_port_deallocate failed:");
        return pid;
    case 0: /* child */
        err = task_get_bootstrap_port (mach_task_self (), &parent_recv_port);
        CHECK_MACH_ERROR (err, "task_get_bootstrap_port failed:");
        if (setup_recv_port (&child_recv_port) != 0)
            return -1;
        if (send_port (parent_recv_port, mach_task_self ()) != 0)
            return -1;
        if (send_port (parent_recv_port, child_recv_port) != 0)
            return -1;
        if (recv_port (child_recv_port, &bootstrap_port) != 0)
            return -1;
        err = task_set_bootstrap_port (mach_task_self (), bootstrap_port);
        CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");
        break;
    default: /* parent */
        err = task_set_bootstrap_port (mach_task_self (), bootstrap_port);
        CHECK_MACH_ERROR (err, "task_set_bootstrap_port failed:");
        if (recv_port (parent_recv_port, &get_child_task()) != 0)
            return -1;
        if (recv_port (parent_recv_port, &child_recv_port) != 0)
            return -1;
        if (send_port (child_recv_port, bootstrap_port) != 0)
            return -1;
        err = mach_port_deallocate (mach_task_self(), parent_recv_port);
        CHECK_MACH_ERROR (err, "mach_port_deallocate failed:");
        break;
    }

    return pid;
}

};

#endif

