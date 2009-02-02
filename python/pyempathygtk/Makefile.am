PYDEFS=`pkg-config --variable=defsdir pygtk-2.0`

AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)/libempathy-gtk			\
	-I$(top_builddir)/libempathy-gtk		\
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(PYTHON_BINDING_CFLAGS)			\
	$(PYTHON_INCLUDES)

BUILT_SOURCES = 					\
	pyempathygtk.c					

pyempathygtkdir = $(pyexecdir)
pyempathygtk_LTLIBRARIES = empathygtk.la

empathygtk_la_SOURCES =					\
	pyempathygtkmodule.c

nodist_empathygtk_la_SOURCES =				\
	pyempathygtk.c

empathygtk_la_LIBADD =					\
	$(PYTHON_BINDING_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la	\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la

empathygtk_la_LDFLAGS =					\
	-module -avoid-version

pyempathygtk.c: pyempathygtk.override pyempathygtk.defs
	$(PYGOBJECTCODEGEN)							\
		--prefix empathy 					\
		--register $(PYDEFS)/gdk-types.defs 			\
		--register $(PYDEFS)/gtk-types.defs 			\
		--register $(srcdir)/../pyempathy/pyempathy.defs	\
		--override $(srcdir)/pyempathygtk.override 		\
		$(srcdir)/pyempathygtk.defs > $@ 

EXTRA_DIST =			\
	pyempathygtk.override	\
	pyempathygtk.defs

CLEANFILES = $(BUILT_SOURCES)

