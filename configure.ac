# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([xdrpp],[0])
AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_RANLIB
AM_PROG_AR
AC_PROG_CPP
# gcc does this weird thing where it interprets everything (including
# .x files) as a linker script without the -xc option.
AC_MSG_CHECKING([if C preprocessor can be invoked as $CPP -xc])
if $CPP -xc /dev/null > /dev/null 2>&1; then
   CPP="$CPP -xc"
   AC_MSG_RESULT(yes)
else
   AC_MSG_RESULT(no)
fi
AC_DEFINE_UNQUOTED([CPP_COMMAND], "$CPP", [Shell command for C preprocessor])

test "${WFLAGS+set}" || WFLAGS="-Wall"
AM_PROG_LEX
AC_CHECK_PROGS(YACC, 'bison -y', bison -y)
AC_PROG_CXX([clang++ eg++ g++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC])
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)
test -z "$WFLAGS" || CXXFLAGS="$CXXFLAGS $WFLAGS"
AC_LANG(C++)
# -pthread Seems to be required by g++ -stc=c++1[14]
AX_APPEND_COMPILE_FLAGS([-pthread])

AC_C_BIGENDIAN
AC_C_BIGENDIAN(IS_BIG_ENDIAN=1, IS_BIG_ENDIAN=0)
AC_SUBST(IS_BIG_ENDIAN)

AC_MSG_CHECKING(for cereal)
cereal_CPPFLAGS=
if test -d "$ac_top_srcdir/cereal/include/cereal"; then
   cereal_CPPFLAGS='-I$(top_srcdir)/cereal/include'
   USE_CEREAL=yes
elif test -d cereal/include/cereal; then
   cereal_CPPFLAGS='-I$(top_builddir)/cereal/include'
   USE_CEREAL=yes
fi
AM_CONDITIONAL([USE_CEREAL], [test yes = "$USE_CEREAL"])
AC_MSG_RESULT(${USE_CEREAL:-no})
AC_SUBST(cereal_CPPFLAGS)

test -z "$XDRC" -a "x$cross_compiling" != xyes \
     && XDRC='$(top_builddir)/xdrc/xdrc$(EXEEXT)'
if test -z "$XDRC"; then
   AC_PATH_PROG(XDRC, [xdrc])
   if test -z "$XDRC"; then
      AC_MSG_ERROR(Need native xdrc when cross compiling)
   fi
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile xdrpp/endian.h xdrpp.pc xdrpp-uninstalled.pc])
AC_OUTPUT

