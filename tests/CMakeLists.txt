qt_add_executable(test_configloader test_configloader.cpp)
target_link_libraries(test_configloader PRIVATE Qt6::Test qqcs_config)
target_include_directories(test_configloader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_configloader COMMAND test_configloader)
