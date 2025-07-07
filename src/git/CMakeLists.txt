set(BIN_NAME dfm-extension-git${QT_VERSION_SUFFIX})
file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*"
    "./*.h"
    "./*.cpp"
    "../cache.cpp"
)

add_library(${BIN_NAME} SHARED ${SRCS})

target_include_directories(${BIN_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/include)

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(${BIN_NAME}
        PRIVATE
        Qt6::Core
        Qt6::Core5Compat
        Qt6::Widgets
        Qt6::Gui
        stdc++fs
        ${dfm-extension_LIBRARIES}
    )
else()
    target_link_libraries(${BIN_NAME}
        PRIVATE
        Qt5::Core
        Qt5::Widgets
        Qt5::Gui
        stdc++fs
        ${dfm-extension_LIBRARIES}
    )
endif()

include(GNUInstallDirs)
if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
endif()

if(NOT DEFINED DFM_EXT_PLUGIN_DIR)
    set(DFM_EXT_PLUGIN_DIR ${LIB_INSTALL_DIR}/dde-file-manager/plugins/extensions)
endif()

install(TARGETS ${BIN_NAME} LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR})
