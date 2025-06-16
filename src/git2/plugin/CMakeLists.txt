# Git文件管理器插件（精简版）
project(dfm-extension-git2)

# 查找dfm-extension依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(DFM_EXT REQUIRED dfm-extension)

# 设置头文件
set(PLUGIN_HEADERS
    include/git-emblem-plugin.h
    include/git-menu-plugin.h
    include/git-window-plugin.h
    include/git-local-cache.h
)

# 设置源文件
set(PLUGIN_SOURCES
    src/dfm-extension-git.cpp
    src/git-emblem-plugin.cpp
    src/git-menu-plugin.cpp
    src/git-window-plugin.cpp
    src/git-local-cache.cpp
    dbus/git-dbus-client.cpp
)

# DBus接口生成 - 依赖daemon生成的XML
# 注意：这里使用构建目录中生成的XML文件
qt_add_dbus_interface(PLUGIN_SOURCES
    ${CMAKE_BINARY_DIR}/src/git2/daemon/org.deepin.FileManager.Git.xml
    gitdbus_interface
)

# 创建共享库
add_library(${PROJECT_NAME} SHARED
    ${PLUGIN_HEADERS}
    ${PLUGIN_SOURCES}
)

# 添加依赖关系，确保daemon的XML首先生成
add_dependencies(${PROJECT_NAME} dde-file-manager-git-daemon)

# 设置包含目录
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}  # 用于生成的DBus文件
        ${COMMON_INCLUDE_DIR}
        ${DFM_EXT_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::DBus
        dde-git-common
        ${DFM_EXT_LIBRARIES}
)

# 设置编译属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    SOVERSION 1
    VERSION 1.0.0
)

# 设置编译定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GIT_PLUGIN_PROCESS=1
)

# 设置编译选项
target_compile_options(${PROJECT_NAME} PRIVATE ${DFM_EXT_CFLAGS_OTHER})

# 安装插件库文件
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR}
    COMPONENT extension
) 