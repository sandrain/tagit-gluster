bin_PROGRAMS = ixfind

ixfind_SOURCES = ixfind.c

noinst_HEADERS = ixfind.h

ixfind_LDADD = $(top_builddir)/libglusterfs/src/libglusterfs.la $(GF_LDADD) \
	       $(RLLIBS) $(top_builddir)/rpc/xdr/src/libgfxdr.la \
	       $(top_builddir)/rpc/rpc-lib/src/libgfrpc.la \
	       $(top_builddir)/api/src/libgfapi.la \
	       $(XML_LIBS) \
	       $(libgfapi_la_LIBADD) $(api_la_LIBADD)

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src \
	      -I$(top_srcdir)/api/src

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES = $(bin_PROGRAMS)

$(top_builddir)/libglusterfs/src/libglusterfs.la:
	$(MAKE) -C $(top_builddir)/libglusterfs/src/ all

$(top_builddir)/api/src/libgfapi.la:
	$(MAKE) -C $(top_builddir)/api/src/ all


