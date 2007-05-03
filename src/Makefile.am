AM_CPPFLAGS =	                                 		\
	-I$(top_srcdir)                          		\
	-DPREFIX="\"$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"              		\
	-DLIBDIR=\""$(libdir)"\"				\
	$(EMPATHY_CFLAGS)					\
	$(WARN_CFLAGS)

LDADD =								\
	$(top_builddir)/libempathy/libempathy.la		\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(EMPATHY_LIBS)

bin_PROGRAMS = empathy empathy-accounts empathy-chat empathy-contact-list
empathy_SOURCES =						\
	empathy-main.c

empathy_accounts_SOURCES =					\
	empathy-accounts-main.c

empathy_chat_SOURCES =						\
	empathy-chat-main.c

empathy_contact_list_SOURCES =					\
	empathy-contact-list-main.c

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Empathy.Chat.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

chandlerdir = $(datadir)/telepathy/managers
chandler_DATA = empathy-chat.chandler

autostartdir = $(datadir)/gnome/autostart
autostart_in_files = empathy.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

BUILT_SOURCES =							\
	org.gnome.Empathy.Chat.service

EXTRA_DIST =							\
	org.gnome.Empathy.Chat.service.in			\
	$(autostart_DATA)					\
	$(chandler_DATA)

CLEANFILES = $(BUILT_SOURCES)

