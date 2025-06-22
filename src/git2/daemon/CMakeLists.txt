# Git状态服务守护进程
project(dde-file-manager-git-daemon)

# 设置头文件
set(DAEMON_HEADERS
    include/git-daemon.h
    include/git-status-cache.h
    include/git-repository-watcher.h
    include/git-service.h
    include/git-version-worker.h
)

# 设置库源文件（不包含main.cpp）
set(DAEMON_LIB_SOURCES
    src/git-daemon.cpp
    src/git-status-cache.cpp
    src/git-repository-watcher.cpp
    src/git-service.cpp
    src/git-version-worker.cpp
)

# 设置可执行文件源文件
set(DAEMON_SOURCES
    src/main.cpp
)

# DBus接口生成 - Qt6方式
# 1. 从头文件生成XML接口
qt_generate_dbus_interface(
    include/git-service.h
    org.deepin.FileManager.Git.xml
)

# 2. 生成DBus适配器代码
qt_add_dbus_adaptor(DAEMON_LIB_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.FileManager.Git.xml
    include/git-service.h
    GitService
    gitservice_adaptor
)

# 创建守护进程库
add_library(dde-git-daemon-lib STATIC
    ${DAEMON_HEADERS}
    ${DAEMON_LIB_SOURCES}
)

# 设置库包含目录
target_include_directories(dde-git-daemon-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}  # 用于生成的DBus文件
        ${COMMON_INCLUDE_DIR}
)

# 库链接依赖
target_link_libraries(dde-git-daemon-lib
    PUBLIC
        Qt6::Core
        Qt6::DBus
        dde-git-common
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${DAEMON_SOURCES}
)

# 链接库到可执行文件
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        dde-git-daemon-lib
)

# 设置编译属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 设置编译定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    GIT_DAEMON_PROCESS=1
)

# 安装二进制文件
install(TARGETS ${PROJECT_NAME}
    DESTINATION bin
    COMPONENT service
)

# 安装DBus服务配置
install(FILES systemd/org.deepin.FileManager.Git.service
    DESTINATION share/dbus-1/services
    COMPONENT service
)

# 安装Systemd用户服务
install(FILES systemd/dde-file-manager-git-daemon.service
    DESTINATION lib/systemd/user
    COMPONENT service
)

# 创建服务组件
set(CPACK_COMPONENT_SERVICE_DISPLAY_NAME "Git Service Daemon")
set(CPACK_COMPONENT_SERVICE_DESCRIPTION "Git status monitoring service for DDE File Manager")
set(CPACK_COMPONENT_SERVICE_REQUIRED ON) 
