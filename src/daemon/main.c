/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <event.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <xcm.h>

#include "log.h"
#include "server.h"
#include "tpaf_version.h"

#define DEFAULT_LOG_LEVEL LOG_INFO

static void usage(const char *name)
{
    printf("%s [options] [<domain-addr> ...]\n", name);
    printf("Options:\n");
    printf("  -s          Enable logging to standard error.\n");
    printf("  -l <level>  Filter levels below <level>. Default is \"%s\".\n",
	   log_level_to_str(DEFAULT_LOG_LEVEL));
    printf("  -h          Show this text.\n");
}

static void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ": %s.\n", strerror(errno));

    va_end(ap);

    exit(EXIT_FAILURE);
}

static void signal_cb(evutil_socket_t fd, short event, void *arg)
{
    struct event_base *event_base = arg;

    event_base_loopbreak(event_base);
}

int main(int argc, char **argv)
{
    bool log_to_stderr = false;
    int log_filter = LOG_INFO;
    int c;
    while ((c = getopt(argc, argv, "sl:h")) != -1)
	switch (c) {
	case 's':
	    log_to_stderr = true;
	    break;
	case 'l':
	    log_filter = log_str_to_level(optarg);
	    if (log_filter < 0) {
		fprintf(stderr, "Unknown filter level \"%s\".\n", optarg);
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'h':
	case '?':
	    usage(argv[0]);
	    exit(EXIT_SUCCESS);
	    break;
	}


    log_init(log_filter, log_to_stderr);

    int num_servers = argc - optind;
    if (num_servers == 0) {
	usage(argv[0]);
	exit(EXIT_FAILURE);
    }

    struct event_base *event_base = event_base_new();

    if (event_base == NULL)
	die("Unable to create event_base");

    struct event sigint_event;
    evsignal_assign(&sigint_event, event_base, SIGINT, signal_cb, event_base);
    evsignal_add(&sigint_event, NULL);

    struct event sighup_event;
    evsignal_assign(&sighup_event, event_base, SIGHUP, signal_cb, event_base);
    evsignal_add(&sighup_event, NULL);

    struct event sigterm_event;
    evsignal_assign(&sigterm_event, event_base, SIGTERM, signal_cb, event_base);
    evsignal_add(&sigterm_event, NULL);

    struct server *servers[num_servers];

    syslog(LOG_INFO, "tpafd version %s started.", TPAF_VERSION);

    int i;

    for (i = 0; i < num_servers; i++) {
	const char *server_addr = argv[optind + i];

	servers[i] = server_create(NULL, server_addr, event_base);

	if (servers[i] == NULL)
	    die("Unable to create server bound to \"%s\"", server_addr);
    }

    for (i = 0; i < num_servers; i++) {
	const char *server_addr = argv[optind + i];

	if (server_start(servers[i]) < 0)
	    die("Unable to start server bound to \"%s\"", server_addr);
    }
    event_base_dispatch(event_base);

    for (i = 0; i < num_servers; i++)
	server_destroy(servers[i]);

    event_base_free(event_base);

    exit(EXIT_SUCCESS);
}
