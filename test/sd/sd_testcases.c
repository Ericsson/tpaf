/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Ericsson AB
 */

#include "utest.h"

#include <event2/event.h>
#include <inttypes.h>

#include "testutil.h"
#include "util.h"

#include "sd.h"

#define CHKNOSDERR(x)							\
    do {                                                                \
	int64_t err;							\
	if ((err = (x)) < 0) {                                          \
	    fprintf(stderr, "\n%s:%d: Unexpected error return code: %"	\
		    PRIu64 " [%s]\n", __FILE__, __LINE__, err,		\
		    sd_str_error(err));					\
	    return UTEST_FAILED;                                          \
	}                                                               \
    } while(0)

static struct event_base *event_base;
static struct sd *sd;

static int setup(unsigned setup_flags)
{
    event_base = event_base_new();

    CHK(event_base != NULL);

    sd = sd_create(event_base);

    CHK(sd != NULL);

    return UTEST_SUCCESS;
}

static int teardown(unsigned setup_flags)
{
    sd_destroy(sd);

    event_base_free(event_base);

    return UTEST_SUCCESS;
}

TESTSUITE(sd, setup, teardown)

TESTCASE(sd, connect_disconnect)
{
    int64_t client_id_0 = 99;
    int64_t client_id_1 = 42;

    CHKNOSDERR(sd_client_connect(sd, client_id_0, "ux:foo"));

    CHKNOSDERR(sd_client_connect(sd, client_id_1, "ux:bar"));

    CHKNOSDERR(sd_client_disconnect(sd, client_id_1));
    
    CHKINTEQ(sd_client_disconnect(sd, client_id_1), SD_ERR_NO_SUCH_CLIENT);

    return UTEST_SUCCESS;
}

static void run_loop(double timeout)
{
    struct timeval tmo;
    tu_f_to_timeval(timeout, &tmo);

    event_base_loopexit(event_base, &tmo);

    event_base_dispatch(event_base);
}

struct record_match
{
    enum sub_match_type match_type;
    const struct service *service;
};

static void record_match_cb(struct sub *sub, const struct service *service,
			    enum sub_match_type match_type,
			    void *cb_data)
{
    struct record_match *m = cb_data;
    m->service = service;
    m->match_type = match_type;
}

TESTCASE(sd, publish_subscribe)
{
    int64_t pub_client_id = 99;

    CHKNOSDERR(sd_client_connect(sd, pub_client_id, "ux:asdf"));

    int64_t service_id = 4444;
    int64_t generation = 44;
    struct props *props = props_create();
    props_add_int64(props, "x", 17);
    int64_t ttl = 1;

    CHKNOSDERR(sd_publish(sd, pub_client_id, service_id, generation,
			  props, ttl));

    int64_t sub_client_id = 100;

    CHKNOSDERR(sd_client_connect(sd, sub_client_id, "ux:foo"));

    struct record_match match = {};
    int64_t sub_id = 1234;
    CHKNOSDERR(sd_create_sub(sd, sub_client_id, sub_id, "(x=17)",
			     record_match_cb, &match));

    sd_activate_sub(sd, sub_client_id, sub_id);

    CHKINTEQ(match.match_type, sub_match_type_appeared);

    CHK(service_get_id(match.service) == service_id);
    CHK(service_get_generation(match.service) == generation);
    CHK(props_equal(service_get_props(match.service), props));
    CHK(service_get_ttl(match.service) == ttl);
    CHK(!service_is_orphan(match.service));

    match = (struct record_match) { };

    double before_disconnect = ut_ftime();
    tu_fsleep(0.1);

    CHKNOSDERR(sd_client_disconnect(sd, pub_client_id));

    CHKINTEQ(match.match_type, sub_match_type_modified);

    CHK(service_get_id(match.service) == service_id);
    CHK(service_is_orphan(match.service));

    tu_fsleep(0.1);
    double after_disconnect = ut_ftime();
    double orphan_since = service_get_orphan_since(match.service);

    CHK(orphan_since > 0);
    CHK(orphan_since > before_disconnect);
    CHK(orphan_since < after_disconnect);

    run_loop(2.0);

    CHKINTEQ(match.match_type, sub_match_type_disappeared);

    CHKNOSDERR(sd_client_disconnect(sd, sub_client_id));

    props_destroy(props);

    return UTEST_SUCCESS;
}

static int run_subscribe_unsubscribe(int64_t client_id, bool disconnect)
{
    CHKNOSDERR(sd_client_connect(sd, client_id, "tcp:1.2.3.4:5555"));

    int num_subscriptions = 1 + tu_rand_max(100);

    struct record_match match = {};

    int i;
    for (i = 0; i < num_subscriptions; i++) {
	int64_t sub_id = i;

	CHKNOSDERR(sd_create_sub(sd, client_id, sub_id, "(&(a=17)(b=99))",
				 record_match_cb, &match));
	sd_activate_sub(sd, client_id, sub_id);
    }

    run_loop(0.1);

    for (i = 0; i < num_subscriptions; i++) {
	int64_t sub_id = i;
	CHKNOSDERR(sd_unsubscribe(sd, client_id, sub_id));
    }

    run_loop(0.1);

    if (disconnect)
	CHKNOSDERR(sd_client_disconnect(sd, client_id));

    return UTEST_SUCCESS;
}

TESTCASE(sd, subscribe_unsubscribe)
{
    int64_t client_id_0 = 99;
    int64_t client_id_1 = 4711;

    if (run_subscribe_unsubscribe(client_id_0, true) != UTEST_SUCCESS)
	return UTEST_FAILED;

    if (run_subscribe_unsubscribe(client_id_1, false) != UTEST_SUCCESS)
	return UTEST_FAILED;

    return UTEST_SUCCESS;
}
