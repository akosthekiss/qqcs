# Copyright (c) 2026 Akos Kiss.
#
# Licensed under the BSD 3-Clause License
# <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
# This file may not be copied, modified, or distributed except
# according to those terms.

add_library(qqcs_camera STATIC
    CameraState.h
    CameraListModel.h
    CameraListModel.cpp
    CameraManager.h
    CameraManager.cpp
    PlatformGstElements.h
    PlatformGstElements.cpp
    AppSinkVideoItem.h
    AppSinkVideoItem.cpp
    VideoFillMath.h
    ReconnectScheduler.h
    ReconnectScheduler.cpp
    RtspStreamPipeline.h
    RtspStreamPipeline.cpp
)

target_include_directories(qqcs_camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_camera
    PUBLIC Qt6::Core Qt6::Quick Qt6::Qml qqcs_config
    PRIVATE PkgConfig::GST
)
