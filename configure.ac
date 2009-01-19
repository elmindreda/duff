#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([duff], [0.5], [elmindreda@users.sourceforge.net])
AC_CONFIG_SRCDIR([duff.c])
AM_INIT_AUTOMAKE([check-news dist-bzip2])
AM_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS([assert.h sys/param.h ctype.h errno.h limits.h locale.h stdio.h stdarg.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_OFF_T

# Check for endianness (for sha1-asaddi).
AC_C_BIGENDIAN([], [], [AC_MSG_ERROR([Unable to detect endianness.])])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_FUNC_STAT
AC_FUNC_LSTAT
AC_FUNC_CLOSEDIR_VOID
AC_CHECK_FUNCS([strdup strerror memset strchr strrchr strtoull], \
  [], [AC_MSG_ERROR([Function not found])])
AC_CHECK_FUNCS([asprintf vasprintf])

AC_OUTPUT(Makefile)

