include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	$(LIBEMPATHYGTK_CFLAGS)				\
	$(ENCHANT_CFLAGS)				\
	$(LIBCHAMPLAIN_CFLAGS)				\
	$(GEOCLUE_CFLAGS)				\
	$(WEBKIT_CFLAGS)				\
	$(WARN_CFLAGS)					\
	$(DISABLE_DEPRECATED)

BUILT_SOURCES = 					\
	empathy-gtk-marshal.h				\
	empathy-gtk-marshal.c				\
	empathy-gtk-marshal.list			\
	empathy-gtk-enum-types.h			\
	empathy-gtk-enum-types.c

lib_LTLIBRARIES = libempathy-gtk.la

libempathy_gtk_handwritten_source =            	\
	empathy-account-chooser.c		\
	empathy-account-widget.c		\
	empathy-account-widget-irc.c		\
	empathy-account-widget-sip.c		\
	empathy-avatar-chooser.c		\
	empathy-avatar-image.c			\
	empathy-cell-renderer-activatable.c	\
	empathy-cell-renderer-expander.c	\
	empathy-cell-renderer-text.c		\
	empathy-chat.c				\
	empathy-chat-text-view.c		\
	empathy-chat-view.c			\
	empathy-conf.c				\
	empathy-contact-dialogs.c		\
	empathy-contact-list-store.c		\
	empathy-contact-list-view.c		\
	empathy-contact-menu.c			\
	empathy-contact-selector.c		\
	empathy-contact-widget.c		\
	empathy-geometry.c			\
	empathy-audio-sink.c			\
	empathy-audio-src.c			\
	empathy-video-src.c			\
	empathy-video-widget.c			\
	empathy-irc-network-dialog.c		\
	empathy-log-window.c			\
	empathy-new-message-dialog.c		\
	empathy-presence-chooser.c		\
	empathy-profile-chooser.c		\
	empathy-smiley-manager.c		\
	empathy-spell.c				\
	empathy-status-preset-dialog.c		\
	empathy-theme-boxes.c			\
	empathy-theme-irc.c			\
	empathy-theme-manager.c			\
	empathy-ui-utils.c

libempathy_gtk_la_SOURCES =			\
	$(libempathy_gtk_handwritten_source)	\
	totem-subtitle-encoding.c totem-subtitle-encoding.h

# do not distribute generated files
nodist_libempathy_gtk_la_SOURCES =\
	$(BUILT_SOURCES)

