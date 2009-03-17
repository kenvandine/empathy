#! /bin/sh
#Manually update headers in pyempathy.override and pyempathygtk.override.

# Update the list of headers from Makefile.am
cd ../libempathy
python /usr/share/pygobject/2.0/codegen/h2def.py	\
        -m empathy				\
	empathy-account-manager.h		\
	empathy-chatroom.h			\
	empathy-chatroom-manager.h		\
	empathy-call-factory.h			\
	empathy-call-handler.h			\
	empathy-contact.h			\
	empathy-contact-factory.h		\
	empathy-contact-groups.h		\
	empathy-contact-list.h			\
	empathy-contact-manager.h		\
	empathy-contact-monitor.h		\
	empathy-debug.h				\
	empathy-dispatcher.h			\
	empathy-dispatch-operation.h		\
	empathy-idle.h				\
	empathy-irc-network.h			\
	empathy-irc-network-manager.h		\
	empathy-irc-server.h			\
	empathy-log-manager.h			\
	empathy-log-store.h			\
	empathy-log-store-empathy.h		\
	empathy-message.h			\
	empathy-status-presets.h		\
	empathy-time.h				\
	empathy-tp-call.h			\
	empathy-tp-chat.h			\
	empathy-tp-contact-factory.h		\
	empathy-tp-contact-list.h		\
	empathy-tp-file.h			\
	empathy-tp-group.h			\
	empathy-tp-roomlist.h			\
	empathy-tp-tube.h			\
	empathy-tube-handler.h			\
	empathy-types.h				\
	empathy-utils.h				\
 > ../python/pyempathy/pyempathy.defs

# Update the list of headers from Makefile.am
cd ../libempathy-gtk
python /usr/share/pygobject/2.0/codegen/h2def.py	\
	-m empathy				\
	empathy-account-chooser.h		\
	empathy-account-widget.h		\
	empathy-account-widget-irc.h		\
	empathy-account-widget-sip.h		\
	empathy-avatar-chooser.h		\
	empathy-avatar-image.h			\
	empathy-cell-renderer-activatable.h	\
	empathy-cell-renderer-expander.h	\
	empathy-cell-renderer-text.h		\
	empathy-chat.h				\
	empathy-chat-text-view.h		\
	empathy-chat-view.h			\
	empathy-conf.h				\
	empathy-contact-dialogs.h		\
	empathy-contact-list-store.h		\
	empathy-contact-list-view.h		\
	empathy-contact-menu.h			\
	empathy-contact-selector.h		\
	empathy-contact-widget.h		\
	empathy-geometry.h			\
	empathy-audio-sink.h			\
	empathy-audio-src.h			\
	empathy-video-src.h			\
	empathy-video-widget.h			\
	empathy-images.h			\
	empathy-irc-network-dialog.h		\
	empathy-log-window.h			\
	empathy-new-message-dialog.h		\
	empathy-presence-chooser.h		\
	empathy-profile-chooser.h		\
	empathy-smiley-manager.h		\
	empathy-spell.h				\
	empathy-spell-dialog.h			\
	empathy-theme-boxes.h			\
	empathy-theme-irc.h			\
	empathy-theme-manager.h			\
	empathy-ui-utils.h			\
 > ../python/pyempathygtk/pyempathygtk.defs

# Keep original version
cd ../python
cp pyempathy/pyempathy.defs /tmp
cp pyempathygtk/pyempathygtk.defs /tmp

# Apply patches
patch -p0 < pyempathy.patch
patch -p0 < pyempathygtk.patch

# Fix define of interfaces, they are not objects
sed -e 's/define-object \(ChatView$\)/define-interface \1/' \
    -i pyempathygtk/pyempathygtk.defs
sed -e 's/define-object \(ContactList$\)/define-interface \1/' \
    -i pyempathy/pyempathy.defs

# Make modification then run that:
#diff -up /tmp/pyempathy.defs pyempathy/pyempathy.defs > pyempathy.patch
#diff -up /tmp/pyempathygtk.defs pyempathygtk/pyempathygtk.defs > pyempathygtk.patch

