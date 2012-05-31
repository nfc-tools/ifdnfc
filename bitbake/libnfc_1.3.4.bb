DESCRIPTION = "Public platform independent Near Field Communication (NFC) library"
HOMEPAGE = "http://www.libnfc.org"
LICENSE = "GPL"

DEPENDS = "libusb pcsc-lite ccid"
RDEPENDS = "libusb libpcsclite ccid"

SRC_URI = "http://libnfc.googlecode.com/files/libnfc-${PV}.tar.gz"

inherit autotools_stage pkgconfig

FILES_${PN} += "${libdir}/*"

SRC_URI[md5sum] = "f89f58e70b72bf4aac0567d0741719c8"
SRC_URI[sha256sum] = "143266d8a542eedca6f24b7c6d9355e00d541c3914cf941c1be9d5212c764b86"
