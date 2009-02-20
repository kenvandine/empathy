AC_INIT(Empathy, 2.25.92, http://bugzilla.gnome.org/browse.cgi?product=empathy)
AC_PREREQ(2.59)
AC_COPYRIGHT([
  Copyright (C) 2003-2007 Imendio AB
  Copyright (C) 2007-2008 Collabora Ltd.
])

# LT Version numbers, remember to change them just *before* a release.
#   (Interfaces removed:    CURRENT++, AGE=0, REVISION=0)
#   (Interfaces added:      CURRENT++, AGE++, REVISION=0)
#   (No interfaces changed:                   REVISION++)
LIBEMPATHY_CURRENT=21
LIBEMPATHY_AGE=0
LIBEMPATHY_REVISION=0

LIBEMPATHY_GTK_CURRENT=20
LIBEMPATHY_GTK_AGE=1
LIBEMPATHY_GTK_REVISION=0

AC_SUBST(LIBEMPATHY_CURRENT)
AC_SUBST(LIBEMPATHY_AGE)
AC_SUBST(LIBEMPATHY_REVISION)
AC_SUBST(LIBEMPATHY_GTK_CURRENT)
AC_SUBST(LIBEMPATHY_GTK_AGE)
AC_SUBST(LIBEMPATHY_GTK_REVISION)

# Minimal version required
GLIB_REQUIRED=2.16.0
GTK_REQUIRED=2.14.0
GCONF_REQUIRED=1.2.0
LIBGLADE_REQUIRED=2.0.0
LIBPANELAPPLET_REQUIRED=2.10.0
TELEPATHY_GLIB_REQUIRED=0.7.23
MISSION_CONTROL_REQUIRED=4.61
ENCHANT_REQUIRED=1.2.0
ISO_CODES_REQUIRED=0.35
LIBNOTIFY_REQUIRED=0.4.4

# Uncomment that to build with deprecated symbols disabled
#AC_DEFINE(G_DISABLE_DEPRECATED, [], [Disable deprecated GLib symbols])
#AC_DEFINE(GDK_PIXBUF_DISABLE_DEPRECATED, [], [Disable deprecated GDK pixbuf symbols])
#AC_DEFINE(GDK_DISABLE_DEPRECATED, [], [Disable deprecated GDK symbols])
#AC_DEFINE(GTK_DISABLE_DEPRECATED, [], [Disable deprecated GTK symbols])

# Uncomment that to build with single include disabled
#AC_DEFINE(G_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GLib])
#AC_DEFINE(GDK_PIXBUF_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GDK pixbuf])
#AC_DEFINE(GTK_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GTK])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR(.)

GNOME_COMMON_INIT

AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-define -Wno-portability)
AM_MAINTAINER_MODE
AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL
AM_PROG_MKDIR_P
AM_PATH_GLIB_2_0
AC_PATH_XTRA
IT_PROG_INTLTOOL([0.35.0])
GTK_DOC_CHECK([1.3])
GNOME_DOC_INIT
IDT_COMPILE_WARNINGS
AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2
GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

AC_CHECK_PROGS([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc (from libxslt) is required])
fi
AM_PATH_PYTHON([2.3])

EMPATHY_ARG_VALGRIND

# -----------------------------------------------------------
# Pkg-Config dependency checks
# -----------------------------------------------------------

PKG_CHECK_MODULES(LIBEMPATHY,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   libxml-2.0
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   gio-2.0 >= $GLIB_REQUIRED
   gio-unix-2.0 >= $GLIB_REQUIRED
   telepathy-farsight
])

PKG_CHECK_MODULES(EMPATHY,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gio-2.0 >= $GLIB_REQUIRED
   gconf-2.0 >= $GCONF_REQUIRED
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   gtk+-2.0 >= $GTK_REQUIRED
   x11
   libglade-2.0 >= $LIBGLADE_REQUIRED
   libebook-1.2
   libcanberra-gtk >= 0.4
   gstreamer-0.10
   gstreamer-interfaces-0.10
   telepathy-farsight
])

PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED)

# -----------------------------------------------------------
# Enable debug
# -----------------------------------------------------------

AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code]),
    enable_debug=$enableval, enable_debug=yes )

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi

# -----------------------------------------------------------
# Language Support
# -----------------------------------------------------------

GETTEXT_PACKAGE=empathy
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])

AM_GLIB_GNU_GETTEXT

# -----------------------------------------------------------
# spellchecking checks: enchant and iso-codes
# -----------------------------------------------------------
AC_ARG_ENABLE(spell,
              AS_HELP_STRING([--enable-spell=@<:@no/yes/auto@:>@],
                             [Enable spell checking]), ,
                             enable_spell=auto)

if test "x$enable_spell" != "xno"; then
   PKG_CHECK_MODULES(ENCHANT,
   [
      enchant >= $ENCHANT_REQUIRED,
      iso-codes >= $ISO_CODES_REQUIRED
   ], have_enchant="yes", have_enchant="no")

   if test "x$have_enchant" = "xyes"; then
      AC_MSG_CHECKING([whether iso-codes has iso-639 and iso-3166 domains])
      if $PKG_CONFIG --variable=domains iso-codes | grep 639 > /dev/null && \
         $PKG_CONFIG --variable=domains iso-codes | grep 3166 > /dev/null ; then
            AC_DEFINE_UNQUOTED(ISO_CODES_PREFIX, ["`$PKG_CONFIG --variable=prefix iso-codes`"], [ISO codes prefix])
            AC_DEFINE(HAVE_ENCHANT, [], [Define if you have libenchant])
            result=yes
      else
            result=no
            have_enchant="no"
      fi
      AC_MSG_RESULT([$result])
   fi
else
   have_enchant=no
fi

