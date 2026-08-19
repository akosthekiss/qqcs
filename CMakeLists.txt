# Copyright (c) 2026 Akos Kiss.
#
# Licensed under the BSD 3-Clause License
# <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
# This file may not be copied, modified, or distributed except
# according to those terms.

# Overridable at configure time (-DQQCS_VERSION=1.2.3, or
# 1.2.3.5.g1a2b3c4 for a tag-less manual release build -- see
# release.yml) so CI can stamp a release build with the version derived
# from the pushed git tag, without needing a source-code edit/commit per
# release. "0.0.0" is a deliberately obvious non-release placeholder for
# ordinary from-source/desktop builds.
#
# QQCS_VERSION itself becomes the actual package version (CPACK_PACKAGE_
# VERSION, set explicitly further down) -- but project(VERSION ...) only
# accepts strict numeric dotted components (max 4), so the "N.g<sha>"
# suffix above would make it fail outright. QQCS_CMAKE_VERSION strips any
# such suffix just for project()'s sake; nothing here actually reads
# PROJECT_VERSION for packaging purposes.
if (NOT DEFINED QQCS_VERSION)
    set(QQCS_VERSION "0.0.0")
endif()
string(REGEX MATCH "^[0-9]+(\\.[0-9]+){0,3}" QQCS_CMAKE_VERSION "${QQCS_VERSION}")
if (NOT QQCS_CMAKE_VERSION)
    set(QQCS_CMAKE_VERSION "0.0.0")
endif()

cmake_minimum_required(VERSION 3.21)
project(qqcs VERSION ${QQCS_CMAKE_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml Test)
qt_standard_project_setup(REQUIRES 6.8)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-video-1.0
    gstreamer-audio-1.0
    gstreamer-sdp-1.0
)

find_package(yaml-cpp CONFIG QUIET)
if (NOT yaml-cpp_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
    )
    FetchContent_MakeAvailable(yaml-cpp)
endif()

set(QQCS_IS_RPI FALSE)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm")
    set(QQCS_IS_RPI TRUE)
endif()
option(QQCS_ENABLE_CEC "Enable HDMI-CEC support via libCEC" ${QQCS_IS_RPI})

add_subdirectory(src/config)
add_subdirectory(src/camera)
add_subdirectory(src/input)
add_subdirectory(src/navigation)
add_subdirectory(src/cec)

qt_add_executable(qqcs
    src/main.cpp
)

qt_add_qml_module(qqcs
    URI QqcsUi
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Mosaic.qml
        qml/CameraTile.qml
        qml/Fullscreen.qml
        qml/StatusOverlay.qml
        qml/DiagnosticsOverlay.qml
)

target_link_libraries(qqcs PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    PkgConfig::GST
    yaml-cpp::yaml-cpp
    qqcs_config
    qqcs_camera
    qqcs_input
    qqcs_navigation
    qqcs_cec
)

include(GNUInstallDirs)
install(TARGETS qqcs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)

# "raspberrypi" component: everything needed for kiosk/appliance
# deployment (systemd unit + config templates) on top of the plain
# "runtime" component's binary. Desktop Linux packaging only ever needs
# "runtime" -- there is no desktop equivalent of an always-on systemd
# service or a KMS output config, per SPEC's desktop/keyboard-and-mouse
# use case (see README's "Building for Linux desktop" vs "Deploying to
# a Raspberry Pi"). Gated on Linux since systemd units/`/etc` layout
# make no sense on macOS.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(FILES deploy/raspberrypi/qqcs.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
        COMPONENT raspberrypi)
    # The live, admin-editable KMS config -- shipped as a real dpkg
    # conffile (see CPACK_DEBIAN_RASPBERRYPI_PACKAGE_CONFFILES below)
    # because, per real Raspberry Pi 5 testing, the shipped default
    # already works as-is; a second, pristine .example copy is kept
    # alongside it purely as a restore/reference point.
    install(FILES deploy/raspberrypi/kms.json.example
        DESTINATION /etc/qqcs
        RENAME kms.json
        COMPONENT raspberrypi)
    install(FILES deploy/raspberrypi/kms.json.example
        DESTINATION /etc/qqcs
        COMPONENT raspberrypi)
    # config.yaml, by contrast, is deliberately NEVER installed as a
    # live file -- there is no valid default camera list, so shipping
    # one that merely looks plausible would be worse than not shipping
    # one at all. Only the .example ever gets installed; postinst
    # explains the rest (see deploy/raspberrypi/debian/postinst).
    install(FILES config.example.yaml
        DESTINATION /etc/qqcs
        RENAME config.yaml.example
        COMPONENT raspberrypi)
endif()

enable_testing()
add_subdirectory(tests)
add_subdirectory(tools/smoke_single_stream)
add_subdirectory(tools/smoke_multi_stream)

# --- Packaging (CPack, Debian only for now) -------------------------------
#
# Two components -> two independent .debs from one build:
#   qqcs              -- just the binary (desktop Linux: user brings their
#                         own config.yaml and starts it by hand, no systemd
#                         unit or /etc/qqcs at all).
#   qqcs-raspberrypi   -- depends on qqcs; adds the systemd unit and the
#                         /etc/qqcs config templates/live KMS default for
#                         appliance-style deployment.
set(CPACK_PACKAGE_NAME "qqcs")
set(CPACK_PACKAGE_VENDOR "Akos Kiss")
set(CPACK_PACKAGE_CONTACT "Akos Kiss <akiss@inf.u-szeged.hu>")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/akosthekiss/qqcs")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt Quick Camera Station -- RTSP security camera monitor")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")
# Set explicitly, from the real QQCS_VERSION (not the stripped
# QQCS_CMAKE_VERSION used for project() above) -- otherwise the CPack
# module would derive this from PROJECT_VERSION_MAJOR/MINOR/PATCH once
# include(CPack) runs, losing any "N.g<sha>" suffix.
set(CPACK_PACKAGE_VERSION "${QQCS_VERSION}")

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT") # <package>_<version>_<arch>.deb
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON) # auto-detect Qt6/GStreamer/libCEC Depends:
set(CPACK_DEBIAN_PACKAGE_SECTION "video")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "qqcs")

set(CPACK_DEBIAN_RASPBERRYPI_PACKAGE_NAME "qqcs-raspberrypi")
set(CPACK_DEBIAN_RASPBERRYPI_PACKAGE_DEPENDS "qqcs (= ${CPACK_PACKAGE_VERSION})")
set(CPACK_DEBIAN_RASPBERRYPI_PACKAGE_CONFFILES "/etc/qqcs/kms.json")
set(CPACK_DEBIAN_RASPBERRYPI_PACKAGE_CONTROL_EXTRA
    "${CMAKE_SOURCE_DIR}/deploy/raspberrypi/debian/postinst"
    "${CMAKE_SOURCE_DIR}/deploy/raspberrypi/debian/prerm"
    "${CMAKE_SOURCE_DIR}/deploy/raspberrypi/debian/postrm"
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB")
endif()
# Default component selection mirrors QQCS_ENABLE_CEC's own default further
# up: arm64/Raspberry-relevant Linux builds package both components, other
# platforms (desktop Linux amd64, macOS) only the plain binary -- override
# with `cpack -D CPACK_COMPONENTS_ALL="runtime"` if a Pi-style package is
# ever wanted from a non-arm64 Linux build, or vice versa.
if (QQCS_IS_RPI)
    set(CPACK_COMPONENTS_ALL runtime raspberrypi)
else()
    set(CPACK_COMPONENTS_ALL runtime)
endif()

include(CPack)
