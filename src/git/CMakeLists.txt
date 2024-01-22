set(BIN_NAME dfm-extension-git)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "./*.h"
    "./*.cpp"
)

add_library(${BIN_NAME} SHARED ${SRCS})

target_link_libraries(${BIN_NAME}
    PRIVATE
    ${dfm-extension_LIBRARIES}
    Qt5::Core
    stdc++fs
    )

install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR})
