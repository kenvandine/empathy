AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

lib_LTLIBRARIES = libempathy-gtk.la

libempathy_gtk_la_SOURCES =             	\
	ephy-spinner.c	ephy-spinner.h		\
	empathy-main-window.c			\
	empathy-status-icon.c			\
	empathy-contact-widget.c		\
	empathy-contact-dialogs.c		\
	empathy-accounts-dialog.c		\
	empathy-account-widget-generic.c	\
	empathy-account-widget-jabber.c		\
	empathy-profile-chooser.c		\
	empathy-cell-renderer-expander.c	\
	empathy-cell-renderer-text.c		\
	empathy-spell.c				\
	empathy-spell-dialog.c			\
	empathy-contact-groups.c		\
	empathy-contact-list-store.c		\
	empathy-contact-list-view.c		\
	empathy-preferences.c			\
	empathy-theme-manager.c			\
	empathy-chat-window.c			\
	empathy-chat.c				\
	empathy-chat-view.c			\
	empathy-private-chat.c			\
	empathy-group-chat.c			\
	empathy-geometry.c			\
	empathy-status-presets.c		\
	empathy-presence-chooser.c		\
	empathy-about-dialog.c			\
	empathy-account-chooser.c		\
	empathy-new-chatroom-dialog.c		\
	empathy-chatrooms-window.c		\
	empathy-log-window.c			\
	empathy-ui-utils.c

libempathy_gtk_la_LIBADD =			\
	$(EMPATHY_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la

libempathy_gtkdir = $(includedir)/libempathy-gtk/
libempathy_gtk_HEADERS =			\
	empathy-images.h			\
	empathy-main-window.h			\
	empathy-status-icon.h			\
	empathy-contact-widget.h		\
	empathy-contact-dialogs.h		\
	empathy-accounts-dialog.h		\
	empathy-account-widget-generic.h	\
	empathy-account-widget-jabber.h		\
	empathy-profile-chooser.h		\
	empathy-cell-renderer-expander.h	\
	empathy-cell-renderer-text.h		\
	empathy-spell.h				\
	empathy-spell-dialog.h			\
	empathy-contact-groups.h		\
	empathy-contact-list-store.h		\
	empathy-contact-list-view.h		\
	empathy-preferences.h			\
	empathy-theme-manager.h			\
	empathy-chat-window.h			\
	empathy-chat.h				\
	empathy-chat-view.h			\
	empathy-private-chat.h			\
	empathy-group-chat.h			\
	empathy-geometry.h			\
	empathy-status-presets.h		\
	empathy-presence-chooser.h		\
	empathy-about-dialog.h			\
	empathy-account-chooser.h		\
	empathy-new-chatroom-dialog.h		\
	empathy-chatrooms-window.h		\
	empathy-log-window.h			\
	empathy-ui-utils.h

gladedir = $(datadir)/empathy
glade_DATA = 					\
	empathy-main-window.glade		\
	empathy-status-icon.glade		\
	empathy-contact-widget.glade		\
	empathy-contact-dialogs.glade		\
	empathy-preferences.glade		\
	empathy-presence-chooser.glade		\
	empathy-accounts-dialog.glade		\
	empathy-account-widget-jabber.glade	\
	empathy-new-chatroom-dialog.glade	\
	empathy-group-chat.glade		\
	empathy-chatrooms-window.glade		\
	empathy-spell-dialog.glade		\
	empathy-log-window.glade		\
	empathy-chat.glade

dtddir = $(datadir)/empathy
dtd_DATA = 					\
	empathy-status-presets.dtd		\
	empathy-contact-groups.dtd

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libempathy-gtk.pc

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(dtd_DATA)

