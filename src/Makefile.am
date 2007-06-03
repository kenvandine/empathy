cppflags =			\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
	-DLIBDIR=\""$(libdir)"\"			\
	-DLOCALEDIR=\""$(datadir)/locale"\"		\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

libs =								\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(EMPATHY_LIBS)

bin_PROGRAMS =			\
	empathy			\
	empathy-chat		\
	empathy-accounts

empathy_SOURCES = empathy-main.c
empathy_CPPFLAGS = $(cppflags)
empathy_LDADD = $(libs)

empathy_accounts_SOURCES = empathy-accounts-main.c
empathy_accounts_CPPFLAGS = $(cppflags)
empathy_accounts_LDADD = $(libs)

empathy_chat_SOURCES = empathy-chat-main.c
empathy_chat_CPPFLAGS = $(cppflags)
empathy_chat_LDADD = $(libs)

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Empathy.Chat.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

chandlerdir = $(datadir)/telepathy/managers
chandler_DATA = empathy-chat.chandler

BUILT_SOURCES =							\
	org.gnome.Empathy.Chat.service

EXTRA_DIST =							\
	org.gnome.Empathy.Chat.service.in			\
	$(autostart_DATA)					\
	$(chandler_DATA)

CLEANFILES = $(BUILT_SOURCES)

