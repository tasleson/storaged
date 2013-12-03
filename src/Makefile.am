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
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" 			\
	-D_POSIX_PTHREAD_SEMANTICS -D_REENTRANT			\
	-DUDISKS_COMPILATION					\
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

noinst_LTLIBRARIES = libudisks-lvm.la

libudisks_lvm_la_SOURCES = \
	basejob.h basejob.c \
	blockobject.h blockobject.c \
	daemon.h daemon.c \
	logicalvolume.h logicalvolume.c \
	logicalvolumeobject.h logicalvolumeobject.c \
	manager.h manager.c \
	physicalvolume.h physicalvolume.c \
	spawnedjob.h spawnedjob.c \
	util.h util.c \
	volumegroup.h volumegroup.c \
	volumegroupobject.h volumegroupobject.c \
	$(dbus_built_sources) \
	$(NULL)

libudisks_lvm_la_CFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"udisks-lvm\" \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GUDEV_CFLAGS) \
	$(POLKIT_GOBJECT_1_CFLAGS) \
	$(UDISKS_CFLAGS) \
	$(NULL)

libudisks_lvm_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	$(GUDEV_LIBS) \
	$(POLKIT_GOBJECT_1_LIBS) \
	$(UDISKS_LIBS) \
	$(NULL)

# ----------------------------------------------------------------------------------------------------

udisksdprivdir = $(prefix)/lib/udisks2
udisksdpriv_PROGRAMS = udisks-lvm udisks-lvm-helper

udisks_lvm_SOURCES = \
	main.c \
	$(NULL)

udisks_lvm_CFLAGS = \
	-DG_LOG_DOMAIN=\"udisks-lvm\" \
	$(UDISKS_CFLAGS) \
	$(NULL)

udisks_lvm_LDADD = \
	$(GLIB_LIBS) \
	$(GIO_LIBS) \
	libudisks-lvm.la \
	$(NULL)

udisks_lvm_helper_SOURCES = \
	helper.c \
	$(NULL)

udisks_lvm_helper_CFLAGS = \
	$(LVM2_CFLAGS) \
	$(NULL)

udisks_lvm_helper_LDADD = \
	$(GLIB_LIBS) \
	$(LVM2_LIBS) \
	-llvm2app \
	$(NULL)
