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

SRC_URI[md5sum] = "00ebf4173f17745cf395525e1583be2e"
SRC_URI[sha256sum] = "9da4676673cf9751a614330051a1d4ec1c24aca1857edbc10ec0a544cdaeba4f"
