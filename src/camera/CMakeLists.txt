add_library(qqcs_camera STATIC
    CameraState.h
    CameraListModel.h
    CameraListModel.cpp
    CameraManager.h
    CameraManager.cpp
)

target_include_directories(qqcs_camera PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_camera PUBLIC Qt6::Core qqcs_config)
