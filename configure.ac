 -*- Autoconf -*-

m4_define([tpaf_major_version], [0])
m4_define([tpaf_minor_version], [0])
m4_define([tpaf_patch_version], [1])
m4_define([tpaf_version],[tpaf_major_version.tpaf_minor_version.tpaf_patch_version])

AC_INIT(ttpaf, [tpaf_version], [mattias.ronnblom@ericsson.com])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_PREREQ([2.63])
AC_PROG_CC

AC_SUBST([TPAF_MAJOR_VERSION], [tpaf_major_version])
AC_SUBST([TPAF_MINOR_VERSION], [tpaf_minor_version])
AC_SUBST([TPAF_PATCH_VERSION], [tpaf_patch_version])

AC_USE_SYSTEM_EXTENSIONS

LT_INIT

AC_CHECK_HEADERS(xcm.h, [],
                 [AC_MSG_ERROR([Unable to find XCM header files.])])
AC_CHECK_LIB(xcm, xcm_connect, [],
             [AC_MSG_ERROR([Unable to find the XCM library.])])
AC_CHECK_HEADERS(jansson.h, [],
                 [AC_MSG_ERROR([Unable to find Jansson header files.])])
AC_CHECK_LIB(jansson, json_loads, [],
             [AC_MSG_ERROR([Unable to find the Jansson library.])])
AC_CHECK_HEADERS(event2/event.h, [],
                 [AC_MSG_ERROR([Unable to libevent header files.])])
AC_CHECK_LIB(event, event_base_new, [],
             [AC_MSG_ERROR([Unable to find the libevent library.])])

AC_ARG_ENABLE([valgrind],
    AS_HELP_STRING([--enable-valgrind], [use Valgrind when running tests]))

AS_IF([test "x$enable_valgrind" = "xyes"], [
	AC_CHECK_PROG(valgrind, [],
                 [AC_MSG_ERROR([Unable to find Valgrind executable.])])
])

AM_CONDITIONAL([VALGRIND], [test "x$enable_valgrind" = "xyes"])

AC_SUBST([AM_CFLAGS])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
