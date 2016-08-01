xlator_LTLIBRARIES = imess-client.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/features

imess_client_la_LDFLAGS = -module -avoid-version

imess_client_la_SOURCES = imess-client.c
imess_client_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la \
			 $(SQLITE_LIBS)

noinst_HEADERS = imess-client.h imess-client-mem-types.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS) $(SQLITE_CFLAGS)

CLEANFILES =

