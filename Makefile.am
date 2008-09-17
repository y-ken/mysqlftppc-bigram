# Copyright (C) 2008 Hiroaki Kawai

pkgplugindir=@MYSQL_PLUGINDIR@
INCLUDES= -I$(top_builddir)/include -I$(top_srcdir)/include @MYSQL_INCLUDES@
#noinst_LTLIBRARIES= mypluglib.la
pkgplugin_LTLIBRARIES= libftbigram.la
libftbigram_la_SOURCES= ftnorm.c ftbool.c plugin_bigram.c
libftbigram_la_LDFLAGS= -module -rpath $(pkgplugindir)
libftbigram_la_CFLAGS= -DMYSQL_DYNAMIC_PLUGIN

