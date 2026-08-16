cmake_minimum_required(VERSION 3.21)
project(qqcs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Quick Qml Test)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

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

qt_add_executable(qqcs
    src/main.cpp
)

qt_add_qml_module(qqcs
    URI QqcsUi
    VERSION 1.0
    QML_FILES
        qml/main.qml
)

target_link_libraries(qqcs PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Qml
    PkgConfig::GST
    yaml-cpp::yaml-cpp
    qqcs_config
)

enable_testing()
add_subdirectory(tests)
