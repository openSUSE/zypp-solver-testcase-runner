dnl Process this file with autoconf to produce a configure script.
dnl ==================================================
m4_include([VERSION])
AC_INIT([zypp-testsuite-tools], [LIBZYPPTESTSUITE_MAJOR.LIBZYPPTESTSUITE_MINOR.LIBZYPPTESTSUITE_PATCH])
dnl ==================================================

AC_PREFIX_DEFAULT(/usr)
AC_CANONICAL_TARGET
AC_ARG_PROGRAM

AM_INIT_AUTOMAKE(foreign dist-bzip2 no-dist-gzip tar-ustar)
AM_CONFIG_HEADER(config.h)

dnl ==================================================

CXXFLAGS="${CXXFLAGS} -DFAKE -g -O3 -Wall -Wformat -Woverloaded-virtual -std=c++11"
AC_SYS_LARGEFILE
if test "$enable_largefile" != no ; then
	enable_largefile=yes
	if test "$ac_cv_sys_file_offset_bits" != no; then
		LFS_CFLAGS="-D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
	fi
	if test "$ac_cv_sys_large_files" != no; then
		LFS_CFLAGS="-D_LARGE_FILES=$ac_cv_sys_large_files $LFS_CFLAGS"
	fi
fi
AC_SUBST(LFS_CFLAGS)
test -n "$LFS_CFLAGS" && CXXFLAGS="$CXXFLAGS $LFS_CFLAGS"

dnl ==================================================

zyppdir=${prefix}/share/zypp
localedir=${prefix}/share/locale
testsuitebindir=${prefix}/lib/zypp/testsuite/bin
testsuitedatadir=${zyppdir}/testsuite/data

AC_SUBST(zyppdir)
AC_SUBST(localedir)
AC_SUBST(testsuitebindir)
AC_SUBST(testsuitedatadir)

dnl AH_TEMPLATE([LOCALEDIR],[gettext localedir])
dnl AC_DEFINE_UNQUOTED([LOCALEDIR], ["${localedir}"])
dnl AH_TEMPLATE([TEXTDOMAIN],[gettext textdomain])
dnl AC_DEFINE_UNQUOTED([TEXTDOMAIN], ["$PACKAGE"])

dnl ==================================================
dnl checks for programs

dnl   check for c++ compiler and tools
AC_PROG_CXX
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL
AM_PROG_LIBTOOL

dnl   do AC_CHECK_* tests using c++ (e.g. boost header)
AC_LANG([C++])

dnl ==================================================
dnl checks for typedefs

AH_TEMPLATE([HAVE_RPM_ENHANCES], [the new enhances dependency])
AC_MSG_CHECKING([for Enhances in librpm])
hre=no
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([#include <rpm/rpmlib.h>],
	 	  [int i = RPMTAG_ENHANCESNAME;])],
  [AC_DEFINE([HAVE_RPM_ENHANCES]) hre=yes]
)
AC_MSG_RESULT([$hre])

dnl ==================================================
dnl checks for structures
dnl ==================================================
dnl checks for compiler characteristics
dnl ==================================================
dnl checks for library functions
dnl ==================================================
dnl checks for system services
dnl ==================================================
dnl AC_CONFIG_FILES([FILE...])
AC_OUTPUT(
	Makefile			\
	libzypp-testsuite-tools.spec	\
	libzypp-testsuite-tools.pc	\
	src/Makefile
)
dnl ==================================================

echo "
Configuration:

  PACKAGE:              ${PACKAGE}
  VERSION:              ${VERSION}

  LIBZYPPTESTSUITE_VERSION_INFO: ${LIBZYPPTESTSUITE_VERSION_INFO}

  COMPILER:             ${CXX}
  CXXFLAGS:             ${CXXFLAGS}

  PREFIX:               ${prefix}
  LIBDIR:               ${libdir}

"
echo
