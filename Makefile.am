## Process this file with automake to produce Makefile.in
# $Id$

#LDADD = 

bin_PROGRAMS = xdrc

xdrc_SOURCES = xdrc.cc scan.ll parse.yy union.h
#xdrc_SOURCES = genheader.C gencfile.C genpython.C xdrc.cc genpyc.C	\
parse.yy scan.ll rpcc.h xdrc.h
BUILT_SOURCES = parse.cc parse.hh scan.cc
AM_YFLAGS = -d

CLEANFILES = *~ .gitignore~

distclean-local:
	rm -rf autom4te.cache

maintainer-clean-local:
	+@echo rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in
	rm -rf `sed -ne 's!^/!!p' .gitignore` Makefile.in

EXTRA_DIST = .gitignore
MAINTAINERCLEANFILES = parse.hh parse.cc scan.cc
