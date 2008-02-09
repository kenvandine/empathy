AM_CPPFLAGS =                                           \
	-I.                                             \
	-I$(top_srcdir)                                 \
	-DDATADIR=\""$(datadir)"\"              	\
	$(LIBEMPATHY_CFLAGS)				\
	$(WARN_CFLAGS)

BUILT_SOURCES = 					\
	empathy-marshal.h				\
	empathy-marshal.c				\
	empathy-marshal.list				\
	empathy-enum-types.h				\
	empathy-enum-types.c				\
	empathy-filter-glue.h				\
	empathy-chandler-glue.h				\
	tp-stream-engine-gen.h

lib_LTLIBRARIES = libempathy.la

libempathy_la_SOURCES =					\
	empathy-avatar.c				\
	empathy-time.c					\
	empathy-status-presets.c			\
	empathy-debug.c					\
	empathy-utils.c					\
	empathy-message.c				\
	empathy-chatroom-manager.c			\
	empathy-chatroom.c				\
	empathy-contact.c				\
	empathy-contact-groups.c			\
	empathy-contact-list.c				\
	empathy-contact-manager.c			\
	empathy-contact-factory.c			\
	empathy-tp-contact-factory.c			\
	empathy-tp-group.c				\
	empathy-tp-contact-list.c			\
	empathy-tp-chat.c				\
	empathy-tp-chatroom.c				\
	empathy-tp-roomlist.c				\
	empathy-tp-call.c				\
	empathy-chandler.c				\
	empathy-filter.c				\
	empathy-idle.c					\
	empathy-log-manager.c

# do not distribute generated files
nodist_libempathy_la_SOURCES =\
	$(BUILT_SOURCES)

libempathy_la_LIBADD =		\
	$(LIBEMPATHY_LIBS)

libempathy_la_LDFLAGS =		\
       -version-info ${LIBEMPATHY_CURRENT}:${LIBEMPATHY_REVISION}:${LIBEMPATHY_AGE} \
       -export-symbols-regex ^empathy_

libempathy_headers =				\
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
	empathy-tp-chatroom.h			\
	empathy-tp-roomlist.h			\
	empathy-tp-call.h			\
	empathy-chandler.h			\
	empathy-filter.h			\
	empathy-idle.h				\
	empathy-log-manager.h

libempathy_includedir = $(includedir)/libempathy/
libempathy_include_HEADERS =			\
	$(libempathy_headers)			\
	empathy-enum-types.h

empathy-marshal.list: $(libempathy_la_SOURCES) Makefile.am
	( cd $(srcdir) && \
	sed -n -e 's/.*empathy_marshal_\([[:upper:]]*__[[:upper:]_]*\).*/\1/p' \
	$(libempathy_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.h: %-marshal.list Makefile
	$(GLIB_GENMARSHAL) --header --prefix=_$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	echo "#include \"empathy-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=_$(subst -,_,$*)_marshal $< >> $*-marshal.c

empathy-chandler-glue.h: empathy-chandler.xml Makefile
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=empathy_chandler --mode=glib-server --output=$@ $<
empathy-filter-glue.h: empathy-filter.xml Makefile
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=empathy_filter --mode=glib-server --output=$@ $<

tp-stream-engine-gen.h: tp-stream-engine.xml Makefile.am
	$(LIBTOOL) --mode=execute $(DBUS_BINDING_TOOL) --prefix=tp_stream_engine --mode=glib-client --output=$@ $<


empathy-enum-types.h: stamp-empathy-enum-types.h
	@true
stamp-empathy-enum-types.h: $(libempathy_headers) Makefile
	(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __LIBEMPATHY_ENUM_TYPES_H__\n" \
			--fhead "#define __LIBEMPATHY_ENUM_TYPES_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __LIBEMPATHY_ENUM_TYPES_H__ */\n" \
			--fprod "#include <libempathy/@filename@>\n" \
			--eprod "#define EMPATHY_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(libempathy_headers) ) > xgen-gth \
	&& (cmp -s xgen-gth empathy-enum-type.h || cp xgen-gth empathy-enum-types.h) \
	&& rm -f xgen-gth \
	&& echo timestamp > $(@F)

empathy-enum-types.c: $(libempathy_headers) Makefile
	(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <config.h>\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"empathy-enum-types.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(libempathy_headers) ) > xgen-gtc \
	&& cp xgen-gtc $(@F) \
	&& rm -f xgen-gtc

dtddir = $(datadir)/empathy
dtd_DATA = 					\
	empathy-status-presets.dtd		\
	empathy-contact-groups.dtd		\
	empathy-chatroom-manager.dtd

stylesheetdir = $(datadir)/empathy
stylesheet_DATA =		\
	empathy-log-manager.xsl

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libempathy.pc

EXTRA_DIST = 			\
	empathy-marshal.list	\
	empathy-chandler.xml	\
	empathy-filter.xml	\
	tp-stream-engine.xml	\
	$(stylesheet_DATA)	\
	$(dtd_DATA)

CLEANFILES = 				\
	$(BUILT_SOURCES)		\
	stamp-empathy-enum-types.h
