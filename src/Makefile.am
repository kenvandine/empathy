AM_CPPFLAGS =						\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
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
	empathy-call-chandler

empathy_SOURCES = empathy.c
empathy_accounts_SOURCES = empathy-accounts.c
empathy_call_chandler_SOURCES = empathy-call-chandler.c

# Dbus service files
servicedir = $(datadir)/dbus-1/services
service_DATA =							\
	org.gnome.Empathy.Chat.service				\
	org.gnome.Empathy.Call.service
%.service: %.service.in Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

chandlerdir = $(datadir)/telepathy/managers
chandler_DATA =							\
	empathy-chat.chandler

if HAVE_VOIP
chandler_DATA += empathy-call.chandler
endif

BUILT_SOURCES =							\
	org.gnome.Empathy.Chat.service				\
	org.gnome.Empathy.Call.service

EXTRA_DIST =							\
	org.gnome.Empathy.Chat.service.in			\
	org.gnome.Empathy.Call.service.in			\
	$(autostart_DATA)					\
	$(chandler_DATA)

CLEANFILES = $(BUILT_SOURCES)

