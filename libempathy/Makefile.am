AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(LIBEMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

BUILT_SOURCES = 					\
	empathy-marshal.h				\
	empathy-marshal.c				\
	empathy-chandler-glue.h

lib_LTLIBRARIES = libempathy.la

libempathy_la_SOURCES =					\
	empathy-conf.c					\
	empathy-contact.c				\
	empathy-avatar.c				\
	empathy-time.c					\
	empathy-presence.c				\
	empathy-debug.c					\
	empathy-utils.c					\
	empathy-message.c				\
	empathy-chatroom-manager.c			\
	empathy-chatroom.c				\
	empathy-contact-list.c				\
	empathy-contact-manager.c			\
	empathy-tp-group.c				\
	empathy-tp-contact-list.c			\
	empathy-tp-chat.c				\
	empathy-tp-chatroom.c				\
	empathy-chandler.c				\
	empathy-idle.c					\
	empathy-log-manager.c				\
	empathy-marshal-main.c

libempathy_la_LIBADD =		\
	$(LIBEMPATHY_LIBS)

libempathydir = $(includedir)/libempathy/
libempathy_HEADERS =				\
	empathy-conf.h				\
	empathy-contact.h			\
	empathy-avatar.h			\
	empathy-time.h				\
	empathy-presence.h			\
	empathy-debug.h				\
	empathy-utils.h				\
	empathy-message.h			\
	empathy-chatroom-manager.h		\
	empathy-chatroom.h			\
	empathy-contact-list.h			\
	empathy-contact-manager.h		\
	empathy-tp-group.h			\
	empathy-tp-contact-list.h		\
	empathy-tp-chat.h			\
	empathy-tp-chatroom.h			\
	empathy-chandler.h			\
	empathy-idle.h				\
	empathy-log-manager.h

%-marshal.h: %-marshal.list Makefile.am
	$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile.am
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c

%-marshal-main.c: %-marshal.c %-marshal.h

empathy-chandler-glue.h: empathy-chandler.xml
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=empathy_chandler --mode=glib-server --output=$@ $<

dtddir = $(datadir)/empathy
dtd_DATA = 					\
	empathy-chatroom-manager.dtd

stylesheetdir = $(datadir)/empathy
stylesheet_DATA =		\
	empathy-log-manager.xsl

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libempathy.pc

EXTRA_DIST = 			\
	empathy-marshal.list	\
	empathy-chandler.xml	\
	$(stylesheet_DATA)	\
	$(dtd_DATA)

CLEANFILES = $(BUILT_SOURCES)
