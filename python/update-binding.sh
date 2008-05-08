#! /bin/sh
#Manually update headers in pyempathy.override and pyempathygtk.override.

# Update the list of headers from Makefile.am
cd ../libempathy
python /usr/share/pygtk/2.0/codegen/h2def.py	\
	empathy-avatar.h			\
	empathy-time.h				\
	empathy-status-presets.h		\
	empathy-debug.h				\
	empathy-utils.h				\
	empathy-message.h			\
	empathy-chatroom-manager.h		\
	empathy-chatroom.h			\
	empathy-contact.h			\
	empathy-contact-groups.h		\
	empathy-contact-list.h			\
	empathy-contact-manager.h		\
	empathy-contact-factory.h		\
	empathy-tp-contact-factory.h		\
	empathy-tp-group.h			\
	empathy-tp-contact-list.h		\
	empathy-tp-chat.h			\
	empathy-tp-roomlist.h			\
	empathy-tp-call.h			\
	empathy-tp-tube.h			\
	empathy-idle.h				\
	empathy-log-manager.h			\
	empathy-irc-network-manager.h		\
	empathy-irc-network.h			\
	empathy-irc-server.h			\
	empathy-tube-handler.h			\
	empathy-dispatcher.h			\
 > ../python/pyempathy/pyempathy.defs

# Update the list of headers from Makefile.am
cd ../libempathy-gtk
python /usr/share/pygtk/2.0/codegen/h2def.py	\
	empathy-images.h			\
	empathy-account-chooser.h		\
	empathy-chat.h				\
	empathy-irc-network-dialog.h		\
	empathy-spell-dialog.h			\
	empathy-accounts-dialog.h		\
	empathy-chat-view.h			\
	empathy-log-window.h			\
	empathy-theme-boxes.h			\
	empathy-account-widget.h		\
	empathy-conf.h				\
	empathy-theme.h				\
	empathy-account-widget-irc.h		\
	empathy-account-widget-sip.h		\
	empathy-contact-dialogs.h		\
	empathy-new-message-dialog.h		\
	empathy-theme-irc.h			\
	empathy-avatar-chooser.h		\
	empathy-contact-list-store.h		\
	empathy-presence-chooser.h		\
	empathy-theme-manager.h			\
	empathy-avatar-image.h			\
	empathy-contact-list-view.h		\
	empathy-ui-utils.h			\
	empathy-cell-renderer-activatable.h	\
	empathy-contact-widget.h		\
	empathy-profile-chooser.h		\
	empathy-cell-renderer-expander.h	\
	empathy-geometry.h			\
	empathy-smiley-manager.h		\
	empathy-cell-renderer-text.h		\
	empathy-spell.h				\
	empathy-contact-menu.h			\
 > ../python/pyempathygtk/pyempathygtk.defs

# Keep original version
cd ../python
cp pyempathy/pyempathy.defs /tmp
cp pyempathygtk/pyempathygtk.defs /tmp

# Apply patches
patch -p0 < pyempathy.patch
patch -p0 < pyempathygtk.patch

# Make modification then run that:
#diff -up /tmp/pyempathy.defs pyempathy/pyempathy.defs > pyempathy.patch
#diff -up /tmp/pyempathygtk.defs pyempathygtk/pyempathygtk.defs > pyempathygtk.patch

