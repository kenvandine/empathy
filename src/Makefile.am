AM_CPPFLAGS =						\
	-I$(top_srcdir)                          	\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

LDADD =								\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(top_builddir)/libempathy/libempathy.la		\
	$(top_builddir)/extensions/libemp-extensions.la		\
	$(EMPATHY_LIBS)

bin_PROGRAMS =			\
	empathy			\
	empathy-logs

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
	empathy-new-chatroom-dialog.c empathy-new-chatroom-dialog.h	\
	empathy-preferences.c empathy-preferences.h			\
	empathy-status-icon.c empathy-status-icon.h			\
	ephy-spinner.c ephy-spinner.h

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

EXTRA_DIST =			\
	$(autostart_DATA)	\
	$(glade_DATA)

CLEANFILES = $(BUILT_SOURCES)

