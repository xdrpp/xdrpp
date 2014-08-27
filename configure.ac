# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([xdrc],[0])
AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AM_PROG_LEX
AC_PROG_YACC
AC_PROG_CXX([clang++ eg++ g++ c++ gpp aCC CC cxx cc++ cl.exe FCC KCC RCC xlC_r xlC])
AX_CXX_COMPILE_STDCXX_11(noext,mandatory)
CXXFLAGS="$CXXFLAGS -Wall"
dnl AC_USE_SYSTEM_EXTENSIONS
AC_LANG(C++)
# -pthread Seems to be required by g++ -stc=c++11
AX_APPEND_COMPILE_FLAGS([-pthread])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

