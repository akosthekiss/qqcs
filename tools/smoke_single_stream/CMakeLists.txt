qt_add_executable(smoke_single_stream main.cpp)
target_link_libraries(smoke_single_stream PRIVATE qqcs_camera PkgConfig::GST)
target_include_directories(smoke_single_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
