qt_add_executable(test_configloader test_configloader.cpp)
target_link_libraries(test_configloader PRIVATE Qt6::Test qqcs_config)
target_include_directories(test_configloader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_configloader COMMAND test_configloader)

qt_add_executable(test_cameramanager test_cameramanager.cpp)
target_link_libraries(test_cameramanager PRIVATE Qt6::Test qqcs_camera)
target_include_directories(test_cameramanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_cameramanager COMMAND test_cameramanager)

qt_add_executable(test_navigation test_navigation.cpp)
target_link_libraries(test_navigation PRIVATE Qt6::Test qqcs_navigation)
target_include_directories(test_navigation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_navigation COMMAND test_navigation)
