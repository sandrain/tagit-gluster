xlator_LTLIBRARIES = imess.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/features

imess_la_LDFLAGS = -module -avoid-version

imess_la_SOURCES = imess.c xdb.c

nodist_imess_la_SOURCES = xdb-schema.c

BUILT_SOURCES = xdb-schema.c

imess_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
		  $(SQLITE_LIBS)

noinst_HEADERS = imess.h imess-mem-types.h xdb.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS) $(SQLITE_CFLAGS)

EXTRA_DIST = xdb-schema.sql

xdb-schema.c: xdb-schema.sql
	@( echo "const char *xdb_schema_sqlstr = ";\
           sed 's/^/"/; s/$$/\\n"/' < $< ;\
           echo ";" ) > $@

CLEANFILES = xdb-schema.c

