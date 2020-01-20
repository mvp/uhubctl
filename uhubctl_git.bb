DESCRIPTION = "uhubctl - USB hub per-port power control"
HOMEPAGE = "https://github.com/mvp/uhubctl"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=b234ee4d69f5fce4486a80fdaf4a4263 \
                    file://LICENSE;md5=a79e6a142b69522fe7757fe7313895eb"

DEPENDS = "libusb1"
RDEPENDS_${PN} = "libusb1"

SRC_URI = "git://github.com/mvp/uhubctl.git"
SRCREV = "${AUTOREV}"
PV = "git"
S = "${WORKDIR}/git"

do_install_append () {
	install -d ${D}${bindir}
	install -Dm 0755 ${S}/uhubctl ${D}${bindir}/
}

FILES_${PN}  += "${bindir}/*"
