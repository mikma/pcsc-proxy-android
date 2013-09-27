#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_INIT([pcsc-proxy], [2.0.1], [martin@libchipcard.de])
AC_CANONICAL_TARGET([])

AC_CONFIG_SRCDIR([src/common/network.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([])

# Checks for programs.
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL

ACX_PTHREAD

AQ_CHECK_OS

# Checks for libraries.
AQ_CHECK_PCSC


# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h paths.h stdint.h stdlib.h string.h sys/socket.h unistd.h utmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname memmove memset socket strcasecmp strerror utmpname])



###-------------------------------------------------------------------------
#
# check for libgnutls/openssl
#
AC_ARG_ENABLE(openssl,
  [  --enable-openssl                enable openssl (default=no)],
  enable_openssl="$enableval",
  enable_openssl="no")

# Required since the first call to PKG_CHECK_MODULES might not happen
PKG_PROG_PKG_CONFIG
if test "$enable_openssl" = "yes"; then
  PKG_CHECK_MODULES([OPENSSL], [openssl])
else
  PKG_CHECK_MODULES([LIBGNUTLS], [gnutls])
fi
AM_CONDITIONAL([OPENSSL], [test "$enable_openssl" = "yes"])
AM_CONDITIONAL([LIBGNUTLS], [test "$enable_openssl" = "no"])



###-------------------------------------------------------------------------
#
# check for bluetooth
#
AC_ARG_ENABLE(bluetooth,
  [  --enable-bluetooth              enable bluetooth (default=auto)],
  enable_openssl="$enableval",
  enable_openssl="no")

if test ! "$enable_bluetooth" = "no"; then
  PKG_CHECK_MODULES([BLUETOOTH], [bluez],[
    use_bluetooth="yes"
    AC_DEFINE(USE_BLUETOOTH,[1], [bluetooth])
  ],[use_bluetooth="no"])
fi
AM_CONDITIONAL([BLUETOOTH], [test "$use_bluetooth" = "yes"])



###-------------------------------------------------------------------------
#
# GCC version (check for usability)
#
AC_MSG_CHECKING(if symbol visibility is supported)
AC_ARG_ENABLE(visibility,
  [  --enable-visibility             enable visibility (default=no)],
  enable_visibility="$enableval",
  enable_visibility="yes")
AC_MSG_RESULT($enable_visibility)

if test "$enable_visibility" = "yes"; then
  AC_MSG_CHECKING(if symbol visibility is supported)
  visibility_cflags=""
  case `$CC --version|head -n1|cut -f1 -d " "` in 
    gcc*)
       dnl Determine version number (watch out: the outside square
       dnl parenteses are vitally important here!)
       gccversion=["` ${CC} -dumpversion `"]
       case $gccversion in
         4.*)
           # Activate -fvisibility=hidden only for explicit 4.x gcc
           # versions. We never now what future gcc's might do with
           # that argument, and we also can't be sure that other
           # gcc's happen to show very weird version
           # numbers. Therefore leave the default at non-activated.
           visibility_supported="yes, gcc-${gccversion}"
           AC_DEFINE(GCC_WITH_VISIBILITY_ATTRIBUTE, 1, [visibility])
           visibility_cflags="-fvisibility=hidden"
           ;;
         *)
           # Default: -fvisibility=hidden not activated.
           visibility_supported="no (needs gcc >=4.0, you have gcc-${gccversion})"
           ;;
        esac
        ;;
    *)
      visibility_supported="no (needs gcc >=4.0, you use $CC)"
      ;;
  esac
  
  # -fvisibility is at least not available on MinGW/gcc-3.4.4 (will
  # give an "unrecognized cmdline option"). Also, unfortunately I
  # don't know an easy way to ask the compiler here. Maybe
  # http://autoconf-archive.cryp.to/ax_cflags_gcc_option.html
  case "$OS_TYPE" in
    windows)
          visibility_supported="no (not yet on MinGW/Windows)"
          visibility_cflags=""
          ;;
    *)
          ;;
  esac
else
  visibility_supported="no (not wanted)"
  visibility_cflags=""
fi


AC_MSG_RESULT(${visibility_supported})
AC_SUBST(visibility_cflags)


AC_CONFIG_FILES([Makefile
                 m4/Makefile
                 src/Makefile
                 src/client/Makefile
                 src/common/Makefile
                 src/server/Makefile])
AC_OUTPUT
