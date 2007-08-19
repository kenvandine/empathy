PYDEFS=`pkg-config --variable=defsdir pygtk-2.0`

AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)/libempathy-gtk			\
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(PYTHON_BINDING_CFLAGS)			\
	`python-config --cflags`			

BUILT_SOURCES = 					\
	pyempathygtk.c					

pyempathygtkdir = $(prefix)/lib/`pyversions -d`/site-packages
pyempathygtk_LTLIBRARIES = empathygtk.la

empathygtk_la_SOURCES =					\
	pyempathygtkmodule.c				\
	pyempathygtk.c

empathygtk_la_LIBADD =					\
	$(PYTHON_BINDING_LIBS)				\
	$(top_builddir)/libempathy/libempathy.la	\
	$(top_builddir)/libempathy-gtk/libempathy-gtk.la

empathygtk_la_LDFLAGS =					\
	`python-config --libs`				\
	-module -avoid-version

pyempathygtk.c: pyempathygtk.override pyempathygtk.defs
	pygtk-codegen-2.0						\
		--prefix empathy 					\
		--register $(PYDEFS)/gdk-types.defs 			\
		--register $(PYDEFS)/gtk-types.defs 			\
		--register ../pyempathy/pyempathy.defs			\
		--override pyempathygtk.override 			\
		pyempathygtk.defs > $@ 

EXTRA_DIST =			\
	pyempathygtk.override	\
	pyempathygtk.defs

CLEANFILES = $(BUILT_SOURCES)

