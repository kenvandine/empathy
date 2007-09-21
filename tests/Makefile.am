AM_CPPFLAGS =						\
	-I$(top_srcdir)                          	\
	$(EMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

LDADD =								\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la	\
	$(top_builddir)/libempathy/libempathy.la		\
	$(EMPATHY_LIBS)

noinst_PROGRAMS =			\
	contact-manager

contact_manager_SOURCES = contact-manager.c

