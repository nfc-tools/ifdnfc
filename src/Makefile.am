IFDNFC_LIB = libifdnfc.$(DYN_LIB_EXT)

lib_LTLIBRARIES = libifdnfc.la
libifdnfc_la_SOURCES = ifd-nfc.c atr.c
libifdnfc_la_LIBADD = $(LIBNFC_LIBS) $(PCSC_LIBS)
libifdnfc_la_CFLAGS = $(LIBNFC_CFLAGS) $(PCSC_CFLAGS)

bin_PROGRAMS = ifdnfc-activate
ifdnfc_activate_SOURCES = ifdnfc-activate.c
ifdnfc_activate_LDADD = $(PCSC_LIBS)
ifdnfc_activate_CFLAGS = $(PCSC_CFLAGS)

noinst_HEADERS = ifd-nfc.h atr.h

EXTRA_DIST = reader.conf.in


install: install_ifdnfc install_ifdnfc_activate

install_ifdnfc: libifdnfc.la
	$(mkinstalldirs) $(DESTDIR)$(serialdropdir)
	$(LIBTOOL) --mode=install cp .libs/$(IFDNFC_LIB) $(DESTDIR)$(serialdropdir)/$(IFDNFC_LIB).$(VERSION)
	ln -fs $(IFDNFC_LIB).$(VERSION) $(DESTDIR)$(serialdropdir)/$(IFDNFC_LIB)
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/reader.conf.d
	name=`grep IFDNFC_READER_NAME $(srcdir)/ifd-nfc.h`; \
		name=$${name##*IFDNFC_READER_NAME}; \
		perl -ne "s|TARGETNAME|$${name}| ; print" $(srcdir)/reader.conf.in > tmp ; \
		perl -ne "s|TARGETPATH|$(serialdropdir)/$(IFDNFC_LIB)| ; print" tmp > $(DESTDIR)$(sysconfdir)/reader.conf.d/ifdnfc ; \
		rm -f tmp;

install_ifdnfc_activate: ifdnfc-activate
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	$(LIBTOOL) --mode=install cp ifdnfc-activate $(DESTDIR)$(bindir)


uninstall: uninstall_ifdnfc uninstall_ifdnfc_activate

uninstall_ifdnfc:
	rm -f $(DESTDIR)$(serialdropdir)/$(IFDNFC_LIB).$(VERSION) \
	    $(DESTDIR)$(serialdropdir)/$(IFDNFC_LIB)

uninstall_ifdnfc_activate:
	rm -f $(DESTDIR)$(bindir)/ifdnfc-activate
