cppflags =			\
	-I.			\
	-I$(top_srcdir)		\
	$(EMPATHY_CFLAGS)	\
	$(WARN_CFLAGS)

libs =								\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(EMPATHY_LIBS)

bin_PROGRAMS =			\
	empathy			\
	empathy-chat		\
	empathy-accounts	\
	empathy-contact-list

empathy_SOURCES =						\
	empathy-main.c						\
	empathy-filter.c	empathy-filter.h
empathy_CPPFLAGS = $(cppflags)
empathy_LDADD = $(libs)

empathy_accounts_SOURCES = empathy-accounts-main.c
empathy_accounts_CPPFLAGS = $(cppflags)
empathy_accounts_LDADD = $(libs)

empathy_chat_SOURCES = empathy-chat-main.c
empathy_chat_CPPFLAGS = $(cppflags)
empathy_chat_LDADD = $(libs)

empathy_contact_list_SOURCES = empathy-contact-list-main.c
empathy_contact_list_CPPFLAGS = $(cppflags)
empathy_contact_list_LDADD = $(libs)

# MC plugin
#pluginlib_LTLIBRARIES = libempathy-filter-plugin.la
#libempathy_filter_plugin_la_SOURCES = empathy-filter-plugin.c
#libempathy_filter_plugin_la_CPPFLAGS = $(MISSION_CONTROL_PLUGINS_CFLAGS)
#libempathy_filter_plugin_la_LIBADD = $(MISSION_CONTROL_PLUGINS_LIBS)

# Dbus service file
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Empathy.Chat.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

chandlerdir = $(datadir)/telepathy/managers
chandler_DATA = empathy-chat.chandler

# Empathy filter DBus API
empathy-filter-glue.h: empathy-filter.xml
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=empathy_filter --mode=glib-server --output=$@ $<
empathy-filter-gen.h: empathy-filter.xml
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=empathy_filter --mode=glib-client --output=$@ $<

BUILT_SOURCES =							\
	org.gnome.Empathy.Chat.service				\
	empathy-filter-glue.h					\
	empathy-filter-gen.h

EXTRA_DIST =							\
	org.gnome.Empathy.Chat.service.in			\
	empathy-filter.xml					\
	$(autostart_DATA)					\
	$(chandler_DATA)

CLEANFILES = $(BUILT_SOURCES)

