## Process this file with automake to produce Makefile.in

SUBDIRS =

NULL =

EXTRA_DIST = \
	$(NULL)

AM_CPPFLAGS = \
	-I$(top_builddir) -I$(top_srcdir)	 		\
	-I$(top_builddir)/src -I$(top_srcdir)/src 		\
	-DSRCDIR=\""$(abs_srcdir)"\"				\
	-DBUILDDIR=\""$(abs_builddir)"\"			\
	$(POLKIT_GOBJECT_1_CFLAGS) 				\
	$(GUDEV_CFLAGS) 					\
	$(GLIB_CFLAGS) 						\
	$(GIO_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(NULL)

# ----------------------------------------------------------------------------------------------------

TESTS =

noinst_PROGRAMS =

all:
	@true