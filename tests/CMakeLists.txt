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

qt_add_executable(test_reconnectscheduler test_reconnectscheduler.cpp)
target_link_libraries(test_reconnectscheduler PRIVATE Qt6::Test qqcs_camera)
target_include_directories(test_reconnectscheduler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_reconnectscheduler COMMAND test_reconnectscheduler)

qt_add_executable(test_inputmanager test_inputmanager.cpp)
target_link_libraries(test_inputmanager PRIVATE Qt6::Test qqcs_input)
target_include_directories(test_inputmanager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_inputmanager COMMAND test_inputmanager)

qt_add_executable(test_cecmapping test_cecmapping.cpp)
target_link_libraries(test_cecmapping PRIVATE Qt6::Test qqcs_cec)
target_include_directories(test_cecmapping PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_cecmapping COMMAND test_cecmapping)
