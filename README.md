# issue-rcv 
This code try to simulate the issue on fty-nut agent, where on heavy load stress two Threads (3 & 4) are stucked. They are waiting forever on zmq::mailbox_t::recv.

fty-nut creates one mlm client which both subscribe and consume on different STREAM. One Thread (3) is trying to inform the application that some ASSET has been published in STREAM ASSET, and the other Thread (4) is trying to publish metrics in loop.
~~~~
                "STREAM SEND"+----------+ "STREAM DELIVER"    
mlm_client_send()----------->| msgpipe  |<-------------pass_stream_message_to_app()
Thread4:producer             +----------+              Thread 3:consumer
~~~~


## How to reproduce the issue ?
We suppose that malamute broker is running.
~~~~
./autoconfigure
./configure
make
src/issue-rc 
~~~~
Then it will take several hours before geting stucked. The message "No msg received from STREAM1" noticed it.
On my last experimentation, it tooks 154 loops before to be stucked.

## How to reproduce the issue in a straightforward forward approach ?
If we change the High Water Mark to 1, we will got the issue in just one iteration. WHich show how HWM may influence the issue.
~~~~
./autoconfigure
./configure
make
src/issue-rc -m 1
~~~~
~~~~
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD Produce 1000 msg on STREAM2 ..
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD Produce 1000 msg on STREAM2 .. DONE
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD Produce 94000 msg on STREAM2 ..
issue_rcv [140348084594432] -DEBUG- issue_rcv_producer_consumer (src/issue_rcv_server.cc:175) ACTOR_PROD_CONS loop 1
issue_rcv [140348084594432] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD_CONS Produce 100000 msg on STREAM1 ..
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD Produce 94000 msg on STREAM2 .. DONE
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD Produce 1000 msg on STREAM2 ..
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD Produce 1000 msg on STREAM2 .. DONE
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD Produce 94000 msg on STREAM2 ..
issue_rcv [140348067809024] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD Produce 94000 msg on STREAM2 .. DONE
issue_rcv [140347845895936] -ERROR- issue_rcv_consumer (src/issue_rcv_server.cc:308) No msg received from STREAM1 after total_loop=1
~~~~

## How avoid the issue ?
A first appraoch could be to avoid having a client which both cosume&produce message. But We are not 100% sure that the issue is resolve.

Another approach is to set High Water Mark to "nolimit" (0) and check how it behaves.
~~~~
./autoconfigure
./configure
make
src/issue-rc -m 0
~~~~

This output will loop forver (hopefully).
After several days of endurance, the issue never appear again. so it seems the best approach.
~~~~
issue_rcv [139965669308160] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD Produce 1000 msg on STREAM2 ..
issue_rcv [139965669308160] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD Produce 1000 msg on STREAM2 .. DONE
issue_rcv [139965669308160] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD Produce 94000 msg on STREAM2 ..
issue_rcv [139965895677696] -DEBUG- issue_rcv_producer_consumer (src/issue_rcv_server.cc:175) ACTOR_PROD_CONS loop <X>
issue_rcv [139965895677696] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:114) ACTOR_PROD_CONS Produce 100000 msg on STREAM1 ..
issue_rcv [139965669308160] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD Produce 94000 msg on STREAM2 .. DONE
issue_rcv [139965895677696] -DEBUG- issue_rcv_produce (src/issue_rcv_server.cc:124) ACTOR_PROD_CONS Produce 100000 msg on STREAM1 .. DONE
~~~~



The backtrace of both Thread 3 and Thread 4 is bellow.

## Thread 3 (consumer):
~~~~
#0  0x00007f7b802faaed in poll () at ../sysdeps/unix/syscall-template.S:81
#1  0x00007f7b818e478a in poll (__timeout=-1, __nfds=1, __fds=0x7f7b7e3f2bd0) at /usr/include/x86_64-linux-gnu/bits/poll2.h:46
#2  zmq::signaler_t::wait (this=this@entry=0x7f7b780017a0, timeout_=-1) at src/signaler.cpp:205
#3  0x00007f7b818ce472 in zmq::mailbox_t::recv (this=this@entry=0x7f7b78001740, cmd_=cmd_@entry=0x7f7b7e3f2c40, timeout_=timeout_@entry=-1) at src/mailbox.cpp:70
#4  0x00007f7b818e5954 in zmq::socket_base_t::process_commands (this=this@entry=0x7f7b78001380, timeout_=timeout_@entry=-1, throttle_=throttle_@entry=false)
    at src/socket_base.cpp:975
