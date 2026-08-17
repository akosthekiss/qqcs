# Copyright (c) 2026 Akos Kiss.
#
# Licensed under the BSD 3-Clause License
# <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
# This file may not be copied, modified, or distributed except
# according to those terms.

add_library(qqcs_input STATIC
    InputAction.h
    KeyboardMapping.h
    InputManager.h
    InputManager.cpp
)

target_include_directories(qqcs_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_input PUBLIC Qt6::Core)
