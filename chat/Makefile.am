AM_CPPFLAGS =	                                 		\
	-I$(top_srcdir)                          		\
	-DPREFIX="\"$(prefix)"\"				\
	-DSYSCONFDIR=\""$(sysconfdir)"\"			\
	-DDATADIR=\""$(datadir)"\"              		\
	-DLIBDIR=\""$(libdir)"\"				\
	$(EMPATHY_CFLAGS)					\
	$(WARN_CFLAGS)

bin_PROGRAMS = empathy-chat

empathy_chat_SOURCES =					\
	empathy-chat-main.c

empathy_chat_LDADD =						\
	$(top_builddir)/libempathy/libempathy.la		\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(EMPATHY_LIBS)

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Empathy.Chat.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

chandlerdir = $(datadir)/telepathy/managers
chandler_DATA = empathy-chat.chandler


EXTRA_DIST =					\
	org.gnome.Empathy.Chat.service.in	\
	$(chandler_DATA)

BUILT_SOURCES =				\
	org.gnome.Empathy.Chat.service
	
CLEANFILES = $(BUILT_SOURCES)
