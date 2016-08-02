#!/bin/bash

./autogen.sh


prefix=/usr

./configure \
--prefix=$prefix \
--enable-debug \
--disable-glupy \
--disable-qemu-block \
--disable-bd-xlator \
--disable-xml-output \
--disable-georeplication \
--enable-imess

#prefix=$HOME/experiments/tagit/prefix
#
#./configure PKG_CONFIG_PATH=$HOME/sw/lib/pkgconfig \
#--prefix=$prefix \
#--exec-prefix=$prefix \
#--with-mountutildir=$prefix/sbin \
#--with-initdir=$prefix/etc/init.d \
#--sysconfdir=$prefix/etc \
#--localstatedir=/var \
#--enable-debug \
#--disable-glupy \
#--disable-qemu-block \
#--disable-bd-xlator \
#--disable-xml-output \
#--disable-georeplication \
#--enable-imess

#make -j5

