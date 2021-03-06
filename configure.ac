AC_INIT(Empathy, 2.27.4, http://bugzilla.gnome.org/browse.cgi?product=empathy)
AC_PREREQ(2.59)
AC_COPYRIGHT([
  Copyright (C) 2003-2007 Imendio AB
  Copyright (C) 2007-2008 Collabora Ltd.
])

# LT Version numbers, remember to change them just *before* a release.
#   (Interfaces removed:    CURRENT++, AGE=0, REVISION=0)
#   (Interfaces added:      CURRENT++, AGE++, REVISION=0)
#   (No interfaces changed:                   REVISION++)
LIBEMPATHY_CURRENT=26
LIBEMPATHY_AGE=0
LIBEMPATHY_REVISION=0

LIBEMPATHY_GTK_CURRENT=24
LIBEMPATHY_GTK_AGE=0
LIBEMPATHY_GTK_REVISION=0

AC_SUBST(LIBEMPATHY_CURRENT)
AC_SUBST(LIBEMPATHY_AGE)
AC_SUBST(LIBEMPATHY_REVISION)
AC_SUBST(LIBEMPATHY_GTK_CURRENT)
AC_SUBST(LIBEMPATHY_GTK_AGE)
AC_SUBST(LIBEMPATHY_GTK_REVISION)

# Minimal version required
GLIB_REQUIRED=2.16.0
GTK_REQUIRED=2.16.0
GCONF_REQUIRED=1.2.0
LIBPANELAPPLET_REQUIRED=2.10.0
TELEPATHY_GLIB_REQUIRED=0.7.31
MISSION_CONTROL_REQUIRED=4.61
ENCHANT_REQUIRED=1.2.0
ISO_CODES_REQUIRED=0.35
LIBNOTIFY_REQUIRED=0.4.4
LIBCANBERRA_GTK_REQUIRED=0.4
LIBCHAMPLAIN_REQUIRED=0.3.3
LIBINDICATE_REQUIRED=0.1.6
LIBCHAMPLAIN_GTK_REQUIRED=0.3.3
CLUTTER_GTK_REQUIRED=0.8.2
GEOCLUE_REQUIRED=0.11
WEBKIT_REQUIRED=1.1.7
NETWORK_MANAGER_REQUIRED=0.7.0

# Use --enable-maintainer-mode to disabled deprecated symbols
GNOME_MAINTAINER_MODE_DEFINES

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
   gio-2.0 >= $GLIB_REQUIRED
   gio-unix-2.0 >= $GLIB_REQUIRED
   libxml-2.0
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   telepathy-farsight
   farsight2-0.10
   gstreamer-0.10
])

PKG_CHECK_MODULES(LIBEMPATHYGTK,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gio-2.0 >= $GLIB_REQUIRED
   gconf-2.0 >= $GCONF_REQUIRED
   x11
   gtk+-2.0 >= $GTK_REQUIRED
   libcanberra-gtk >= $LIBCANBERRA_GTK_REQUIRED
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   farsight2-0.10
   gstreamer-0.10
   gstreamer-interfaces-0.10
])

