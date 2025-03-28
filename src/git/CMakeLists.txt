set(BIN_NAME dfm-extension-git)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*"
    "./*.h"
    "./*.cpp"
    "../cache.cpp"
)

add_library(${BIN_NAME} SHARED ${SRCS}
    )

target_include_directories(${BIN_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(${BIN_NAME}
    PRIVATE
    ${dfm-extension_LIBRARIES}
    Qt6::Core
    Qt6::Core5Compat
    stdc++fs
    )

install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR})
