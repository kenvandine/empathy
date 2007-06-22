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

noinst_LTLIBRARIES = libempathy.la

libempathy_la_SOURCES =             						\
	empathy-conf.c			empathy-conf.h				\
	empathy-contact.c		empathy-contact.h			\
	empathy-avatar.c		empathy-avatar.h			\
	empathy-time.c			empathy-time.h				\
	empathy-presence.c		empathy-presence.h			\
	empathy-debug.c			empathy-debug.h				\
	empathy-utils.c			empathy-utils.h				\
	empathy-message.c		empathy-message.h			\
	empathy-chatroom-manager.c	empathy-chatroom-manager.h		\
	empathy-chatroom.c		empathy-chatroom.h			\
	empathy-contact-list.c		empathy-contact-list.h			\
	empathy-contact-manager.c	empathy-contact-manager.h		\
	empathy-tp-group.c		empathy-tp-group.h			\
	empathy-tp-contact-list.c	empathy-tp-contact-list.h		\
	empathy-tp-chat.c		empathy-tp-chat.h			\
	empathy-tp-chatroom.c		empathy-tp-chatroom.h			\
	empathy-chandler.c		empathy-chandler.h			\
	empathy-idle.c			empathy-idle.h				\
	empathy-log-manager.c		empathy-log-manager.h			\
	empathy-marshal-main.c

libempathy_la_LIBADD =		\
	$(LIBEMPATHY_LIBS)

libempathy_includedir = $(includedir)/empathy/

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

EXTRA_DIST = 			\
	empathy-marshal.list	\
	empathy-chandler.xml	\
	$(stylesheet_DATA)	\
	$(dtd_DATA)

CLEANFILES = $(BUILT_SOURCES)
