
NULL =

man_MANS =

if ENABLE_MAN
man_MANS += \
	udisksd-lvm.8 \
	$(NULL)
endif

udisksd-lvm.8 : udisksd-lvm.xml
	$(XSLTPROC) -path "$(builddir)/.." -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

EXTRA_DIST= \
	udisksd-lvm.xml \
	$(NULL)

clean-local:
	rm -f *~ *.[18]
