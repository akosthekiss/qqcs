# Copyright (c) 2026 Akos Kiss.
#
# Licensed under the BSD 3-Clause License
# <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
# This file may not be copied, modified, or distributed except
# according to those terms.

qt_add_executable(smoke_single_stream main.cpp)
target_link_libraries(smoke_single_stream PRIVATE qqcs_camera PkgConfig::GST)
target_include_directories(smoke_single_stream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
