imagedir = $(datadir)/empathy

image_DATA =				\
	gossip-offline.png		\
	gossip-available.png    	\
	gossip-busy.png			\
	gossip-away.png			\
	gossip-extended-away.png 	\
	gossip-pending.png 		\
	gossip-message.png		\
	gossip-typing.png		\
	gossip-group-message.png 	\
	vcard_16.png			\
	vcard_48.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = 		\
	$(image_DATA)


