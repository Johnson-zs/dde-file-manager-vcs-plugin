cmake_minimum_required(VERSION 3.16)

project(dde-file-manager-git-plugin VERSION 2.0.0)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 依赖
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus)

# 启用Qt6的自动处理功能
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置全局包含目录 - 避免相对路径
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
set(DAEMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/daemon/include)
set(DIALOG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dialog/include)
set(PLUGIN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include)

# 添加全局编译定义
add_compile_definitions(
    QT_NO_KEYWORDS
    QT_DEPRECATED_WARNINGS
)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 添加子项目
add_subdirectory(common)
add_subdirectory(daemon)
add_subdirectory(dialog)
add_subdirectory(plugin)

# 添加测试目录
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# 安装规则
install(DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
        DESTINATION bin
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN "*")

install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
        DESTINATION lib
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN "*.so*") 
