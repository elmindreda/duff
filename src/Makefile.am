
AM_CPPFLAGS = -I$(top_srcdir)/lib -DLOCALEDIR=\"$(localedir)\"

bin_PROGRAMS = duff

duff_SOURCES = duff.c duffdriver.c duffentry.c duffstring.c duffutil.c sha1.c sha256.c sha384.c sha512.c
duff_LDADD = @LIBINTL@

noinst_HEADERS = duff.h duffstring.h sha1.h sha256.h sha384.h sha512.h

