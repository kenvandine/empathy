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
	gossip-contact-list-store.c		gossip-contact-list-store.h	\
	gossip-contact-list-view.c		gossip-contact-list-view.h	\
	gossip-preferences.c			gossip-preferences.h		\
	gossip-theme-manager.c			gossip-theme-manager.h		\
	gossip-chat-window.c			gossip-chat-window.h		\
	gossip-chat.c				gossip-chat.h			\
	gossip-chat-view.c			gossip-chat-view.h		\
	gossip-private-chat.c			gossip-private-chat.h		\
	gossip-group-chat.c			gossip-group-chat.h		\
	gossip-geometry.c			gossip-geometry.h		\
	gossip-status-presets.c			gossip-status-presets.h		\
	gossip-presence-chooser.c		gossip-presence-chooser.h	\
	gossip-about-dialog.c			gossip-about-dialog.h		\
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
	gossip-group-chat.glade			\
	gossip-chat.glade

dtddir = $(datadir)/empathy
dtd_DATA = 					\
	gossip-status-presets.dtd		\
	gossip-contact-groups.dtd


EXTRA_DIST =			\
	$(glade_DATA)		\
	$(dtd_DATA)

