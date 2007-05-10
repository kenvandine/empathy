AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libempathy-gtk.la

libempathy_gtk_la_SOURCES =             					\
	ephy-spinner.c				ephy-spinner.h			\
	empathy-images.h							\
	empathy-main-window.c			empathy-main-window.h		\
	empathy-status-icon.c			empathy-status-icon.h		\
	gossip-accounts-dialog.c		gossip-accounts-dialog.h	\
	gossip-account-widget-generic.c		gossip-account-widget-generic.h	\
	gossip-account-widget-jabber.c		gossip-account-widget-jabber.h	\
	gossip-profile-chooser.c		gossip-profile-chooser.h	\
	gossip-cell-renderer-expander.c		gossip-cell-renderer-expander.h	\
	gossip-cell-renderer-text.c		gossip-cell-renderer-text.h	\
	gossip-spell.c				gossip-spell.h			\
	gossip-contact-groups.c			gossip-contact-groups.h		\
	gossip-contact-list.c			gossip-contact-list.h		\
	gossip-preferences.c			gossip-preferences.h		\
	gossip-theme-manager.c			gossip-theme-manager.h		\
	gossip-chat.c				gossip-chat.h			\
	gossip-chat-view.c			gossip-chat-view.h		\
	gossip-chat-window.c			gossip-chat-window.h		\
	gossip-private-chat.c			gossip-private-chat.h		\
	gossip-geometry.c			gossip-geometry.h		\
	gossip-status-presets.c			gossip-status-presets.h		\
	gossip-presence-chooser.c		gossip-presence-chooser.h	\
	gossip-ui-utils.c			gossip-ui-utils.h

libempathy_gtk_la_LIBADD =				\
	$(EMPATHY_LIBS)					\
	$(top_builddir)/libempathy/libempathy.la

libempathy_gtk_includedir = $(includedir)/empathy/

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-main-window.glade		\
	empathy-status-icon.glade		\
	gossip-preferences.glade		\
	gossip-presence-chooser.glade		\
	gossip-accounts-dialog.glade		\
	gossip-account-widget-jabber.glade	\
	gossip-chat.glade

dtddir = $(datadir)/empathy
dtd_DATA = 									\
	gossip-status-presets.dtd						\
	gossip-contact-groups.dtd

schemasdir	 = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = empathy.schemas.in
schemas_DATA 	 = $(schemas_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p ; \
	  done \
	fi
else
install-data-local:
endif

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(dtd_DATA)		\
	$(schemas_in_files)	\
	$(schemas_DATA)

DISTCLEANFILES = 		\
	$(schemas_DATA)

