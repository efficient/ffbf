#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([ffbf], [0.9], [imoraru+@cs.cmu.edu])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/rabin-karp/ffbf.cpp])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h sys/param.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero getcwd gettimeofday memset munmap])

#AC_CONFIG_FILES([src/gpgpu/Makefile
#                 src/hashes/Makefile
#                 src/rabin-karp/Makefile])

CXXFLAGS="$CXXFLAGS -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable"

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/hashes/Makefile
                 src/rabin-karp/Makefile])

AC_OUTPUT
