## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include

bin_PROGRAMS = xdrc/xdrc

xdrc_xdrc_SOURCES = xdrc/xdrc.cc xdrc/gen_hh.cc xdrc/scan.ll	\
	xdrc/parse.yy xdrc/union.h xdrc/xdrc_internal.h
xdrc_xdrc_LDADD =

AM_YFLAGS = -d
BUILT_SOURCES = xdrc/parse.cc xdrc/parse.hh xdrc/scan.cc

lib_LIBRARIES = xmsg/libxmsg.a
xmsg_libxmsg_a_SOURCES = xmsg/iniparse.cc xmsg/msgsock.cc		\
	include/xdrc/msgsock.h xmsg/pollset.cc include/xdrc/pollset.h	\
	include/xdrc/iniparse.h

nodist_pkginclude_HEADERS = include/xdrc/endian.h
pkginclude_HEADERS = include/xdrc/types.h include/xdrc/cereal.h	\
	include/xdrc/marshal.h include/xdrc/printer.h

LDADD = $(top_builddir)/xmsg/libxmsg.a

check_PROGRAMS = tests/test-msgsock tests/test-marshal tests/test-printer
#TESTS = $(check_PROGRAMS)
tests_test_printer_SOURCES = tests/printer.cc xmsg/libxmsg.a
tests_test_msgsock_SOURCES = tests/msgsock.cc xmsg/libxmsg.a
tests_test_marshal_SOURCES = tests/marshal.cc xmsg/libxmsg.a
tests/marshal.o: tests/xdrtest.hh
tests/printer.o: tests/xdrtest.hh

SUFFIXES = .x .hh
.x.hh:
	$(top_builddir)/xdrc/xdrc$(EXEEXT) -hh -o $@ $<
$(top_builddir)/tests/xdrtest.hh: $(top_builddir)/xdrc/xdrc$(EXEEXT)

CLEANFILES = *~ */*~ .gitignore~ tests/xdrtest.hh

$(srcdir)/xdrc/xdrc.1: $(srcdir)/xdrc/xdrc.1.md
	-pandoc -s -w man $(srcdir)/xdrc/xdrc.1.md -o $(srcdir)/xdrc/xdrc.1

distclean-local:
	rm -rf autom4te.cache

maintainer-clean-local:
	+@echo rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in
	rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in

man_MANS = xdrc/xdrc.1
EXTRA_DIST = .gitignore xdrc/xdrc.1 xdrc/xdrc.1.md	\
	include/xdrc/endian.h.in
MAINTAINERCLEANFILES = xdrc/parse.hh xdrc/parse.cc xdrc/scan.cc	\
	$(srcdir)/xdrc/xdrc.1
