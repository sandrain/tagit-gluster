xlator_LTLIBRARIES = imess.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/features

imess_la_LDFLAGS = -module -avoid-version

imess_la_SOURCES = imess.c xdb.c xdb-schema.c

imess_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
		  $(SQLITE_LIBS)

#imess_la_LIBADD +=

noinst_HEADERS = imess.h imess-mem-types.h xdb.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS) $(SQLITE_CFLAGS)

xdb-schema.c: xdb-schema.sql
        @( echo "const char *xdb_schema_sqlstr = ";\
           sed 's/^/"/; s/$$/\\n"/' < $< ;\
           echo ";" ) > $@

#CLEANFILES = xdb-schema.c
CLEANFILES =

