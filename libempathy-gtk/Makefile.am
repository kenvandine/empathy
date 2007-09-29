AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

BUILT_SOURCES = 					\
	empathy-gtk-enum-types.h			\
	empathy-gtk-enum-types.c

lib_LTLIBRARIES = libempathy-gtk.la

libempathy_gtk_la_SOURCES =             	\
	ephy-spinner.c	ephy-spinner.h		\
	empathy-main-window.c			\
	empathy-status-icon.c			\
	empathy-contact-widget.c		\
	empathy-contact-dialogs.c		\
	empathy-accounts-dialog.c		\
	empathy-account-widget-generic.c	\
	empathy-account-widget-jabber.c		\
	empathy-account-widget-msn.c		\
	empathy-account-widget-salut.c		\
	empathy-profile-chooser.c		\
	empathy-cell-renderer-expander.c	\
	empathy-cell-renderer-text.c		\
	empathy-cell-renderer-activatable.c	\
	empathy-spell.c				\
	empathy-spell-dialog.c			\
	empathy-contact-groups.c		\
	empathy-contact-list-store.c		\
	empathy-contact-list-view.c		\
	empathy-preferences.c			\
	empathy-theme-manager.c			\
	empathy-smiley-manager.c		\
	empathy-chat-window.c			\
	empathy-chat.c				\
	empathy-chat-view.c			\
	empathy-private-chat.c			\
	empathy-group-chat.c			\
	empathy-geometry.c			\
	empathy-status-presets.c		\
	empathy-presence-chooser.c		\
	empathy-about-dialog.c			\
	empathy-account-chooser.c		\
	empathy-new-chatroom-dialog.c		\
	empathy-chatrooms-window.c		\
	empathy-log-window.c			\
	empathy-call-window.c			\
	empathy-ui-utils.c

# do not distribute generated files
nodist_libempathy_gtk_la_SOURCES =\
	$(BUILT_SOURCES)

libempathy_gtk_la_LIBADD =			\
	$(EMPATHY_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la

libempathy_gtk_la_LDFLAGS =                \
       -version-info ${LIBEMPATHY_GTK_CURRENT}:${LIBEMPATHY_GTK_REVISION}:${LIBEMPATHY_GTK_AGE}

libempathy_gtk_headers =			\
	empathy-images.h			\
	empathy-main-window.h			\
	empathy-status-icon.h			\
	empathy-contact-widget.h		\
	empathy-contact-dialogs.h		\
	empathy-accounts-dialog.h		\
	empathy-account-widget-generic.h	\
	empathy-account-widget-jabber.h		\
	empathy-account-widget-msn.h		\
	empathy-account-widget-salut.h		\
	empathy-profile-chooser.h		\
	empathy-cell-renderer-expander.h	\
	empathy-cell-renderer-text.h		\
	empathy-cell-renderer-activatable.h	\
	empathy-spell.h				\
	empathy-spell-dialog.h			\
	empathy-contact-groups.h		\
	empathy-contact-list-store.h		\
	empathy-contact-list-view.h		\
	empathy-preferences.h			\
	empathy-theme-manager.h			\
	empathy-smiley-manager.h		\
	empathy-chat-window.h			\
	empathy-chat.h				\
	empathy-chat-view.h			\
	empathy-private-chat.h			\
	empathy-group-chat.h			\
	empathy-geometry.h			\
	empathy-status-presets.h		\
	empathy-presence-chooser.h		\
	empathy-about-dialog.h			\
	empathy-account-chooser.h		\
	empathy-new-chatroom-dialog.h		\
	empathy-chatrooms-window.h		\
	empathy-log-window.h			\
	empathy-call-window.h			\
	empathy-ui-utils.h

libempathy_gtk_includedir = $(includedir)/libempathy-gtk/
libempathy_gtk_include_HEADERS =		\
	$(libempathy_gtk_headers)		\
	empathy-gtk-enum-types.h

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-main-window.glade		\
	empathy-status-icon.glade		\
	empathy-contact-widget.glade		\
	empathy-contact-dialogs.glade		\
	empathy-preferences.glade		\
	empathy-presence-chooser.glade		\
	empathy-accounts-dialog.glade		\
	empathy-account-widget-jabber.glade	\
	empathy-account-widget-msn.glade	\
	empathy-account-widget-salut.glade	\
	empathy-new-chatroom-dialog.glade	\
	empathy-group-chat.glade		\
	empathy-chatrooms-window.glade		\
	empathy-spell-dialog.glade		\
	empathy-log-window.glade		\
	empathy-call-window.glade		\
	empathy-chat.glade

empathy-gtk-enum-types.h: stamp-empathy-gtk-enum-types.h
	@true
stamp-empathy-gtk-enum-types.h: Makefile $(libempathy_gtk_headers)
	(cd $(srcdir) \
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
	(cd $(srcdir) \
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

dtddir = $(datadir)/empathy
dtd_DATA = 					\
	empathy-status-presets.dtd		\
	empathy-contact-groups.dtd

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libempathy-gtk.pc

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(dtd_DATA)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	stamp-empathy-gtk-enum-types.h
