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
