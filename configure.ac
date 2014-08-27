# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([xdrc],[0])
AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

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

dnl AC_C_BIGENDIAN

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

