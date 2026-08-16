add_library(qqcs_cec STATIC
    CecButton.h
    CecMapping.h
    CecAdapter.h
)

if (QQCS_ENABLE_CEC)
    pkg_check_modules(LIBCEC REQUIRED IMPORTED_TARGET libcec)
    target_sources(qqcs_cec PRIVATE CecAdapter.cpp)
    target_link_libraries(qqcs_cec PRIVATE PkgConfig::LIBCEC)
else()
    target_sources(qqcs_cec PRIVATE CecAdapterStub.cpp)
endif()

target_include_directories(qqcs_cec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_cec PUBLIC Qt6::Core qqcs_input)
