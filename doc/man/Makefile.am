
NULL =

man_MANS =

if ENABLE_MAN
man_MANS += \
	storaged.8 \
	$(NULL)
endif

storaged.8 : storaged.xml
	$(XSLTPROC) -path "$(builddir)/.." -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

EXTRA_DIST= \
	storaged.xml \
	$(NULL)

clean-local:
	rm -f *~ *.[18]
