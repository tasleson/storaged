
NULL =

man_MANS =

if ENABLE_MAN
man_MANS += \
	udisks-lvm.8 \
	$(NULL)
endif

udisks-lvm.8 : udisks-lvm.xml
	$(XSLTPROC) -path "$(builddir)/.." -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

EXTRA_DIST= \
	udisks-lvm.xml \
	$(NULL)

clean-local:
	rm -f *~ *.[18]
