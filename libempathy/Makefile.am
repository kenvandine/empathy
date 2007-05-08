AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	-DLOCALEDIR=\""$(datadir)/locale"\"		\
	$(LIBEMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

AM_LDFLAGS =						\
	$(LIBEMPATHY_LIBS)

BUILT_SOURCES = 					\
	empathy-marshal.h				\
	empathy-marshal.c				\
	empathy-chandler-glue.h

noinst_LTLIBRARIES = libempathy.la

libempathy_la_SOURCES =             						\
	gossip-conf.c			gossip-conf.h				\
	gossip-contact.c		gossip-contact.h			\
	gossip-avatar.c			gossip-avatar.h				\
	gossip-time.c			gossip-time.h				\
	gossip-presence.c		gossip-presence.h			\
	gossip-telepathy-group.c	gossip-telepathy-group.h		\
	gossip-paths.c			gossip-paths.h				\
	gossip-debug.c			gossip-debug.h				\
	gossip-utils.c			gossip-utils.h				\
	gossip-message.c		gossip-message.h			\
	empathy-contact-list.c		empathy-contact-list.h			\
	empathy-contact-manager.c	empathy-contact-manager.h		\
	empathy-tp-chat.c		empathy-tp-chat.h			\
	empathy-chandler.c		empathy-chandler.h			\
	empathy-marshal-main.c

libempathy_includedir = $(includedir)/empathy/

%-marshal.h: %-marshal.list Makefile.am
	$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile.am
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c

%-marshal-main.c: %-marshal.c %-marshal.h

empathy-chandler-glue.h: empathy-chandler.xml
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=empathy_chandler --mode=glib-server --output=$@ $<

EXTRA_DIST = 			\
	empathy-marshal.list	\
	empathy-chandler.xml

CLEANFILES = $(BUILT_SOURCES)