PKG_CHECK_MODULES(EMPATHY,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   gio-2.0 >= $GLIB_REQUIRED
   gdk-x11-2.0
   gtk+-2.0 >= $GTK_REQUIRED
   libebook-1.2
   dbus-glib-1
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
   libmissioncontrol >= $MISSION_CONTROL_REQUIRED
   telepathy-farsight
   gstreamer-0.10
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
# NM integration
# -----------------------------------------------------------
AC_ARG_ENABLE(network-manager,
              AS_HELP_STRING([--enable-network-manager=@<:@no/yes/auto@:>@],
                             [build with network-manager support]), ,
                             enable_webkit=auto)

if test "x$enable_network_manager" != "xno"; then
   PKG_CHECK_MODULES(NETWORK_MANAGER,
   [
      libnm_glib >= $NETWORK_MANAGER_REQUIRED
   ], have_nm="yes", have_nm="no")

   if test "x$have_nm" = "xyes"; then
      AC_DEFINE(HAVE_NM, 1, [Define if you have libnm-glib])
   fi
else
   have_nm=no
fi

if test "x$enable_network_manager" = "xyes" -a "x$have_nm" != "xyes"; then
   AC_MSG_ERROR([Couldn't find libnm-glib dependencies.])
fi
AM_CONDITIONAL(HAVE_NM, test "x$have_NM" = "xyes")

# -----------------------------------------------------------
# Webkit
# -----------------------------------------------------------
AC_ARG_ENABLE(webkit,
              AS_HELP_STRING([--enable-webkit=@<:@no/yes/auto@:>@],
                             [build with webkit support]), ,
                             enable_webkit=auto)

if test "x$enable_webkit" != "xno"; then
   PKG_CHECK_MODULES(WEBKIT,
   [
      webkit-1.0 >= $WEBKIT_REQUIRED
   ], have_webkit="yes", have_webkit="no")

   if test "x$have_webkit" = "xyes"; then
      AC_DEFINE(HAVE_WEBKIT, 1, [Define if you have libwebkitgtk])
   fi
else
   have_webkit=no
fi

if test "x$enable_webkit" = "xyes" -a "x$have_webkit" != "xyes"; then
   AC_MSG_ERROR([Couldn't find webkit dependencies.])
fi
AM_CONDITIONAL(HAVE_WEBKIT, test "x$have_webkit" = "xyes")

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
# Map view checks: libchamplain
# -----------------------------------------------------------
AC_ARG_ENABLE(map,
              AS_HELP_STRING([--enable-map=@<:@no/yes/auto@:>@],
                             [Enable map view]), ,
                             enable_map=auto)

if test "x$enable_map" != "xno"; then
   PKG_CHECK_MODULES(LIBCHAMPLAIN,
   [
      champlain-0.3 >= $LIBCHAMPLAIN_REQUIRED,
      champlain-gtk-0.3 >= $LIBCHAMPLAIN_GTK_REQUIRED
      clutter-gtk-0.8 >= $CLUTTER_GTK_REQUIRED
   ], have_libchamplain="yes", have_libchamplain="no")

   if test "x$have_libchamplain" = "xyes"; then
      AC_DEFINE(HAVE_LIBCHAMPLAIN, 1, [Define if you have libchamplain])
   fi
else
   have_libchamplain=no
fi

if test "x$enable_map" = "xyes" -a "x$have_libchamplain" != "xyes"; then
   AC_MSG_ERROR([Couldn't find map view dependencies.])
fi

AM_CONDITIONAL(HAVE_LIBCHAMPLAIN, test "x$have_libchamplain" = "xyes")

# -----------------------------------------------------------
# location checks: geoclue
# -----------------------------------------------------------
AC_ARG_ENABLE(location,
              AS_HELP_STRING([--enable-location=@<:@no/yes/auto@:>@],
                             [Enable location awareness]), ,
                             enable_location=auto)

if test "x$enable_location" != "xno"; then
   PKG_CHECK_MODULES(GEOCLUE,
   [
      geoclue >= $GEOCLUE_REQUIRED
   ], have_geoclue="yes", have_geoclue="no")

   if test "x$have_geoclue" = "xyes"; then
      AC_DEFINE(HAVE_GEOCLUE, 1, [Define if you have geoclue])
   fi
else
   have_geoclue="no"
fi

if test "x$enable_location" = "xyes" -a "x$have_geoclue" != "xyes"; then
   AC_MSG_ERROR([Couldn't find location dependencies.])
fi

AM_CONDITIONAL(HAVE_GEOCLUE, test "x$have_geoclue" = "xyes")
AC_SUBST(GEOCLUE_CFLAGS)
AC_SUBST(GEOCLUE_LIBS)

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
# indicator checks: libindicate
# -----------------------------------------------------------
AC_ARG_ENABLE(libindicate,
              AS_HELP_STRING([--enable-libindicate=@<:@no/yes/auto@:>@],
                             [Enable message indicator]), ,
                             enable_libindicate=auto)

if test "x$enable_libindicate" != "xno"; then
   PKG_CHECK_MODULES(INDICATE,
   [
      indicate >= $LIBINDICATE_REQUIRED
   ], have_libindicate="yes", have_libindicate="no")

   if test "x$have_libindicate" = "xyes"; then
      AC_DEFINE(HAVE_LIBINDICATE, 1, [Define if you have libindicate])
   fi
else
   have_libindicate="no"
fi

if test "x$enable_libindicate" = "xyes" -a "x$have_libindicate" != "xyes"; then
   AC_MSG_ERROR([Couldn't find libindicate dependencies.])
fi

AM_CONDITIONAL(HAVE_LIBINDICATE, test "x$have_libindicate" = "xyes")
AC_SUBST(INDICATE_CFLAGS)
AC_SUBST(INDICATE_LIBS)

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
# Coding style checks
# -----------------------------------------------------------
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--disable-coding-style-checks],
                 [don't check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])
# -----------------------------------------------------------

SHAVE_INIT(.)

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
   shave
   shave-libtool
])

echo "
Configure summary:

	Compiler...............................:  ${CC}
	Compiler Flags.........................:  ${CFLAGS}
	Prefix.................................:  ${prefix}
	Shaved build...........................:  ${enable_shave}
	Tests..................................:  ${have_check}
	Coding style checks....................:  ${ENABLE_CODING_STYLE_CHECKS}

    Features:
	Spell checking (enchant)...............:  ${have_enchant}
	Display maps (libchamplain)............:  ${have_libchamplain}
	Location awareness (Geoclue)...........:  ${have_geoclue}
	Adium themes (Webkit)..................:  ${have_webkit}
	Message indicator support (libindicate):  ${have_libindicate}
	NetworkManager integration.............:  ${have_nm}

    Extras:
	Documentation..........................:  ${enable_gtk_doc}
	Python bindings........................:  ${have_python}
	Megaphone applet.......................:  ${have_megaphone}
	Nothere applet.........................:  ${have_nothere}
"
