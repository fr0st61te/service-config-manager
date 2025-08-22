SUMMARY = "service manager for services"
DESCRIPTION = "service manager"
PV = "1.0"
PR = "r1"

# License info
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=86d3f3a95c324c9479bd8986968f4327"
DEPENDS = " \
    boost \
    phosphor-logging \
    phosphor-settings-manager \
    sdbusplus \
    systemd \
    nlohmann-json \
    "

S = "${WORKDIR}/git"
SRC_URI = "git://github.com/fr0st61te/service-config-manager;branch=main;protocol=https"
SRCREV = "f6a3b20d8848913f0169d996204d3e8514d2edc7"

SYSTEMD_SERVICE:${PN} += "srvcfg-manager.service"

inherit meson pkgconfig
inherit obmc-phosphor-systemd

do_install(){
    install -d ${D}${sysconfdir}
    install -d ${D}${bindir}
    install -m 0755 ${B}/phosphor-srvcfg-manager ${D}${bindir}
}
