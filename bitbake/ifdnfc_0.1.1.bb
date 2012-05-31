DESCRIPTION = "PC/SC chip card interface driver to use NFC devices supported by libnfc via PC/SC"
HOMEPAGE = "http://sourceforge.net/projects/ifdnfc/"
LICENSE = "GPL"

DEPENDS = "libnfc pcsc-lite"
RDEPENDS = "libnfc pcsc-lite"

SRC_URI = "${SOURCEFORGE_MIRROR}/ifdnfc/ifdnfc_${PV}.tar.gz"
S = "${WORKDIR}/ifdnfc_${PV}"

EXTRA_OECONF = "    --prefix=${libdir} \
                    --enable-serialdropdir=/pcsc/drivers/serial"

inherit autotools_stage pkgconfig

FILES_${PN} += "${libdir}/pcsc/drivers/serial/* ${sysconfdir}/* ${bindir}/*"

SRC_URI[md5sum] = "1817b572bc3b1fdbc17b9be5fec96284"
SRC_URI[sha256sum] = "3cea0ef8723973fac078dace17cbfc7aa7830ff8e9b6bfe28fe89ed0e0c34167"
