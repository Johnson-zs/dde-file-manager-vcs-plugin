cmake_minimum_required(VERSION 3.14)

project(dde-file-manager-vcs-plugin)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-g -std=c++17 -lstdc++fs")

find_package(Qt6 REQUIRED COMPONENTS Core Core5Compat)
find_package(dfm-extension REQUIRED)

include(GNUInstallDirs)
if(NOT DEFINED LIB_INSTALL_DIR)
    set(LIB_INSTALL_DIR ${CMAKE_INSTALL_FULL_LIBDIR})
endif()

if(NOT DEFINED DFM_EXT_PLUGIN_DIR)
    set(DFM_EXT_PLUGIN_DIR ${LIB_INSTALL_DIR}/dde-file-manager/plugins/extensions)
endif()

add_subdirectory(src/git)
