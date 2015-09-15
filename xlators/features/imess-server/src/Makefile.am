xlator_LTLIBRARIES = imess-server.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/features

imess_server_la_LDFLAGS = -module -avoid-version
imess_server_la_SOURCES = imess-server.c imess-server-xdb.c
nodist_imess_server_la_SOURCES = imess-server-xdb-schema.c
BUILT_SOURCES = imess-server-xdb-schema.c
imess_server_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
		         $(SQLITE_LIBS)

noinst_HEADERS = imess-server.h imess-server-mem-types.h imess-server-xdb.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS) $(SQLITE_CFLAGS)

EXTRA_DIST = imess-server-xdb-schema.sql

imess-server-xdb-schema.c: imess-server-xdb-schema.sql
	@( echo "const char *ims_xdb_schema_sqlstr = ";\
           sed 's/^/"/; s/$$/\\n"/' < $< ;\
           echo ";" ) > $@

CLEANFILES = imess-server-xdb-schema.c


