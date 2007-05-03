AC_INIT(Empathy, 0.3)
AC_PREREQ(2.59)
AC_COPYRIGHT([Copyright (C) 2003-2007 Imendio AB])

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-define)

AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC

AM_PROG_LIBTOOL
AM_PATH_GLIB_2_0
AC_PATH_XTRA

AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

IT_PROG_INTLTOOL([0.35.0])

GLIB_REQUIRED=2.12.0
GTK_REQUIRED=2.10.0
GCONF_REQUIRED=1.2.0
LIBGLADE_REQUIRED=2.0.0
TELEPATHY_REQUIRED=0.0.51
MISSION_CONTROL_REQUIRED=4.20

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

IDT_COMPILE_WARNINGS

dnl -----------------------------------------------------------
dnl Pkg-Config dependency checks
dnl -----------------------------------------------------------

PKG_CHECK_MODULES(EMPATHY,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gtk+-2.0 >= $GTK_REQUIRED
   gconf-2.0 >= $GCONF_REQUIRED
   libglade-2.0 >= $LIBGLADE_REQUIRED
   libgnomeui-2.0
   libtelepathy >= $TELEPATHY_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
])

dnl -----------------------------------------------------------
dnl Language Support
dnl -----------------------------------------------------------
GETTEXT_PACKAGE=empathy
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

AM_GLIB_GNU_GETTEXT


dnl -----------------------------------------------------------
AC_OUTPUT([
  Makefile
  po/Makefile.in
  profiles/Makefile
  pixmaps/Makefile
  libempathy/Makefile
  libempathy-gtk/Makefile
  src/Makefile
])

