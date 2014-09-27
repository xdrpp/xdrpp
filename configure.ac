# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([xdrc],[0])
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
AC_PROG_YACC
AC_PROG_CXX([clang++ eg++ g++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC])
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)
test -z "$WFLAGS" || CXXFLAGS="$CXXFLAGS $WFLAGS"
AC_LANG(C++)
# -pthread Seems to be required by g++ -stc=c++1[14]
AX_APPEND_COMPILE_FLAGS([-pthread])

AC_C_BIGENDIAN
AC_C_BIGENDIAN(IS_BIG_ENDIAN=1, IS_BIG_ENDIAN=0)
AC_SUBST(IS_BIG_ENDIAN)

AC_MSG_CHECKING(for cereal in include/cereal)
if test -d include/cereal -o -d "$ac_top_srcdir/cereal"; then
   USE_CEREAL=yes
fi
AM_CONDITIONAL([USE_CEREAL], [test yes = "$USE_CEREAL"])
AC_MSG_RESULT(${USE_CEREAL:-no})

if test -z "$XDRC"; then
   AS_IF([test yes = "$cross_compiling"], [XDRC=xdrc]
         [XDRC='$(top_builddir)/xdrc/xdrc$(EXEEXT)'])
fi
AC_SUBST(XDRC)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile include/xdrc/endian.h])
AC_OUTPUT

