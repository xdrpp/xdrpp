## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include

bin_PROGRAMS = xdrc/xdrc

xdrc_xdrc_SOURCES = xdrc/xdrc.cc xdrc/gen_hh.cc xdrc/gen_server.cc	\
	xdrc/scan.ll xdrc/parse.yy xdrc/union.h xdrc/xdrc_internal.h
xdrc_xdrc_LDADD =

AM_YFLAGS = -d
BUILT_SOURCES = xdrc/parse.cc xdrc/parse.hh xdrc/scan.cc	\
	include/xdrc/rpc_msg.hh include/xdrc/rpcbind.hh

lib_LIBRARIES = xmsg/libxmsg.a
xmsg_libxmsg_a_SOURCES = xmsg/iniparse.cc xmsg/marshal.cc	\
	xmsg/msgsock.cc xmsg/printer.cc xmsg/pollset.cc		\
	xmsg/rpc_msg.cc xmsg/srpc.cc

nodist_pkginclude_HEADERS = include/xdrc/endian.h
pkginclude_HEADERS = include/xdrc/types.h include/xdrc/cereal.h	\
	include/xdrc/clear.h include/xdrc/exception.h		\
	include/xdrc/iniparse.h include/xdrc/marshal.h		\
	include/xdrc/printer.h include/xdrc/rpc_msg.hh		\
	include/xdrc/message.h include/xdrc/msgsock.h		\
	include/xdrc/pollset.h include/xdrc/srpc.h

LDADD = $(top_builddir)/xmsg/libxmsg.a

check_PROGRAMS = tests/test-msgsock tests/test-marshal	\
	tests/test-srpc tests/test-printer
if USE_CEREAL
check_PROGRAMS += tests/test-cereal
endif
#TESTS = $(check_PROGRAMS)
tests_test_printer_SOURCES = tests/printer.cc xmsg/libxmsg.a
tests_test_msgsock_SOURCES = tests/msgsock.cc xmsg/libxmsg.a
tests_test_marshal_SOURCES = tests/marshal.cc xmsg/libxmsg.a
tests_test_cereal_SOURCES = tests/cereal.cc xmsg/libxmsg.a
tests_test_srpc_SOURCES = tests/srpc.cc xmsg/libxmsg.a
tests/marshal.o: tests/xdrtest.hh
tests/printer.o: tests/xdrtest.hh
tests/cereal.o: tests/xdrtest.hh
tests/srpc.o: tests/xdrtest.hh

SUFFIXES = .x .hh
.x.hh:
	$(XDRC) -hh -o $@ $<
$(top_builddir)/tests/xdrtest.hh: $(XDRC)
$(top_builddir)/include/xdrc/rpc_msg.hh: $(XDRC)

CLEANFILES = *~ */*~ */*/*~ .gitignore~ tests/xdrtest.hh include/xdrc/rpc_msg.hh

$(srcdir)/xdrc/xdrc.1: $(srcdir)/xdrc/xdrc.1.md
	-pandoc -s -w man $(srcdir)/xdrc/xdrc.1.md -o $(srcdir)/xdrc/xdrc.1

distclean-local:
	rm -rf autom4te.cache

maintainer-clean-local:
	+@echo rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in
	rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in

man_MANS = xdrc/xdrc.1
EXTRA_DIST = .gitignore xdrc/xdrc.1 xdrc/xdrc.1.md	\
	include/xdrc/endian.h.in include/xdrc/rpc_msg.x	\
	include/xdrc/rpcbind.x tests/xdrtest.x
MAINTAINERCLEANFILES = xdrc/parse.hh xdrc/parse.cc xdrc/scan.cc	\
	$(srcdir)/xdrc/xdrc.1
