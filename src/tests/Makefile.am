## Process this file with automake to produce Makefile.in

SUBDIRS =

NULL =

EXTRA_DIST = \
	$(NULL)

AM_CPPFLAGS = \
	-I$(top_builddir) -I$(top_srcdir)	 		\
	-I$(top_builddir)/src -I$(top_srcdir)/src 		\
	-DSRCDIR=\""$(abs_top_srcdir)"\"			\
	-DBUILDDIR=\""$(abs_top_builddir)"\"			\
	$(POLKIT_GOBJECT_1_CFLAGS) 				\
	$(GUDEV_CFLAGS) 					\
	$(GLIB_CFLAGS) 						\
	$(GIO_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(NULL)

LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	libtesting.la \
	$(NULL)

# ----------------------------------------------------------------------------------------------------

noinst_LTLIBRARIES = libtesting.la

libtesting_la_SOURCES = \
	testing.h testing.c \
	$(NULL)

TEST_PROGS = \
	test-block \
	test-react \
	test-methods \
	$(NULL)

TESTS = \
	$(TEST_PROGS) \
	$(NULL)

noinst_PROGRAMS = \
	$(TEST_PROGS) \
	$(NULL)
