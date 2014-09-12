## Process this file with automake to produce Makefile.in

bin_PROGRAMS = xdrc/xdrc

xdrc_xdrc_SOURCES = xdrc/xdrc.cc xdrc/gen_hh.cc xdrc/scan.ll	\
	xdrc/parse.yy xdrc/union.h xdrc/xdrc_internal.h
BUILT_SOURCES = xdrc/parse.cc xdrc/parse.hh xdrc/scan.cc
AM_YFLAGS = -d

nodist_pkginclude_HEADERS = include/xdrc/endian.h
pkginclude_HEADERS = include/xdrc/types.h include/xdrc/cereal.h

CLEANFILES = *~ .gitignore~

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
