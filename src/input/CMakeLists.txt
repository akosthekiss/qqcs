add_library(qqcs_input STATIC
    InputAction.h
    KeyboardMapping.h
    InputManager.h
    InputManager.cpp
)

target_include_directories(qqcs_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_input PUBLIC Qt6::Core)
