qt_add_executable(smoke_multi_stream main.cpp)
target_link_libraries(smoke_multi_stream PRIVATE qqcs_camera PkgConfig::GST)
target_include_directories(smoke_multi_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
