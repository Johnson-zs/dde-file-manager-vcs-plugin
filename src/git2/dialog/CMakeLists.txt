# Git对话框进程
project(dde-file-manager-git-dialog)

# 设置头文件
set(DIALOG_HEADERS
    include/git-dialog-launcher.h
    include/git-dialog-base.h
)

# 设置源文件
set(DIALOG_SOURCES
    src/main.cpp
    src/git-dialog-launcher.cpp
    src/git-dialog-base.cpp
)

# 对话框实现文件
set(DIALOG_IMPL_SOURCES
    dialogs/git-commit-dialog.cpp
    dialogs/git-log-dialog.cpp
    dialogs/git-push-dialog.cpp
    dialogs/git-pull-dialog.cpp
    dialogs/git-status-dialog.cpp
    dialogs/git-checkout-dialog.cpp
    dialogs/git-stash-dialog.cpp
    dialogs/git-diff-dialog.cpp
    dialogs/git-blame-dialog.cpp
    dialogs/git-preview-dialog.cpp
    dialogs/git-remote-dialog.cpp
    dialogs/git-branch-compare-dialog.cpp
    dialogs/git-clean-dialog.cpp
)

# DBus接口生成 - 依赖daemon生成的XML
# 注意：这里使用构建目录中生成的XML文件
qt_add_dbus_interface(DIALOG_SOURCES
    ${CMAKE_BINARY_DIR}/src/git2/daemon/org.deepin.FileManager.Git.xml
    gitdbus_interface
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${DIALOG_HEADERS}
    ${DIALOG_SOURCES}
    ${DIALOG_IMPL_SOURCES}
)

# 添加依赖关系，确保daemon的XML首先生成
add_dependencies(${PROJECT_NAME} dde-file-manager-git-daemon)

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
        Qt6::Widgets
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