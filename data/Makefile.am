SUBDIRS = 16x16

profiledir = $(datadir)/mission-control/profiles
profile_DATA = 				\
	jabber.profile			\
	gtalk.profile			\
	salut.profile			\
	msn.profile

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
	$(image_DATA)	\
	$(profile_DATA)


