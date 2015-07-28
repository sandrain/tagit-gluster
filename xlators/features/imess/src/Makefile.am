xlator_LTLIBRARIES = imess.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/features/imess

imess_la_LDFLAGS = -module -avoid-version
imess_la_SOURCES = imess.c
imess_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

#imess_la_LIBADD +=

noinst_HEADERS = imess.h

AM_CPPFLAGS = $(GF_CPPFLAGS) -I$(top_srcdir)/libglusterfs/src

AM_CFLAGS = -Wall $(GF_CFLAGS)

CLEANFILES =
