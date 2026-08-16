add_library(qqcs_config STATIC
    AppConfig.h
    ConfigLoader.h
    ConfigLoader.cpp
    ConfigModel.h
    ConfigModel.cpp
)

target_include_directories(qqcs_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(qqcs_config
    PUBLIC Qt6::Core
    PRIVATE yaml-cpp::yaml-cpp
)