#5  0x00007f7b818e5e8c in zmq::socket_base_t::send (this=this@entry=0x7f7b78001380, msg_=msg_@entry=0x7f7b7e3f2d90, flags_=flags_@entry=2) at src/socket_base.cpp:821
#6  0x00007f7b818fdf5c in s_sendmsg (s_=0x7f7b78001380, msg_=0x7f7b7e3f2d90, flags_=2) at src/zmq.cpp:336
---Type <return> to continue, or q <return> to quit---
#7  0x00007f7b81b54091 in s_send_string (dest=<optimized out>, more=<optimized out>, string=0x7f7b81ddf1b0 "STREAM DELIVER") at src/zstr.c:45
###src/zstr.c###
          Memory                       Wire
           +-------------+---+          +---+-------------+
    Send   | S t r i n g | 0 |  ---->   | 6 | S t r i n g |
           +-------------+---+          +---+-------------+
static int
s_send_string (void *dest, bool more, char *string)
{
    assert (dest);
    void *handle = zsock_resolve (dest);

    size_t len = strlen (string);
    zmq_msg_t message;
    zmq_msg_init_size (&message, len);
    memcpy (zmq_msg_data (&message), string, len);
line 45- >    if (zmq_sendmsg (handle, &message, more ? ZMQ_SNDMORE : 0) == -1) {
        zmq_msg_close (&message);
        return -1;
    }
    else
        return 0;
}
#8  0x00007f7b81dcda37 in pass_stream_message_to_app (self=<optimized out>, self=<optimized out>) at src/mlm_client.c:289

####src/mlm_client.c####
//  ---------------------------------------------------------------------------
//  pass_stream_message_to_app
//  TODO: these methods could be generated automatically from the protocol
//

static void
pass_stream_message_to_app (client_t *self)
{
#line  289->  zstr_sendm (self->msgpipe, "STREAM DELIVER");
    zsock_bsend (self->msgpipe, "sssp",
                 mlm_proto_address (self->message),
                 mlm_proto_sender (self->message),
                 mlm_proto_subject (self->message),
                 mlm_proto_get_content (self->message));
}

####src/mlm_client.c line 32####
//  This structure defines the context for a client connection
typedef struct {
    //  These properties must always be present in the client_t
    //  and are set by the generated engine. The cmdpipe gets
    //  messages sent to the actor; the msgpipe may be used for
    //  faster asynchronous message flows.
    zsock_t *cmdpipe;           //  Command pipe to/from caller API
    zsock_t *msgpipe;           //  Message pipe to/from caller API
    zsock_t *dealer;            //  Socket to talk to server
    mlm_proto_t *message;       //  Message to/from server
    client_args_t *args;        //  Arguments from methods

    //  Own properties
    char *myaddress;            //  Address of client mailbox
    int heartbeat_timer;        //  Timeout for heartbeats to server
    zlistx_t *replays;          //  Replay server-side state set-up
} client_t;

#9  s_client_execute (self=0x7f7b700008c0, event=set_plain_auth_event) at src/mlm_client_engine.inc:710


###src/mlm_client_engine.inc###
//  Execute state machine as long as we have events; if event is NULL_event,
//  or state machine is stopped, do nothing.

