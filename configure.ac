AC_INIT(Empathy, 0.12, http://bugzilla.gnome.org/browse.cgi?product=empathy)
AC_PREREQ(2.59)
AC_COPYRIGHT([
  Copyright (C) 2003-2007 Imendio AB
  Copyright (C) 2007 Collabora Ltd.
])

AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-define)

AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC

# LT Version numbers, remember to change them just *before* a release.
#   (Interfaces removed:    CURRENT++, AGE=0, REVISION=0)
#   (Interfaces added:      CURRENT++, AGE++, REVISION=0)
#   (No interfaces changed:                   REVISION++)
LIBEMPATHY_CURRENT=2
LIBEMPATHY_AGE=0
LIBEMPATHY_REVISION=0

LIBEMPATHY_GTK_CURRENT=2
LIBEMPATHY_GTK_AGE=1
LIBEMPATHY_GTK_REVISION=0

AC_SUBST(LIBEMPATHY_CURRENT)
AC_SUBST(LIBEMPATHY_AGE)
AC_SUBST(LIBEMPATHY_REVISION)
AC_SUBST(LIBEMPATHY_GTK_CURRENT)
AC_SUBST(LIBEMPATHY_GTK_AGE)
AC_SUBST(LIBEMPATHY_GTK_REVISION)

AM_PROG_LIBTOOL
AM_PATH_GLIB_2_0
AC_PATH_XTRA

AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

IT_PROG_INTLTOOL([0.35.0])

GTK_DOC_CHECK([1.3])

GLIB_REQUIRED=2.12.0
GTK_REQUIRED=2.10.0
GCONF_REQUIRED=1.2.0
LIBGLADE_REQUIRED=2.0.0
TELEPATHY_REQUIRED=0.0.51
MISSION_CONTROL_REQUIRED=4.33

IDT_COMPILE_WARNINGS

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

pluginlibdir=`$PKG_CONFIG mission-control --variable=pluginlibdir`
AC_SUBST(pluginlibdir)

dnl -----------------------------------------------------------
dnl Language Support
dnl -----------------------------------------------------------

GETTEXT_PACKAGE=empathy
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

AM_GLIB_GNU_GETTEXT

dnl -----------------------------------------------------------
dnl Pkg-Config dependency checks
dnl -----------------------------------------------------------

PKG_CHECK_MODULES(LIBEMPATHY,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gconf-2.0 >= $GCONF_REQUIRED
   libxml-2.0
   libtelepathy >= $TELEPATHY_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
])

PKG_CHECK_MODULES(EMPATHY,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gconf-2.0 >= $GCONF_REQUIRED
   libxml-2.0
   gnome-vfs-2.0
   libtelepathy >= $TELEPATHY_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   gtk+-2.0 >= $GTK_REQUIRED
   libglade-2.0 >= $LIBGLADE_REQUIRED
   libgnomeui-2.0
   libebook-1.2
])

dnl -----------------------------------------------------------
dnl ISO codes, used for aspell support
dnl -----------------------------------------------------------
AC_MSG_CHECKING([whether iso-codes exists and has iso-639 domain])
if $PKG_CONFIG --variable=domains iso-codes | grep -q 639 ; then
   AC_MSG_RESULT([yes])
   iso_codes_prefix=`$PKG_CONFIG --variable=prefix iso-codes`
   iso_codes_pkgconfig=iso-codes
   have_iso_codes=yes
else
   AC_MSG_RESULT([no])
   iso_codes_prefix=
   iso_codes_pkgconfig=
   have_iso_codes=no
fi

AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["$iso_codes_prefix"],[ISO codes prefix])

dnl -----------------------------------------------------------
dnl ASpell
dnl -----------------------------------------------------------
AC_ARG_ENABLE(aspell,
              AS_HELP_STRING([--enable-aspell=@<:@no/yes/auto@:>@],
                             [compile with aspell support]), ,
                             enable_aspell=auto)

if test "x$enable_aspell" = "xyes" -a "x$have_iso_codes" != "xyes"; then
   AC_MSG_ERROR([You must have iso-codes with the iso-639 domain to use aspell.])
fi

if test "x$enable_aspell" != "xno" -a "x$have_iso_codes" = "xyes"; then
   AC_CHECK_LIB(aspell, aspell_speller_check,have_aspell=yes,have_aspell=no,)

   if test "x$have_aspell" = "xyes"; then
      AC_DEFINE(HAVE_ASPELL, 1, [Define if we have aspell])
      EMPATHY_LIBS="$EMPATHY_LIBS -laspell"
   else
      have_aspell=no
   fi
else
   have_aspell=no
fi

if test "x$enable_aspell" = "xyes" -a "x$have_aspell" != "xyes"; then
   AC_MSG_ERROR([Couldn't find aspell.])
fi


dnl -----------------------------------------------------------
dnl Python Bindings
dnl -----------------------------------------------------------
AC_ARG_ENABLE(python,
              AS_HELP_STRING([--enable-python=@<:@no/yes/auto@:>@],
                             [build python bindings to libempathy and libempathy-gtk]), ,
                             enable_python=auto)

PKG_CHECK_MODULES(PYTHON_BINDING,
[
   pygtk-2.0,
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gconf-2.0 >= $GCONF_REQUIRED
   libxml-2.0
   gnome-vfs-2.0
   libtelepathy >= $TELEPATHY_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   gtk+-2.0 >= $GTK_REQUIRED
   libglade-2.0 >= $LIBGLADE_REQUIRED
   libgnomeui-2.0
], have_python="yes", have_python="no")

if test "x$have_python" = "xno"; then
	if test "x$enable_python" = "xyes"; then
		AC_MSG_ERROR([Could not find python dependencies])
	fi
	enable_python="no (missing dependencies)"
else
	if test "x$enable_python" != "xno"; then
		enable_python="yes"
	else
		enable_python="no (disabled)"
	fi
fi
AM_CONDITIONAL(HAVE_PYTHON, test x$enable_python = xyes)

dnl -----------------------------------------------------------

AC_OUTPUT([
  Makefile
  data/Makefile
  data/empathy.desktop.in
  data/16x16/Makefile
  data/22x22/Makefile
  data/32x32/Makefile
  data/48x48/Makefile
  data/scalable/Makefile
  po/Makefile.in
  libempathy/Makefile
  libempathy/libempathy.pc
  libempathy-gtk/Makefile
  libempathy-gtk/libempathy-gtk.pc
  src/Makefile
  doc/Makefile
  doc/libempathy/Makefile
  doc/libempathy-gtk/Makefile
  python/Makefile
  python/pyempathy/Makefile
  python/pyempathygtk/Makefile
])
