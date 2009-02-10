SUBDIRS = icons

profiledir = $(datadir)/mission-control/profiles
profile_DATA = 				\
	aim.profile			\
	icq.profile			\
	jabber.profile			\
	salut.profile			\
	yahoo.profile			\
	gtalk.profile			\
	irc.profile			\
	msn.profile			\
	msn-haze.profile		\
	sofiasip.profile		\
	ekiga.profile			\
	fwd.profile			\
	sipphone.profile		\
	groupwise.profile		\
	gadugadu.profile		\
	qq.profile

desktopdir = $(datadir)/applications
desktop_in_files = empathy.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemasdir	 = $(GCONF_SCHEMA_FILE_DIR)
schemas_in_files = empathy.schemas.in
schemas_DATA 	 = $(schemas_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p ; \
	  done \
	fi
else
install-data-local:
endif

EXTRA_DIST = 				\
	$(profile_DATA)			\
	$(desktop_in_files)		\
	$(desktop_DATA)		\
	$(schemas_in_files)		\
	$(schemas_DATA)

DISTCLEANFILES = 			\
	$(desktop_DATA)		\
	$(schemas_DATA)

