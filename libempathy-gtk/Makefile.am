AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

noinst_LTLIBRARIES = libempathy-gtk.la

libempathy_gtk_la_SOURCES =             					\
	ephy-spinner.c				ephy-spinner.h			\
	empathy-images.h							\
	empathy-main-window.c			empathy-main-window.h		\
	empathy-status-icon.c			empathy-status-icon.h		\
	empathy-contact-widget.c		empathy-contact-widget.h	\
	empathy-contact-dialogs.c		empathy-contact-dialogs.h	\
	empathy-accounts-dialog.c		empathy-accounts-dialog.h	\
	empathy-account-widget-generic.c		empathy-account-widget-generic.h	\
	empathy-account-widget-jabber.c		empathy-account-widget-jabber.h	\
	empathy-profile-chooser.c		empathy-profile-chooser.h	\
	empathy-cell-renderer-expander.c		empathy-cell-renderer-expander.h	\
	empathy-cell-renderer-text.c		empathy-cell-renderer-text.h	\
	empathy-spell.c				empathy-spell.h			\
	empathy-spell-dialog.c			empathy-spell-dialog.h		\
	empathy-contact-groups.c			empathy-contact-groups.h		\
	empathy-contact-list-store.c		empathy-contact-list-store.h	\
	empathy-contact-list-view.c		empathy-contact-list-view.h	\
	empathy-preferences.c			empathy-preferences.h		\
	empathy-theme-manager.c			empathy-theme-manager.h		\
	empathy-chat-window.c			empathy-chat-window.h		\
	empathy-chat.c				empathy-chat.h			\
	empathy-chat-view.c			empathy-chat-view.h		\
	empathy-private-chat.c			empathy-private-chat.h		\
	empathy-group-chat.c			empathy-group-chat.h		\
	empathy-geometry.c			empathy-geometry.h		\
	empathy-status-presets.c			empathy-status-presets.h		\
	empathy-presence-chooser.c		empathy-presence-chooser.h	\
	empathy-about-dialog.c			empathy-about-dialog.h		\
	empathy-account-chooser.c		empathy-account-chooser.h	\
	empathy-new-chatroom-dialog.c		empathy-new-chatroom-dialog.h	\
	empathy-chatrooms-window.c		empathy-chatrooms-window.h	\
	empathy-log-window.c			empathy-log-window.h		\
	empathy-ui-utils.c			empathy-ui-utils.h

libempathy_gtk_la_LIBADD =				\
	$(EMPATHY_LIBS)					\
	$(top_builddir)/libempathy/libempathy.la

libempathy_gtk_includedir = $(includedir)/empathy/

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
	empathy-group-chat.glade			\
	empathy-chatrooms-window.glade		\
	empathy-spell-dialog.glade		\
	empathy-log-window.glade			\
	empathy-chat.glade

dtddir = $(datadir)/empathy
dtd_DATA = 					\
	empathy-status-presets.dtd		\
	empathy-contact-groups.dtd


EXTRA_DIST =			\
	$(glade_DATA)		\
	$(dtd_DATA)

