add_library(qqcs_navigation STATIC
    NavMath.h
    NavigationController.h
    NavigationController.cpp
    ../input/InputAction.h
)

target_include_directories(qqcs_navigation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_navigation PUBLIC Qt6::Core)
