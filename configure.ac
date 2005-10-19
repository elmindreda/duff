#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([duff], [0.2], [elmindreda@users.sourceforge.net])
AC_CONFIG_SRCDIR([duff.c])
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS([errno.h stdio.h stdarg.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_MODE_T
AC_TYPE_SIZE_T

# Check for endianness.
AC_C_BIGENDIAN([], [], [AC_MSG_ERROR([Unable to detect endianness.])])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_FUNC_CLOSEDIR_VOID
AC_CHECK_FUNCS([strdup strerror], [], [AC_MSG_ERROR([Function not found])])
AC_CHECK_FUNCS([asprintf vasprintf strdup])

AC_OUTPUT(Makefile)

