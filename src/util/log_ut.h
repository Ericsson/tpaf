/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#ifndef LOG_UT_H
#define LOG_UT_H

#include "log.h"

#define log_ut_mem_exhausted()			\
    log_error("Error allocating heap memory.")

#define log_ut_urandom_failure(op_name, op_errno)		       \
    log_error(NULL, "Error %s /dev/urandom; errno %d (%s).",        \
	      op_name, op_errno, strerror(op_errno))

#define log_ut_urandom_open_failure(op_errno)	\
    log_ut_urandom_failure("opening", op_errno)

#define log_ut_urandom_read_failure(op_errno)	\
    log_ut_urandom_failure("reading", op_errno)

#endif
