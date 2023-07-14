dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(tlock,
        m4_esyscmd_s(echo $(git describe --tags|sed -e's/-/.p/' -e's/-g.*//')))

AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_MAKE_SET
AC_SUBST([LIBTOOL_DEPS])

AC_CONFIG_FILES(Makefile src/Makefile tlock.spec)
AC_OUTPUT

