bin_PROGRAMS = ixfind ixsql

ixfind_SOURCES = ixfind.c

noinst_HEADERS = ixsql.h

ixfind_LDADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
	       $(top_builddir)/api/src/libgfapi.la \
	       $(GF_LDADD) $(RLLIBS)

ixsql_SOURCES = ixsql.c ixsql-cmd.c

ixsql_LDADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
	      $(top_builddir)/api/src/libgfapi.la \
	      $(GF_LDADD) $(RLLIBS)


AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
	      -I$(top_srcdir)/api/src

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES = $(bin_PROGRAMS)

$(top_builddir)/libglusterfs/src/libglusterfs.la:
	$(MAKE) -C $(top_builddir)/libglusterfs/src/ all

$(top_builddir)/api/src/libgfapi.la:
	$(MAKE) -C $(top_builddir)/api/src/ all

