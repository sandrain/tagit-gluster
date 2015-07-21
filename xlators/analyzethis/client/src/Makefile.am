xlator_LTLIBRARIES = client.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/analyzethis

client_la_LDFLAGS = -module -avoid-version
client_la_SOURCES = client.c
client_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

# other external libraries?
#client_la_LIBADD +=

noinst_HEADERS = client.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES =

