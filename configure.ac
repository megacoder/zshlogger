#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([zshlogger], [1.0.3], [Tommy.Reynolds@MegaCoder.com])
AC_CONFIG_SRCDIR([src/zshlogger.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_INIT_AUTOMAKE

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lncurses':
AC_CHECK_LIB([ncurses], [setupterm])
# FIXME: Replace `main' with a function in `-lreadline':
AC_CHECK_LIB([readline], [readline])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_UID_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([atexit regcomp strchr strdup strerror strncasecmp strrchr])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	zshlogger.spec
])
AC_OUTPUT