libempathy_gtk_la_LIBADD =			\
	$(LIBEMPATHYGTK_LIBS)			\
	$(ENCHANT_LIBS)				\
	$(LIBCHAMPLAIN_LIBS)			\
	$(GEOCLUE_LIBS)				\
	$(WEBKIT_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la

libempathy_gtk_la_LDFLAGS =                \
       -version-info ${LIBEMPATHY_GTK_CURRENT}:${LIBEMPATHY_GTK_REVISION}:${LIBEMPATHY_GTK_AGE} \
       -export-symbols-regex ^empathy_

libempathy_gtk_headers =			\
	empathy-account-chooser.h		\
	empathy-account-widget.h		\
	empathy-account-widget-irc.h		\
	empathy-account-widget-sip.h		\
	empathy-avatar-chooser.h		\
	empathy-avatar-image.h			\
	empathy-cell-renderer-activatable.h	\
	empathy-cell-renderer-expander.h	\
	empathy-cell-renderer-text.h		\
	empathy-chat.h				\
	empathy-chat-text-view.h		\
	empathy-chat-view.h			\
	empathy-conf.h				\
	empathy-contact-dialogs.h		\
	empathy-contact-list-store.h		\
	empathy-contact-list-view.h		\
	empathy-contact-menu.h			\
	empathy-contact-selector.h		\
	empathy-contact-widget.h		\
	empathy-geometry.h			\
	empathy-audio-sink.h			\
	empathy-audio-src.h			\
	empathy-video-src.h			\
	empathy-video-widget.h			\
	empathy-images.h			\
	empathy-irc-network-dialog.h		\
	empathy-log-window.h			\
	empathy-new-message-dialog.h		\
	empathy-presence-chooser.h		\
	empathy-profile-chooser.h		\
	empathy-smiley-manager.h		\
	empathy-spell.h				\
	empathy-status-preset-dialog.h		\
	empathy-theme-boxes.h			\
	empathy-theme-irc.h			\
	empathy-theme-manager.h			\
	empathy-ui-utils.h

check_c_sources = \
    $(libempathy_gtk_handwritten_source) \
    $(libempathy_gtk_headers)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

libempathy_gtk_includedir = $(includedir)/libempathy-gtk/
libempathy_gtk_include_HEADERS =		\
	$(libempathy_gtk_headers)		\
	empathy-gtk-enum-types.h

uidir = $(datadir)/empathy
ui_DATA = 					\
	empathy-contact-widget.ui		\
	empathy-contact-dialogs.ui		\
	empathy-account-widget-generic.ui	\
	empathy-account-widget-jabber.ui	\
	empathy-account-widget-msn.ui		\
	empathy-account-widget-sip.ui		\
	empathy-account-widget-salut.ui		\
	empathy-account-widget-irc.ui		\
	empathy-account-widget-icq.ui		\
	empathy-account-widget-yahoo.ui		\
	empathy-account-widget-groupwise.ui	\
	empathy-account-widget-aim.ui		\
	empathy-status-preset-dialog.ui		\
	empathy-log-window.ui			\
	empathy-chat.ui				\
	empathy-new-message-dialog.ui

empathy-gtk-marshal.list: $(libempathy_gtk_la_SOURCES) Makefile.am
	$(QUIET_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*empathy_gtk_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libempathy_gtk_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.h: %-marshal.list Makefile
	$(QUIET_GEN)$(GLIB_GENMARSHAL) --header --prefix=_$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	$(QUIET_GEN)echo "#include \"empathy-gtk-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=_$(subst -,_,$*)_marshal $< >> $*-marshal.c

empathy-gtk-enum-types.h: stamp-empathy-gtk-enum-types.h
	$(QUIET_GEN)true
stamp-empathy-gtk-enum-types.h: Makefile $(libempathy_gtk_headers)
	$(QUIET_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __LIBEMPATHY_GTK_ENUM_TYPES_H__\n" \
			--fhead "#define __LIBEMPATHY_GTK_ENUM_TYPES_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __LIBEMPATHY_GTK_ENUM_TYPES_H__ */\n" \
			--fprod "#include <libempathy-gtk/@filename@>\n" \
			--eprod "#define EMPATHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(libempathy_gtk_headers) ) > xgen-gth \
	&& (cmp -s xgen-gth empathy-gtk-enum-type.h || cp xgen-gth empathy-gtk-enum-types.h) \
	&& rm -f xgen-gth \
	&& echo timestamp > $(@F)

empathy-gtk-enum-types.c: Makefile $(libempathy_gtk_headers)
	$(QUIET_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <config.h>\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"empathy-gtk-enum-types.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libempathy_gtk_headers) ) > xgen-gtc \
	&& cp xgen-gtc $(@F) \
	&& rm -f xgen-gtc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libempathy-gtk.pc

EXTRA_DIST =			\
	$(ui_DATA)

if HAVE_GEOCLUE
libempathy_gtk_handwritten_source +=		\
	empathy-location-manager.c

libempathy_gtk_headers +=			\
	empathy-location-manager.h
else
EXTRA_DIST +=					\
	empathy-location-manager.c		\
	empathy-location-manager.h
endif

if HAVE_WEBKIT
libempathy_gtk_handwritten_source +=		\
	empathy-plist.c				\
	empathy-theme-adium.c
libempathy_gtk_headers +=			\
	empathy-plist.h				\
	empathy-theme-adium.h
else
EXTRA_DIST +=					\
	empathy-plist.c				\
	empathy-plist.h				\
	empathy-theme-adium.c			\
	empathy-theme-adium.h
endif

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	stamp-empathy-gtk-enum-types.h
