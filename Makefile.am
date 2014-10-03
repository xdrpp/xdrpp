## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = $(cereal_CPPFLAGS)

bin_PROGRAMS = xdrc/xdrc

xdrc_xdrc_SOURCES = xdrc/xdrc.cc xdrc/gen_hh.cc xdrc/gen_server.cc	\
	xdrc/scan.ll xdrc/parse.yy xdrc/union.h xdrc/xdrc_internal.h
xdrc_xdrc_LDADD =

AM_YFLAGS = -d
BUILT_SOURCES = xdrc/parse.cc xdrc/parse.hh xdrc/scan.cc	\
	xdrpp/rpc_msg.hh xdrpp/rpcbind.hh

lib_LIBRARIES = xdrpp/libxdrpp.a
xdrpp_libxdrpp_a_SOURCES = xdrpp/iniparse.cc xdrpp/marshal.cc	\
	xdrpp/msgsock.cc xdrpp/printer.cc xdrpp/pollset.cc	\
	xdrpp/rpc_msg.cc xdrpp/server.cc xdrpp/socket.cc	\
	xdrpp/srpc.cc

nodist_pkginclude_HEADERS = xdrpp/endian.h
pkginclude_HEADERS = xdrpp/types.h xdrpp/cereal.h xdrpp/clear.h		\
	xdrpp/exception.h xdrpp/iniparse.h xdrpp/marshal.h		\
	xdrpp/printer.h xdrpp/rpc_msg.hh xdrpp/message.h		\
	xdrpp/msgsock.h xdrpp/pollset.h xdrpp/server.h xdrpp/socket.h	\
	xdrpp/srpc.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = xdrpp.pc

LDADD = $(top_builddir)/xdrpp/libxdrpp.a

check_PROGRAMS = tests/test-msgsock tests/test-marshal		\
	tests/test-srpc tests/test-printer tests/test-listener
if USE_CEREAL
check_PROGRAMS += tests/test-cereal
endif
#TESTS = $(check_PROGRAMS)
tests_test_printer_SOURCES = tests/printer.cc
tests_test_msgsock_SOURCES = tests/msgsock.cc
tests_test_marshal_SOURCES = tests/marshal.cc
tests_test_cereal_SOURCES = tests/cereal.cc
tests_test_srpc_SOURCES = tests/srpc.cc
tests_test_listener_SOURCES = tests/listener.cc
tests/marshal.o: tests/xdrtest.hh
tests/printer.o: tests/xdrtest.hh
tests/cereal.o: tests/xdrtest.hh
tests/srpc.o: tests/xdrtest.hh
tests/listener.o: tests/xdrtest.hh

SUFFIXES = .x .hh
.x.hh:
	$(XDRC) -hh -o $@ $<
$(top_builddir)/tests/xdrtest.hh: $(XDRC)
$(top_builddir)/xdrpp/rpc_msg.hh: $(XDRC)
$(top_builddir)/xdrpp/rpcbind.hh: $(XDRC)

CLEANFILES = *~ */*~ */*/*~ .gitignore~ tests/xdrtest.hh	\
	xdrpp/rpc_msg.hh xdrpp/rpcbind.hh

$(srcdir)/doc/xdrc.1: $(srcdir)/doc/xdrc.1.md
	-pandoc -s -w man $(srcdir)/doc/xdrc.1.md -o $(srcdir)/doc/xdrc.1

distclean-local:
	rm -rf autom4te.cache

maintainer-clean-local:
	+@echo rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in
	rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in

man_MANS = doc/xdrc.1
EXTRA_DIST = .gitignore doc/xdrc.1 doc/xdrc.1.md xdrpp/endian.h.in	\
	xdrpp/rpc_msg.x xdrpp/rpcbind.x tests/xdrtest.x			\
	doc/rfc1833.txt doc/rfc4506.txt doc/rfc5531.txt doc/rfc5665.txt
MAINTAINERCLEANFILES = xdrc/parse.hh xdrc/parse.cc xdrc/scan.cc	\
	$(srcdir)/doc/xdrc.1

#xdrpp.pc.in xdrpp-uninstalled.pc.in 

