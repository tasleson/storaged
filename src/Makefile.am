## Process this file with automake to produce Makefile.in

SUBDIRS = . tests

NULL =

EXTRA_DIST =

AM_CPPFLAGS = \
	-I$(top_builddir) -I$(top_srcdir)	 		\
	-DPACKAGE_LIBEXEC_DIR=\""$(libexecdir)"\" 		\
	-DPACKAGE_SYSCONF_DIR=\""$(sysconfdir)"\" 		\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" 			\
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" 			\
	-DPACKAGE_LOCALSTATE_DIR=\""$(localstatedir)"\" 	\
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" 		\
	-DPACKAGE_LIB_DIR=\""$(prefix)/lib"\" 			\
	-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT			\
	$(POLKIT_GOBJECT_1_CFLAGS) 				\
	$(GUDEV_CFLAGS) 					\
	$(GLIB_CFLAGS) 						\
	$(GIO_CFLAGS)						\
	$(WARN_CFLAGS)						\
	$(NULL)

# ----------------------------------------------------------------------------------------------------

$(dbus_built_sources) : Makefile.am $(top_srcdir)/data/com.redhat.lvm2.xml
	gdbus-codegen \
		--interface-prefix com.redhat.lvm2 \
		--c-namespace Lvm \
		--c-generate-object-manager \
		--generate-c-code com.redhat.lvm2 \
		$(top_srcdir)/data/com.redhat.lvm2.xml

dbus_built_sources = \
	com.redhat.lvm2.h com.redhat.lvm2.c \
	$(NULL)

BUILT_SOURCES = $(dbus_built_sources)

# ----------------------------------------------------------------------------------------------------

noinst_LTLIBRARIES = libstoraged.la

libstoraged_la_SOURCES = \
	types.h \
	block.h block.c \
	daemon.h daemon.c \
	invocation.h invocation.c \
	job.h job.c \
	logicalvolume.h logicalvolume.c \
	manager.h manager.c \
	physicalvolume.h physicalvolume.c \
	spawnedjob.h spawnedjob.c \
	threadedjob.h threadedjob.c \
	util.h util.c \
	volumegroup.h volumegroup.c \
	$(dbus_built_sources) \
	$(NULL)

libstoraged_la_CFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"storaged\" \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GUDEV_CFLAGS) \
	$(POLKIT_GOBJECT_1_CFLAGS) \
	$(UDISKS_CFLAGS) \
	$(NULL)

libstoraged_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(GUDEV_LIBS) \
	$(POLKIT_GOBJECT_1_LIBS) \
	$(UDISKS_LIBS) \
	$(NULL)

# ----------------------------------------------------------------------------------------------------

storagedprivdir = $(libdir)/storaged
storagedpriv_PROGRAMS = storaged storaged-lvm-helper

storaged_SOURCES = \
	main.c \
	$(NULL)

storaged_CFLAGS = \
	-DG_LOG_DOMAIN=\"storaged\" \
	$(UDISKS_CFLAGS) \
	$(NULL)

storaged_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	libstoraged.la \
	$(NULL)

storaged_lvm_helper_SOURCES = \
	helper.c \
	$(NULL)

storaged_lvm_helper_CFLAGS = \
	$(LVM2_CFLAGS) \
	$(NULL)

storaged_lvm_helper_LDADD = \
	$(GLIB_LIBS) \
	$(LVM2_LIBS) \
	-llvm2app \
	$(NULL)
