/*  =========================================================================
    issue_rcv_server - Actor

    Copyright (C) 2014 - 2018 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    =========================================================================
*/

/*
@header
    issue_rcv_server - Actor
@discuss
@end
*/

#include "issue_rcv_classes.h"
#include <fty_log.h>
#include <fty_common_mlm.h>

#define STREAM_PRODUCER1 "STREAM1"
#define STREAM_PRODUCER2 "STREAM2"

#define ACTOR_A "produce_consume"
#define ACTOR_B "produce"

#define MSG_COUNT_STREAM1 100000
#define MSG_COUNT_STREAM2_PRELOAD 1000
#define MSG_COUNT_STREAM2 95000



//  Structure of our class

struct _issue_rcv_server_t {
    int filler;     //  Declare class properties here
};


//  --------------------------------------------------------------------------
//  Create a new issue_rcv_server

issue_rcv_server_t *
issue_rcv_server_new (void)
{
    issue_rcv_server_t *self = (issue_rcv_server_t *) malloc (sizeof (issue_rcv_server_t));
    assert (self);
    //  Initialize class properties here
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the issue_rcv_server

void
issue_rcv_server_destroy (issue_rcv_server_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        issue_rcv_server_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

//  --------------------------------------------------------------------------
//  Self test of this class

// If your selftest reads SCMed fixture data, please keep it in
// src/selftest-ro; if your test creates filesystem objects, please
// do so under src/selftest-rw.
// The following pattern is suggested for C selftest code:
//    char *filename = NULL;
//    filename = zsys_sprintf ("%s/%s", SELFTEST_DIR_RO, "mytemplate.file");
//    assert (filename);
//    ... use the "filename" for I/O ...
//    zstr_free (&filename);
// This way the same "filename" variable can be reused for many subtests.
#define SELFTEST_DIR_RO "src/selftest-ro"
#define SELFTEST_DIR_RW "src/selftest-rw"

void
issue_rcv_server_test (bool verbose)
{
    printf (" * issue_rcv_server: ");

    //  @selftest
    //  Simple create/destroy test
    issue_rcv_server_t *self = issue_rcv_server_new ();
    assert (self);
    issue_rcv_server_destroy (&self);
    //  @end
    printf ("OK\n");
}


void
issue_rcv_produce(MlmClientGuard &client, const char *name, const char *stream_name,int nbMsg){
    log_debug("%s Produce %d msg on %s ..",name, nbMsg,stream_name);
    
    for(int n=1;n<=nbMsg;n++){
        zmsg_t *message=zmsg_new();
        for(int c=1;c<=10;c++){
            zmsg_addstrf (message, "message %d - frame %d", n,c);
        }

        mlm_client_send(client,"TEST",&message);
    }
    log_debug("%s Produce %d msg on %s .. DONE",name, nbMsg,stream_name);
}
int total_loop=1;

bool bProduceOnStreamInProgress=false;
void
issue_rcv_producer_consumer (zsock_t *pipe, void *args)
{
    const char *name = static_cast<const char *>(args);

    MlmClientGuard client(mlm_client_new());
    mlm_client_connect(client, MLM_ENDPOINT, 5000, name);
    
    if (mlm_client_set_producer(client, STREAM_PRODUCER1) < 0) {
        log_error("mlm_client_set_producer (stream = '%s') failed",
                STREAM_PRODUCER1);
        return;
    }
    log_info("%s mlm_client_set_producer (stream = '%s') OK",name, STREAM_PRODUCER1);

    if (mlm_client_set_consumer(client, STREAM_PRODUCER2, ".*") < 0) {
        log_error("mlm_client_set_consumer (stream = '%s', pattern = '.*') failed",
                STREAM_PRODUCER2);
        return;
    }
    log_info("%s mlm_client_set_consumer (stream = '%s') OK", name,STREAM_PRODUCER2);
    zsock_signal (pipe, 0);
    log_info("actor %s started", name);

    ZpollerGuard poller(zpoller_new (pipe, mlm_client_msgpipe (client), NULL));
    int count=0;
    uint64_t last = zclock_mono ()-40000;
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, 100);
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted");
                break;
            }
        }
        /*
        uint64_t now = zclock_mono();
        if (now - last >= 35000) {
            bProduceOnStreamInProgress=true;
            issue_rcv_produce(client,name,STREAM_PRODUCER1, MSG_COUNT_STREAM1);
            bProduceOnStreamInProgress=false;
            last=now;

        }
        */
        if (bProduceOnStreamInProgress) {
            log_debug("%s loop %d",name,total_loop);
            issue_rcv_produce(client, name, STREAM_PRODUCER1,MSG_COUNT_STREAM1);
            total_loop++;
            bProduceOnStreamInProgress=false;
        }


        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (message);
            log_debug ("%s: API command=%s", name, cmd);
             if (streq (cmd, "$TERM")) {
                zstr_free (&cmd);
                zmsg_destroy (&message);
                break;
            }
        }

        if (which == mlm_client_msgpipe (client)) {
            zmsg_t *message = mlm_client_recv (client);
            if (!message) {
                log_error ("Given `which == mlm_client_msgpipe (client)`, function `mlm_client_recv ()` returned NULL");
                continue;
            }
            zmsg_destroy (&message);
            count++;
            if(count==MSG_COUNT_STREAM2){
                log_info("%s received %d msg on %s",name, count,STREAM_PRODUCER2);
                count=0;
            }

        } 
        
    } // while (!zsys_interrupted)
    log_info("actor %s stoped", name);
}



