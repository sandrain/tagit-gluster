xlator_LTLIBRARIES = server.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/analyzethis

server_la_LDFLAGS = -module -avoid-version
server_la_SOURCES = server.c
server_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

# other external libraries?
#server_la_LIBADD +=

noinst_HEADERS = server.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES =

