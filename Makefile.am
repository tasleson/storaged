## Process this file with automake to produce Makefile.in

NULL =

SUBDIRS = data src po doc

EXTRA_DIST = 			\
	HACKING 		\
	$(NULL)

DISTCHECK_CONFIGURE_FLAGS=							\
	--enable-gtk-doc 							\
	--disable-introspection							\
	--with-udevdir=$$dc_install_base/$(udevdir)				\
	--with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)	\
	$(NULL)

clean-local :
	rm -f *~
