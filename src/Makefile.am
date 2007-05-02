AM_CPPFLAGS =	                                 	\
	-I$(top_srcdir)                          	\
	-DPREFIX="\"$(prefix)"\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DDATADIR=\""$(datadir)"\"              	\
	-DLIBDIR=\""$(libdir)"\"			\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

bin_PROGRAMS = empathy

empathy_SOURCES = 				                \
	empathy-main.c

empathy_LDADD = \
	$(top_builddir)/libempathy/libempathy.la				\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la			\
	$(EMPATHY_LIBS)

autostartdir = $(datadir)/gnome/autostart
autostart_in_files = empathy.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@
