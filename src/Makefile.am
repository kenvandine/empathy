AM_CPPFLAGS =						\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
	-DPKGDATADIR=\""$(pkgdatadir)"\"		\
	-DLIBDIR=\""$(libdir)"\"			\
	-DLOCALEDIR=\""$(datadir)/locale"\"		\
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
	empathy.c							\
	bacon-message-connection.c bacon-message-connection.h		\
	ephy-spinner.c ephy-spinner.h					\
	empathy-chat-window.c empathy-chat-window.h			\
	empathy-new-chatroom-dialog.c empathy-new-chatroom-dialog.h	\
	empathy-status-icon.c empathy-status-icon.h			\
	empathy-about-dialog.c empathy-about-dialog.h			\
	empathy-chatrooms-window.c empathy-chatrooms-window.h		\
	empathy-main-window.c empathy-main-window.h			\
	empathy-preferences.c empathy-preferences.h			\
	empathy-call-window.c empathy-call-window.h			\
	empathy-accounts-dialog.c empathy-accounts-dialog.h		\
	empathy-event-manager.c empathy-event-manager.h

empathy_logs_SOURCES = empathy-logs.c

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-call-window.glade		\
	empathy-main-window.glade		\
	empathy-preferences.glade		\
	empathy-chatrooms-window.glade		\
	empathy-chat-window.glade		\
	empathy-new-chatroom-dialog.glade	\
	empathy-status-icon.glade		\
	empathy-accounts-dialog.glade

dist_man_MANS =			\
	empathy.1

EXTRA_DIST =			\
	$(autostart_DATA)	\
	$(glade_DATA)

CLEANFILES = $(BUILT_SOURCES)