void
issue_rcv_producer (zsock_t *pipe, void *args)
{
    const char *name = static_cast<const char *>(args);

    MlmClientGuard client(mlm_client_new());
    mlm_client_connect(client, MLM_ENDPOINT, 5000, name);

    if (mlm_client_set_producer(client, STREAM_PRODUCER2) < 0) {
        log_error("mlm_client_set_producer (stream = '%s') failed",
                STREAM_PRODUCER2);
        return;
    }
    log_info("%s mlm_client_set_producer (stream = '%s') OK",name, STREAM_PRODUCER2);


    ZpollerGuard poller(zpoller_new (pipe, NULL));

    zsock_signal (pipe, 0);
    log_info("actor %s started", name);
    uint64_t last = zclock_mono ()-40000;
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, 1000);
        uint64_t now = zclock_mono();
        if (now - last >= 35000) {
            //preload 1000 msg 
            issue_rcv_produce(client,name,STREAM_PRODUCER2, MSG_COUNT_STREAM2_PRELOAD);
            bProduceOnStreamInProgress=true;
            issue_rcv_produce(client,name,STREAM_PRODUCER2, MSG_COUNT_STREAM2-MSG_COUNT_STREAM2_PRELOAD);
            bProduceOnStreamInProgress=false;
            last=now;

        }
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted");
                break;
            }
        }
        /*
        if (bProduceOnStreamInProgress) {
            issue_rcv_produce(client, name, STREAM_PRODUCER2,MSG_COUNT_STREAM2);
            bProduceOnStreamInProgress=false;
        }
        */


        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (message);
            log_debug ("%s: API command=%s", name, cmd);
             if (streq (cmd, "$TERM")) {
                zstr_free (&cmd);
                zmsg_destroy (&message);
                break;
            }
        }

    } // while (!zsys_interrupted)
    log_info("actor %s stoped", name);
}


void
issue_rcv_consumer (zsock_t *pipe, void *args)
{
    const char *name = static_cast<const char *>(args);

    MlmClientGuard client(mlm_client_new());
    mlm_client_connect(client, MLM_ENDPOINT, 5000, name);
    
    if (mlm_client_set_consumer(client, STREAM_PRODUCER1, ".*") < 0) {
        log_error("mlm_client_set_consumer (stream = '%s') failed",
                STREAM_PRODUCER2);
        return;
    }
    log_info("%s mlm_client_set_consumer (stream = '%s') OK",name, STREAM_PRODUCER1);


    ZpollerGuard poller(zpoller_new (pipe, mlm_client_msgpipe (client), NULL));

    zsock_signal (pipe, 0);
    log_info("actor %s started", name);
    uint64_t last = zclock_mono ();
    int count=0;
    while (!zsys_interrupted) {
        void *which = zpoller_wait (poller, 60000);
        if (which == NULL) {
            if (zpoller_terminated (poller) || zsys_interrupted) {
                log_warning ("zpoller_terminated () or zsys_interrupted");
                break;
            }
            uint64_t now = zclock_mono();
            if (now - last >= 40000) {
                log_error ("No msg received from %s after total_loop=%d",STREAM_PRODUCER1,total_loop);
            }
            continue;
            
        }

        if (which == pipe) {
            zmsg_t *message = zmsg_recv (pipe);
            char *cmd = zmsg_popstr (message);
            log_debug ("%s: API command=%s", name, cmd);
             if (streq (cmd, "$TERM")) {
                zstr_free (&cmd);
                zmsg_destroy (&message);
                break;
            }
        }
        if (which == mlm_client_msgpipe (client)) {
            zmsg_t *message = mlm_client_recv (client);
            if (!message) {
                log_error ("Given `which == mlm_client_msgpipe (client)`, function `mlm_client_recv ()` returned NULL");
                continue;
            }
            zmsg_destroy (&message);
            count++;
            last = zclock_mono();
            if(count==MSG_COUNT_STREAM1){
                log_info("%s received %d msg on %s",name, count,STREAM_PRODUCER1);
                count=0;
            }

        } 

    } // while (!zsys_interrupted)
    log_info("actor %s stoped", name);
}