ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = -g -Wall -Wmissing-prototypes -Werror

AM_CPPFLAGS = -DSYSCONFDIR='"$(sysconfdir)"' \
	-I$(srcdir)/src/util -I$(srcdir)/src/sd -I$(srcdir)/src/proto \
	-I$(builddir)/src/daemon

check_PROGRAMS = tpafd tpaftest

TEST_CPPFLAGS=-I$(srcdir)/test -I$(srcdir)/test/utest

UTIL_SOURCES = src/util/util.c src/util/log.c src/util/plist.c \
	src/util/pqueue.c src/util/slist.c src/util/pmap.c src/util/sbuf.c

#SD_SOURCE_FILES = filter.c props.c pvalue.c
#SD_SOURCES = $(foreach file,$(SD_SOURCE_FILES),src/sd/$(file))
SD_SOURCES = src/sd/flist.c src/sd/filter.c src/sd/props.c \
	src/sd/pvalue.c src/sd/generation.c src/sd/service.c \
	src/sd/sub.c src/sd/db.c src/sd/conn.c src/sd/client.c \
	src/sd/sd_err.c src/sd/sd.c

#UTEST_SOURCE_FILES = utest.c utestreport.c utesthumanreport.c
#UTEST_SOURCES = $(foreach file,$(UTEST_SOURCE_FILES),test/utest/$(file))

TEST_SOURCES = test/utest/utest.c test/utest/utestreport.c \
	test/utest/utesthumanreport.c test/testutil.c

#SD_TC_FILES = value_testcases.c props_testcases.c filter_testcases.c
#SD_TC_SOURCES = $(foreach file,$(SD_TC_FILES),test/$(file))

UTIL_TC_SOURCES = test/util/pqueue_testcases.c test/util/pmap_testcases.c

SD_TC_SOURCES = test/sd/value_testcases.c test/sd/props_testcases.c \
	test/sd/filter_testcases.c test/sd/sd_testcases.c

PROTO_SOURCES = src/proto/msg.c src/proto/proto_ta.c src/proto/proto_conn.c \
	src/proto/server.c

DAEMON_SOURCES = src/daemon/main.c

tpaftest_SOURCES = $(UTIL_SOURCES) $(SD_SOURCES) $(PROTO_SOURCES) \
	$(TEST_SOURCES) $(UTIL_TC_SOURCES) $(SD_TC_SOURCES)
tpaftest_CPPFLAGS = $(AM_CPPFLAGS) $(TEST_CPPFLAGS)
tpaftest_LDFLAGS = -no-install

tpafd_SOURCES = $(UTIL_SOURCES) $(SD_SOURCES) $(PROTO_SOURCES) \
	$(DAEMON_SOURCES)
tpafd_CPPFLAGS = $(AM_CPPFLAGS)

sbin_PROGRAMS = tpafd

distclean-local:
	rm -rf autom4te.cache m4
	rm -f Makefile.in aclocal.m4 compile config.guess include/stamp-h1 \
		config.sub configure depcomp install-sh ltmain.sh missing \
		common/config.h.in
	find . -name \*~ -print0 | xargs -0 rm -f

tpaftest-run: tpaftest
	./tpaftest -v -c $(TESTS)

tpaftest-run-valgrind: tpaftest
	IN_VALGRIND=1 valgrind --tool=memcheck --leak-check=full -q \
		--num-callers=20 ./tpaftest -v -c $(TESTS)

BASIC_TEST_TARGETS=tpaftest-run

if VALGRIND
TEST_TARGETS=tpaftest-run-valgrind $(BASIC_TEST_TARGETS)
else
TEST_TARGETS=$(BASIC_TEST_TARGETS)
endif

check-local: $(TEST_TARGETS)

count:
	@wc -l `git ls-files src | grep -E '\.[ch]{1}$$'`
