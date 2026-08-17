# Copyright (c) 2026 Akos Kiss.
#
# Licensed under the BSD 3-Clause License
# <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
# This file may not be copied, modified, or distributed except
# according to those terms.

add_library(qqcs_navigation STATIC
    NavMath.h
    NavigationController.h
    NavigationController.cpp
    ../input/InputAction.h
)

target_include_directories(qqcs_navigation PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_navigation PUBLIC Qt6::Core)
