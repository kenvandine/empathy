AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPKGDATADIR=\""$(pkgdatadir)"\" \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DIMAGEDIR=\"$(datadir)/empathy\" \
	$(MEGAPHONE_CFLAGS) $(WARN_CFLAGS)

libexec_PROGRAMS = megaphone-applet
megaphone_applet_SOURCES = 					\
	megaphone-applet.c 	megaphone-applet.h

megaphone_applet_LDFLAGS = 					\
	$(MEGAPHONE_LIBS)

megaphone_applet_LDADD = 					\
	$(top_builddir)/libempathy/libempathy.la \
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la
