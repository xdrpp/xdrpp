## Process this file with automake to produce Makefile.in

bin_PROGRAMS = xdrc

xdrc_SOURCES = xdrc.cc gen_hh.cc scan.ll parse.yy union.h xdrc_internal.h
BUILT_SOURCES = parse.cc parse.hh scan.cc
AM_YFLAGS = -d

nodist_pkginclude_HEADERS = include/xdrc/endian.h
pkginclude_HEADERS = include/xdrc/types.h include/xdrc/cereal.h

CLEANFILES = *~ .gitignore~

$(srcdir)/xdrc.1: $(srcdir)/xdrc.1.md
	-pandoc -s -w man $(srcdir)/xdrc.1.md -o $(srcdir)/xdrc.1

distclean-local:
	rm -rf autom4te.cache

maintainer-clean-local:
	+@echo rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in
	rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in

man_MANS = xdrc.1
EXTRA_DIST = .gitignore xdrc.1 xdrc.1.md include/xdrc/endian.h.in
MAINTAINERCLEANFILES = parse.hh parse.cc scan.cc xdrc.1
