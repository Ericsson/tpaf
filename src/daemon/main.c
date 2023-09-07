/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <event.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcm.h>

#include "log.h"
#include "server.h"
#include "tpaf_version.h"
#include "util.h"

#define DEFAULT_LOG_LEVEL LOG_INFO
#define DEFAULT_LOG_FACILITY LOG_DAEMON
#define DEFAULT_LOG_FLAGS LOG_USE_SYSLOG

static void usage(const char *name)
{
    printf("%s [options] [<domain-addr> ...]\n", name);
    printf("Options:\n");
    printf("  -s             Enable logging to standard error.\n");
    printf("  -n             Disable logging to syslog.\n");
    printf("  -y <facility>  Set syslog facility to use. Default is "
	   "\"%s\".\n", log_facility_to_str(DEFAULT_LOG_FACILITY));
    printf("  -l <level>     Filter levels below <level>. Default is "
	   "\"%s\".\n", log_level_to_str(DEFAULT_LOG_LEVEL));
    printf("  -v             Print version information.\n");
    printf("  -h             Print this text.\n");
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

static char *get_prg_name(const char *prg_path)
{
    const char *prg_name = strrchr(prg_path, '/');

    if (prg_name == NULL)
	return ut_strdup(prg_path);
    else
	return ut_strdup(&prg_name[1]);
}

#define NAME_PER_LINE 8

static void print_name(int value, const char *name, void *cb_data)
{
    int *count = cb_data;

    printf("%-9s", name);

    (*count)++;

    if (*count % NAME_PER_LINE == 0)
	printf("\n");
}

static void print_names(void (*foreach_fun)(log_foreach_cb, void *))
{
    int count = 0;
    foreach_fun(print_name, &count);

    if (count % NAME_PER_LINE != 0)
	printf("\n");
}

int main(int argc, char **argv)
{
    int log_facility = DEFAULT_LOG_FACILITY;
    int log_filter = DEFAULT_LOG_LEVEL;
    unsigned log_flags = DEFAULT_LOG_FLAGS;

    int c;
    while ((c = getopt(argc, argv, "sny:l:vh")) != -1)
	switch (c) {
	case 's':
	    log_flags |= LOG_USE_STDERR;
	    break;
	case 'n':
	    log_flags &= ~LOG_USE_SYSLOG;
	    break;
	case 'y':
	    log_facility = log_str_to_facility(optarg);
	    if (log_facility < 0) {
		fprintf(stderr, "Unknown facility \"%s\". Valid facilities "
			"are:\n", optarg);
		print_names(log_facility_foreach);
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'l':
	    log_filter = log_str_to_level(optarg);
	    if (log_filter < 0) {
		fprintf(stderr, "Unknown filter level \"%s\". Valid levels "
			"are:\n", optarg);
		print_names(log_level_foreach);
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'v':
	    printf("%s\n", TPAF_VERSION);
	    exit(EXIT_SUCCESS);
	    break;
	case 'h':
	    usage(argv[0]);
	    exit(EXIT_SUCCESS);
	    break;
	case '?':
	    exit(EXIT_FAILURE);
	    break;
	}

    char *prg_name = get_prg_name(argv[0]);

    log_init(prg_name, log_filter, log_facility, log_flags);

    ut_free(prg_name);

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

    log_info("tpafd version %s started.", TPAF_VERSION);

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

    log_deinit();

    exit(EXIT_SUCCESS);
}