static void
s_client_execute (s_client_t *self, event_t event)
{
...
            case connected_state:
...
                if (self->event == stream_deliver_event) {
                    if (!self->exception) {
                        //  pass stream message to app
                        if (self->verbose)
                            zsys_debug ("%s:         $ pass stream message to app", self->log_prefix);
#line 710 ->            pass_stream_message_to_app (&self->client);
                    }
                }

#10 0x00007f7b81dcdf00 in s_client_handle_protocol (loop=<optimized out>, reader=<optimized out>, argument=0x7f7b700008c0) at src/mlm_client_engine.inc:1450
###src/mlm_client_engine.inc###
//  Handle a message (a protocol reply) from the server

static int
s_client_handle_protocol (zloop_t *loop, zsock_t *reader, void *argument)
{
    s_client_t *self = (s_client_t *) argument;

    //  We will process as many messages as we can, to reduce the overhead
    //  of polling and the reactor:
    while (zsock_events (self->dealer) & ZMQ_POLLIN) {
        if (mlm_proto_recv (self->message, self->dealer))
            return -1;              //  Interrupted; exit zloop

        //  Any input from server counts as activity
        if (self->expiry_timer) {
            zloop_timer_end (self->loop, self->expiry_timer);
            self->expiry_timer = 0;
        }
        //  Reset expiry timer if expiry timeout not zero
        if (self->expiry)
            self->expiry_timer = zloop_timer (
                self->loop, self->expiry, 1, s_client_handle_expiry, self);
line 1450->        s_client_execute (self, s_protocol_event (self, self->message));

#11 0x00007f7b81b42b9d in zloop_start (self=0x7f7b70001b60) at src/zloop.c:818
#12 0x00007f7b81dce6d3 in mlm_client (cmdpipe=0x7f7b78001e80, msgpipe=<optimized out>) at src/mlm_client_engine.inc:1476
//  ---------------------------------------------------------------------------
//  This is the client actor, which polls its two sockets and processes
//  incoming messages

void
mlm_client (zsock_t *cmdpipe, void *msgpipe)
{
    //  Initialize
    s_client_t *self = s_client_new (cmdpipe, (zsock_t *) msgpipe);
    if (self) {
        zsock_signal (cmdpipe, 0);

        //  Set up handler for the sockets the client uses
        engine_handle_socket ((client_t *) self, self->cmdpipe, s_client_handle_cmdpipe);
        engine_handle_socket ((client_t *) self, self->msgpipe, s_client_handle_msgpipe);
        engine_handle_socket ((client_t *) self, self->dealer, s_client_handle_protocol);

        //  Run reactor until there's a termination signal
line 1476->      zloop_start (self->loop);
		
#13 0x00007f7b81b2b593 in s_thread_shim (args=0x7f7b78001e60) at src/zactor.c:67
#14 0x00007f7b805ce064 in start_thread (arg=0x7f7b7e3f3700) at pthread_create.c:309
#15 0x00007f7b8030362d in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:111
~~~~

## Thread 4 (producer):
~~~~
#0  0x00007f7b802faaed in poll () at ../sysdeps/unix/syscall-template.S:81
#1  0x00007f7b818e478a in poll (__timeout=-1, __nfds=1, __fds=0x7f7b7ebf37d0) at /usr/include/x86_64-linux-gnu/bits/poll2.h:46
#2  zmq::signaler_t::wait (this=this@entry=0x7f7b78000d80, timeout_=-1) at src/signaler.cpp:205
#3  0x00007f7b818ce472 in zmq::mailbox_t::recv (this=this@entry=0x7f7b78000d20, cmd_=cmd_@entry=0x7f7b7ebf3840, timeout_=timeout_@entry=-1) at src/mailbox.cpp:70
###libzmq src/mailbox.cpp###
int zmq::mailbox_t::recv (command_t *cmd_, int timeout_)
{
    //  Try to get the command straight away.
    if (active) {
        if (cpipe.read (cmd_))
            return 0;

        //  If there are no more commands available, switch into passive state.
        active = false;
    }

    //  Wait for signal from the command sender.
line 70->    const int rc = signaler.wait (timeout_);
	
#4  0x00007f7b818e5954 in zmq::socket_base_t::process_commands (this=this@entry=0x7f7b78000960, timeout_=timeout_@entry=-1, throttle_=throttle_@entry=false)
    at src/socket_base.cpp:975
###libzmq socket_base.cpp###

int zmq::socket_base_t::process_commands (int timeout_, bool throttle_)
{
    int rc;
    command_t cmd;
    if (timeout_ != 0) {

        //  If we are asked to wait, simply ask mailbox to wait.
line 975->        rc = mailbox.recv (&cmd, timeout_);
    }
	
#5  0x00007f7b818e5e8c in zmq::socket_base_t::send (this=this@entry=0x7f7b78000960, msg_=msg_@entry=0x7f7b7ebf3990, flags_=flags_@entry=2) at src/socket_base.cpp:821
###libzmq socket_base.cpp###
#flags_=flags_@entry=2 => flags= ZMQ_SNDMORE
int zmq::socket_base_t::send (msg_t *msg_, int flags_)
..
    if (flags_ & ZMQ_SNDMORE)
        msg_->set_flags (msg_t::more);
....
    //  Try to send the message.
    rc = xsend (msg_);
	if (rc == 0)
        return 0;
#The mesg is not yet sent	

    //  Compute the time when the timeout should occur.
    //  If the timeout is infinite, don't care.
    int timeout = options.sndtimeo;
#ZMQ_SNDTIMEO,
    uint64_t end = timeout < 0 ? 0 : (clock.now_ms () + timeout);
	
	//  Oops, we couldn't send the message. Wait for the next
    //  command, process it and try to send the message again.
    //  If timeout is reached in the meantime, return EAGAIN.
    while (true) {
line 821->        if (unlikely (process_commands (timeout, false) != 0))
            return -1;
        rc = xsend (msg_);
		
		
	
	
#6  0x00007f7b818fdf5c in s_sendmsg (s_=0x7f7b78000960, msg_=0x7f7b7ebf3990, flags_=2) at src/zmq.cpp:336
###libzmq zmq.cpp###
static int
s_sendmsg (zmq::socket_base_t *s_, zmq_msg_t *msg_, int flags_)
{
    int sz = (int) zmq_msg_size (msg_);
    int rc = s_->send ((zmq::msg_t*) msg_, flags_);
    if (unlikely (rc < 0))
        return -1;
    return sz;
}

#7  0x00007f7b81b54091 in s_send_string (dest=<optimized out>, more=<optimized out>, string=0x7f7b81ddf0c5 "STREAM SEND") at src/zstr.c:45
###czmq src/zstr.c###
          Memory                       Wire
           +-------------+---+          +---+-------------+
    Send   | S t r i n g | 0 |  ---->   | 6 | S t r i n g |
           +-------------+---+          +---+-------------+
static int
s_send_string (void *dest, bool more, char *string)
{
    assert (dest);
    void *handle = zsock_resolve (dest);

    size_t len = strlen (string);
    zmq_msg_t message;
    zmq_msg_init_size (&message, len);
    memcpy (zmq_msg_data (&message), string, len);
line 45- >    if (zmq_sendmsg (handle, &message, more ? ZMQ_SNDMORE : 0) == -1) {
        zmq_msg_close (&message);
        return -1;
    }
    else
        return 0;
}
#8  0x00007f7b81dcec92 in mlm_client_send (self=0x7f7b780008c0, subject=0x7f7b785a42d8 "power.outlet.9@epdu-174", content_p=0x7f7b7ebf3ca0)
    at src/mlm_client_engine.inc:1760


####mlml_client_engine.inc line 1760###
//  ---------------------------------------------------------------------------
//  Send STREAM SEND message to server, takes ownership of message and
//  destroys when done sending it.

int
mlm_client_send (mlm_client_t *self, const char *subject, zmsg_t **content_p)
{
    assert (self);
    //  Send message name as first, separate frame
#line 1760->  zstr_sendm (self->msgpipe, "STREAM SEND");
#https://github.com/zeromq/zyre/issues/500 
    int rc = zsock_bsend (self->msgpipe, "sp", subject, *content_p);
    *content_p = NULL;          //  Take ownership of content
    return rc;
}

####mlml_client_engine.inc line 1491###
//  ---------------------------------------------------------------------------
//  Class interface

struct _mlm_client_t {
    zactor_t *actor;            //  Client actor
    zsock_t *msgpipe;           //  Pipe for async message flow

#9  0x00007f7b8329e4d3 in NUTAgent::send (this=0x7f7b7ebf3dd0, subject="power.outlet.9@epdu-174", message_p=0x7f7b7ebf3ca0) at src/nut_agent.cc:101
#10 0x00007f7b8329f084 in NUTAgent::advertisePhysics (this=this@entry=0x7f7b7ebf3dd0) at src/nut_agent.cc:167
#11 0x00007f7b832a1993 in NUTAgent::onPoll (this=this@entry=0x7f7b7ebf3dd0) at src/nut_agent.cc:84
#12 0x00007f7b832bdd05 in fty_nut_server (pipe=0x109ce60, args=<optimized out>) at src/fty_nut_server.cc:255
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, polling_timeout (timestamp, timeout));
        uint64_t now = zclock_mono();
        if (now - last >= timeout) {
            last = now;
            log_debug("Periodic polling");
            nut_agent.updateDeviceList();
line 255->            nut_agent.onPoll();
        }
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted");
                break;
            }
            if (zpoller_expired (poller)) {
                timestamp = static_cast<uint64_t> (zclock_mono ());
            }
            continue;
        }

        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            if (!message) {
                log_error ("Given `which == pipe`, function `zmsg_recv (pipe)` returned NULL");
                continue;
            }
            if (actor_commands (client, &message, timeout, nut_agent) == 1) {
                break;
            }
            continue;
        }

        // paranoid non-destructive assertion of a twisted mind
        if (which != mlm_client_msgpipe (client)) {
            log_fatal (
                    "zpoller_wait () returned address that is different from "
                    "`pipe`, `mlm_client_msgpipe (client)`, NULL.");
            continue;
        }
#here, we will get STREAM DELIVER .. but never happen, Thread 3 is stuck !!!!
        zmsg_t *message = mlm_client_recv (client);
        if (!message) {
            log_error ("Given `which == mlm_client_msgpipe (client)`, function `mlm_client_recv ()` returned NULL");
            continue;
        }
        if (is_fty_proto(message)) {
            if (state_writer.getState().updateFromProto(message))
                state_writer.commit();
            continue;
        }
        log_error ("Unhandled message (%s/%s)",
                mlm_client_command(client), mlm_client_subject(client));
        zmsg_print (message);
        zmsg_destroy (&message);
    } // while (!zsys_interrupted)
#13 0x00007f7b81b2b593 in s_thread_shim (args=0x109cf60) at src/zactor.c:67
#14 0x00007f7b805ce064 in start_thread (arg=0x7f7b7ebf4700) at pthread_create.c:309
#15 0x00007f7b8030362d in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:111
~~~~