# Git状态服务守护进程
project(dde-file-manager-git-daemon)

# 设置头文件
set(DAEMON_HEADERS
    include/git-daemon.h
    include/git-status-cache.h
    include/git-repository-watcher.h
    include/git-service.h
)

# 设置源文件
set(DAEMON_SOURCES
    src/main.cpp
    src/git-daemon.cpp
    src/git-status-cache.cpp
    src/git-repository-watcher.cpp
    src/git-service.cpp
)

# DBus接口生成 - Qt6方式
# 1. 从头文件生成XML接口
qt_generate_dbus_interface(
    include/git-service.h
    org.deepin.FileManager.Git.xml
)

# 2. 生成DBus适配器代码
qt_add_dbus_adaptor(DAEMON_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.FileManager.Git.xml
    include/git-service.h
    GitService
    gitservice_adaptor
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${DAEMON_HEADERS}
    ${DAEMON_SOURCES}
)

# 设置包含目录
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}  # 用于生成的DBus文件
        ${COMMON_INCLUDE_DIR}
)

# 链接库
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::DBus
        dde-git-common
)

# 设置编译属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 安装二进制文件
install(TARGETS ${PROJECT_NAME}
    DESTINATION bin
    COMPONENT service
)

# 安装DBus服务配置
install(FILES systemd/org.deepin.FileManager.Git.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/services
    COMPONENT service
)

# 安装Systemd用户服务
install(FILES systemd/dde-file-manager-git-daemon.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user
    COMPONENT service
) 