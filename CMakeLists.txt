cmake_minimum_required(VERSION 3.16)

project(dde-file-manager-vcs-plugin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Find required packages
find_package(PkgConfig REQUIRED)

# Find Qt version - check for USE_QT6 option first
if (DEFINED USE_QT6)
    if (USE_QT6)
        set(QT_VERSION_MAJOR 6)
        message("   >>> Using Qt6 (forced by USE_QT6=ON)")
    else()
        set(QT_VERSION_MAJOR 5)
        message("   >>> Using Qt5 (forced by USE_QT6=OFF)")
    endif()
elseif (NOT DEFINED QT_VERSION_MAJOR)
    # Try to find Qt6 first
    find_package(Qt6 QUIET COMPONENTS Core)
    if (Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
        message("   >>> Auto detected Qt version: 6")
    else()
        # Fallback to Qt5
        find_package(Qt5 QUIET COMPONENTS Core)
        if (Qt5_FOUND)
            set(QT_VERSION_MAJOR 5)
            message("   >>> Auto detected Qt version: 5")
        else()
            message(FATAL_ERROR "Neither Qt6 nor Qt5 found!")
        endif()
    endif()
else()
    message("   >>> Using specified Qt version: ${QT_VERSION_MAJOR}")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Core5Compat)
    set(QT_VERSION_SUFFIX "-qt6")
    add_compile_definitions(USE_QT6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    set(QT_VERSION_SUFFIX "-qt5")
endif()

find_package(dfm-extension REQUIRED)

# Set CMAKE_INSTALL_LIBDIR
if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif()

add_subdirectory(src/git)
