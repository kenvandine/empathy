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
	$(EMPATHY_LIBS)

bin_PROGRAMS =			\
	empathy			\
	empathy-accounts	\
	empathy-logs

empathy_SOURCES =							\
	empathy.c							\
	bacon-message-connection.c bacon-message-connection.h		\
	empathy-chat-window.c empathy-chat-window.h			\
	empathy-new-chatroom-dialog.c empathy-new-chatroom-dialog.h	\
	empathy-status-icon.c empathy-status-icon.h			\
	empathy-about-dialog.c empathy-about-dialog.h			\
	empathy-chatrooms-window.c empathy-chatrooms-window.h		\
	empathy-main-window.c empathy-main-window.h			\
	empathy-preferences.c empathy-preferences.h			\
	ephy-spinner.c ephy-spinner.h

empathy_accounts_SOURCES = empathy-accounts.c
empathy_logs_SOURCES = empathy-logs.c

# Dbus service files
servicedir = $(datadir)/dbus-1/services
service_DATA =							\
	org.gnome.Empathy.Chat.service

%.service: %.service.in Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< | sed -e "s|\@bindir\@|$(bindir)|" > $@

chandlerdir = $(datadir)/telepathy/managers
chandler_DATA =							\
	empathy-chat.chandler

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-call-window.glade		\
	empathy-main-window.glade		\
	empathy-preferences.glade		\
	empathy-chatrooms-window.glade		\
	empathy-new-chatroom-dialog.glade	\
	empathy-status-icon.glade

BUILT_SOURCES =							\
	$(service_DATA)

EXTRA_DIST =							\
	org.gnome.Empathy.Chat.service.in			\
	org.gnome.Empathy.Call.service.in			\
	$(autostart_DATA)					\
	$(chandler_DATA)					\
	$(glade_DATA)

CLEANFILES = $(BUILT_SOURCES)

if HAVE_VOIP
libexec_PROGRAMS = empathy-call-chandler
empathy_call_chandler_SOURCES =				\
	empathy-call-chandler.c				\
	empathy-call-window.c empathy-call-window.h
service_DATA +=	org.gnome.Empathy.Call.service
chandler_DATA += empathy-call.chandler
else
EXTRA_DIST += empathy-call.chandler
endif

