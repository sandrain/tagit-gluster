bin_PROGRAMS = ixsql ixview

noinst_HEADERS = ixsql.h

ixsql_SOURCES = ixsql.c ixsql-cmd.c

ixsql_LDADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
	      $(top_builddir)/api/src/libgfapi.la \
	      $(GF_LDADD) $(RLLIBS)

ixview_SOURCES = ixview.c imess-view-db-schema.c
BUILT_SOURCES = imess-view-db-schema.c

ixview_LDADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
	       $(top_builddir)/api/src/libgfapi.la \
	       $(GF_LDADD) $(RLLIBS) $(SQLITE_LIBS)

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
	      -I$(top_srcdir)/api/src

AM_CFLAGS = -Wall $(GF_CFLAGS) $(SQLITE_CFLAGS) -fno-stack-protector

EXTRA_DIST = imess-view-db-schema.sql

imess-view-db-schema.c: imess-view-db-schema.sql
	@( echo "const char *view_db_schema_sqlstr = ";\
           sed 's/^/"/; s/$$/\\n"/' < $< ;\
           echo ";" ) > $@

CLEANFILES = $(bin_PROGRAMS)

$(top_builddir)/libglusterfs/src/libglusterfs.la:
	$(MAKE) -C $(top_builddir)/libglusterfs/src/ all

$(top_builddir)/api/src/libgfapi.la:
	$(MAKE) -C $(top_builddir)/api/src/ all

