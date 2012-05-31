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

SRC_URI[md5sum] = "7147af04e24a440e42c8a26c254362b8"
SRC_URI[sha256sum] = "6dca8fcaa48b1b7c3916376f79b06ecca5dfb8c3488c5e1763e3411cdc6270ba"
