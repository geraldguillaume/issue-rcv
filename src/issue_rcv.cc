/*  =========================================================================
    issue_rcv - Main daemon

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
    issue_rcv - Main daemon
@discuss
@end
*/

#include "issue_rcv_classes.h"
#include <fty_log.h>
#include <fty_common_mlm.h>
#define DEFAULT_LOG_CONFIG "/etc/fty/ftylog.cfg"

int main (int argc, char *argv [])
{
    bool verbose = false;
     ManageFtyLog::setInstanceFtylog(ACTOR_NAME);
    int argn;
    int sndhwm=1000;
    int rcvhwm=1000;
    int pipehwm=1000;
    for (argn = 1; argn < argc; argn++) {
        if (streq (argv [argn], "--help")
        ||  streq (argv [argn], "-h")) {
            puts ("issue-rcv [options] ...");
            puts ("  --verbose / -v         verbose test output");
            puts ("  --help / -h            this information");
            puts ("  --hwm / -m             HWM value. 1000 is default");
            return 0;
        }
        else
        if (streq (argv [argn], "--verbose")
        ||  streq (argv [argn], "-v")){
            verbose = true;
            ManageFtyLog::getInstanceFtylog()->setVeboseMode();
        } else if ((streq (argv [argn], "--hwm")
        ||  streq (argv [argn], "-m"))
        && argn<argc-1) {
            argn++;
            int value = atoi(argv [argn]);
            if(value>=0){
                sndhwm=value;
                rcvhwm=value;
                pipehwm=value;
            }
        } else {
            printf ("Unknown option: %s\n", argv [argn]);
            return 1;
        }
    }
    log_info ("issue_rcv - Main daemon HWM=%d",pipehwm);
    zsys_set_sndhwm (sndhwm);
    zsys_set_rcvhwm (rcvhwm);
    zsys_set_pipehwm (pipehwm);
    zactor_t *issue_server_prod_cons = zactor_new(issue_rcv_producer_consumer, (void*)"ACTOR_PROD_CONS");
    zactor_t *issue_server_prod = zactor_new(issue_rcv_producer, (void*)"ACTOR_PROD");
    zactor_t *issue_server_cons1 = zactor_new(issue_rcv_consumer, (void*)"ACTOR_CONS1");

    //zsys_init();
zsys_debug("zsys_pipehwm=%d ", zsys_pipehwm ());


    /*zactor_t *issue_server_cons2 = zactor_new(issue_rcv_consumer, (void*)"ACTOR_CONS2");
    zactor_t *issue_server_cons2 = zactor_new(issue_rcv_consumer, (void*)"ACTOR_CONS2");
    zactor_t *issue_server_cons3 = zactor_new(issue_rcv_consumer, (void*)"ACTOR_CONS3");
    zactor_t *issue_server_cons4 = zactor_new(issue_rcv_consumer, (void*)"ACTOR_CONS4");
    zactor_t *issue_server_cons5 = zactor_new(issue_rcv_consumer, (void*)"ACTOR_CONS5");

    
    ZpollerGuard poller(zpoller_new (issue_server_prod_cons, 
        issue_server_prod,
        issue_server_cons1,
        issue_server_cons2, 
        issue_server_cons3,
        issue_server_cons4,
        issue_server_cons5,
        NULL));
    */
    ZpollerGuard poller(zpoller_new (issue_server_prod_cons, 
        issue_server_prod,
        issue_server_cons1,NULL));

    while (!zsys_interrupted) {
        void *which = zpoller_wait(poller, 10000);
        if (which) {
            char *message = zstr_recv(which);
            if (message) {
                puts(message);
                zstr_free(&message);
            }
        } else {
            if (zpoller_terminated(poller)) {
                break;
            }
        }
     }


    zactor_destroy(&issue_server_prod_cons);
    zactor_destroy(&issue_server_prod);
    zactor_destroy(&issue_server_cons1);

    return EXIT_SUCCESS;
}
