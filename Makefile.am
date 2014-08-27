## Process this file with automake to produce Makefile.in
# $Id$

#LDADD = 

bin_PROGRAMS = xdrc

xdrc_SOURCES = xdrc.cc gen_hh.cc scan.ll parse.yy union.h xdrc_internal.h
BUILT_SOURCES = parse.cc parse.hh scan.cc
AM_YFLAGS = -d

nodist_include_HEADERS = xdrc.h

CLEANFILES = *~ .gitignore~

distclean-local:
	rm -rf autom4te.cache

maintainer-clean-local:
	+@echo rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in
	rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in

EXTRA_DIST = .gitignore xdrc.h.in
MAINTAINERCLEANFILES = parse.hh parse.cc scan.cc