if test "x$enable_spell" = "xyes" -a "x$have_enchant" != "xyes"; then
   AC_MSG_ERROR([Couldn't find spell dependencies.])
fi

AM_CONDITIONAL(HAVE_ENCHANT, test "x$have_enchant" = "xyes")

# -----------------------------------------------------------
# Megaphone
# -----------------------------------------------------------
AC_ARG_ENABLE(megaphone,
              AS_HELP_STRING([--enable-megaphone=@<:@no/yes/auto@:>@],
                             [build megaphone applet]), ,
                             enable_megaphone=auto)

if test "x$enable_megaphone" != "xno"; then
   PKG_CHECK_MODULES(MEGAPHONE, 
   [
      libpanelapplet-2.0 >= $LIBPANELAPPLET_REQUIRED
      glib-2.0 >= $GLIB_REQUIRED
      gtk+-2.0 >= $GTK_REQUIRED
      gconf-2.0 >= $GCONF_REQUIRED
      libglade-2.0 >= $LIBGLADE_REQUIRED 
      libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   ], have_megaphone="yes", have_megaphone="no")
else
   have_megaphone=no
fi

if test "x$enable_megaphone" = "xyes" -a "x$have_megaphone" != "xyes"; then
   AC_MSG_ERROR([Couldn't find megaphone dependencies.])
fi

AM_CONDITIONAL(HAVE_MEGAPHONE, test "x$have_megaphone" = "xyes")

# -----------------------------------------------------------
# Nothere
# -----------------------------------------------------------
AC_ARG_ENABLE(nothere,
              AS_HELP_STRING([--enable-nothere=@<:@no/yes/auto@:>@],
                             [build nothere applet]), ,
                             enable_nothere=auto)

if test "x$enable_nothere" != "xno"; then
   PKG_CHECK_MODULES(NOTHERE, 
   [
      libpanelapplet-2.0 >= $LIBPANELAPPLET_REQUIRED
      glib-2.0 >= $GLIB_REQUIRED
      gtk+-2.0 >= $GTK_REQUIRED
      gconf-2.0 >= $GCONF_REQUIRED
      libglade-2.0 >= $LIBGLADE_REQUIRED 
      libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   ], have_nothere="yes", have_nothere="no")
else
   have_nothere=no
fi

if test "x$enable_nothere" = "xyes" -a "x$have_nothere" != "xyes"; then
   AC_MSG_ERROR([Couldn't find nothere dependencies.])
fi

AM_CONDITIONAL(HAVE_NOTHERE, test "x$have_nothere" = "xyes")

# -----------------------------------------------------------
# Tests
# -----------------------------------------------------------
AC_ARG_ENABLE(tests,
              AS_HELP_STRING([--enable-tests=@<:@no/yes/auto@:>@],
                             [build tests]), ,
                             enable_tests=auto)

if test "x$enable_tests" != "xno"; then
   PKG_CHECK_MODULES(CHECK,
   [
      check >= 0.9.4
   ], have_check="yes", have_check="no")
else
   have_check=no
fi

if test "x$enable_tests" = "xyes" -a "x$have_check" != "xyes"; then
   AC_MSG_ERROR([Couldn't find check dependencies.])
fi

AM_CONDITIONAL(HAVE_TESTS, test "x$have_check" = "xyes")

# -----------------------------------------------------------
# Python Bindings
# -----------------------------------------------------------
AC_ARG_ENABLE(python,
              AS_HELP_STRING([--enable-python=@<:@no/yes/auto@:>@],
                             [build python bindings to libempathy and libempathy-gtk]), ,
                             enable_python=auto)

if test "x$enable_python" != "xno"; then
   PKG_CHECK_MODULES(PYTHON_BINDING,
   [
      pygtk-2.0,
      glib-2.0 >= $GLIB_REQUIRED
      gobject-2.0
      gconf-2.0 >= $GCONF_REQUIRED
      libxml-2.0
      libmissioncontrol >= $MISSION_CONTROL_REQUIRED
      gtk+-2.0 >= $GTK_REQUIRED
      libglade-2.0 >= $LIBGLADE_REQUIRED
      gstreamer-0.10
      gstreamer-interfaces-0.10
      telepathy-farsight
   ], have_python="yes", have_python="no")
   if test "x$have_python" = "xyes" ; then
      AM_CHECK_PYTHON_HEADERS(,have_python="no")
      AC_CHECK_PROGS([PYGOBJECTCODEGEN], [pygobject-codegen-2.0 pygtk-codegen-2.0])
   fi
else
   have_python=no
fi

if test "x$enable_python" = "xyes" -a "x$have_python" != "xyes"; then
   AC_MSG_ERROR([Couldn't find python.])
fi

AM_CONDITIONAL(HAVE_PYTHON, test "x$have_python" = "xyes")

# -----------------------------------------------------------

AC_OUTPUT([
   Makefile
   data/Makefile
   data/empathy.desktop.in
   data/icons/Makefile
   extensions/Makefile
   po/Makefile.in
   libempathy/Makefile
   libempathy/libempathy.pc
   libempathy-gtk/Makefile
   libempathy-gtk/libempathy-gtk.pc
   src/Makefile
   megaphone/Makefile
   megaphone/src/Makefile
   megaphone/data/Makefile
   nothere/Makefile
   nothere/src/Makefile
   nothere/data/Makefile
   docs/Makefile
   docs/libempathy/Makefile
   docs/libempathy/version.xml
   docs/libempathy-gtk/Makefile
   docs/libempathy-gtk/version.xml
   help/Makefile
   python/Makefile
   python/pyempathy/Makefile
   python/pyempathygtk/Makefile
   tests/Makefile
   tests/xml/Makefile
   tools/Makefile
])
