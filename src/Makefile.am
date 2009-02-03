AM_CPPFLAGS =						\
	-I$(top_srcdir)                          	\
	$(EMPATHY_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)				\
	$(WARN_CFLAGS)

LDADD =								\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(top_builddir)/libempathy/libempathy.la		\
	$(top_builddir)/extensions/libemp-extensions.la		\
	$(LIBNOTIFY_LIBS)					\
	$(EMPATHY_LIBS)

bin_PROGRAMS =			\
	empathy			\
	empathy-logs

BUILT_SOURCES= \
	empathy-tube-dispatch-enumtypes.h \
	empathy-tube-dispatch-enumtypes.c

empathy_SOURCES =							\
	bacon-message-connection.c bacon-message-connection.h		\
	empathy.c							\
	empathy-about-dialog.c empathy-about-dialog.h			\
	empathy-accounts-dialog.c empathy-accounts-dialog.h		\
	empathy-call-window.c empathy-call-window.h			\
	empathy-chatrooms-window.c empathy-chatrooms-window.h		\
	empathy-chat-window.c empathy-chat-window.h			\
	empathy-event-manager.c empathy-event-manager.h			\
	empathy-ft-manager.c empathy-ft-manager.h			\
	empathy-import-dialog.c empathy-import-dialog.h			\
	empathy-import-pidgin.c empathy-import-pidgin.h			\
	empathy-main-window.c empathy-main-window.h			\
	empathy-misc.c empathy-misc.h					\
	empathy-new-chatroom-dialog.c empathy-new-chatroom-dialog.h	\
	empathy-preferences.c empathy-preferences.h			\
	empathy-sidebar.c empathy-sidebar.h			\
	empathy-status-icon.c empathy-status-icon.h			\
	empathy-tube-dispatch.c empathy-tube-dispatch.h			\
	ephy-spinner.c ephy-spinner.h

nodist_empathy_SOURCES = $(BUILT_SOURCES)

empathy_logs_SOURCES = empathy-logs.c

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-accounts-dialog.glade		\
	empathy-call-window.glade		\
	empathy-chatrooms-window.glade		\
	empathy-chat-window.glade		\
	empathy-ft-manager.glade		\
	empathy-import-dialog.glade		\
	empathy-main-window.glade		\
	empathy-new-chatroom-dialog.glade	\
	empathy-preferences.glade		\
	empathy-status-icon.glade

dist_man_MANS =			\
	empathy.1

# rules for making the glib enum objects
%-enumtypes.h: %.h Makefile.in
	glib-mkenums \
	--fhead "#ifndef __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n#define __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $(shell echo $* | tr [:lower:]- [:upper:]_ | sed 's/_.*//')_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif /* __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__ */" \
	$< > $@

%-enumtypes.c: %.h Makefile.in
	glib-mkenums \
	--fhead "#include <$*.h>\n#include <$*-enumtypes.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@VALUENAME@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@

EXTRA_DIST =			\
	$(autostart_DATA)	\
	$(glade_DATA)

CLEANFILES = $(BUILT_SOURCES)

