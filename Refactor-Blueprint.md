# DDE File Manager Git Plugin 重构蓝图

## 项目概述

本文档描述了将现有的 dde-file-manager git vcs-plugin 从单体插件架构重构为进程分离架构的完整方案。新架构通过 DBus 服务实现进程间通信，将业务逻辑与文件管理器进程解耦，提高系统稳定性和性能。

## 架构设计原则

### 核心设计理念
1. **进程分离**：核心业务逻辑独立于文件管理器进程
2. **接口隔离**：插件接口层使用纯 C++ 实现，避免 Qt 版本强耦合
3. **代码复用**：最大化利用现有经过验证的代码
4. **性能优化**：通过本地缓存和批量操作优化高频调用
5. **系统集成**：遵循系统级服务最佳实践

### 技术架构
```
┌─────────────────────────────────────────────────┐
│ dde-file-manager Process                        │
│ ┌─────────────────┐ ┌─────────────────────────┐ │
│ │ Git Emblem      │ │ Git Menu/Window Plugin  │ │
│ │ Plugin (C++)    │ │ (C++ Interface Layer)   │ │
│ │ + Local Cache   │ │                         │ │
│ └─────────────────┘ └─────────────────────────┘ │
└─────────────────────────────────────────────────┘
           │                           │
           │ DBus Query/Signal         │ Process Launch
           ▼                           ▼
┌─────────────────────────────────────────────────┐
│ dde-file-manager-git-daemon                     │
│ ┌─────────────────────────────────────────────┐ │
│ │ Git Status Cache Service                    │ │
│ │ - Repository Discovery & Monitoring         │ │
│ │ - File Status Cache Management              │ │
│ │ - DBus Interface Implementation             │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
           │
           │ Process Launch
           ▼
┌─────────────────────────────────────────────────┐
│ dde-file-manager-git-dialog                     │
│ - git-commit, git-log, git-push, etc.          │
│ - Independent Qt Application Processes          │
└─────────────────────────────────────────────────┘
```

## 目录结构设计

### 新增重构目录结构
```
src/git2/                                    # 重构根目录
├── common/                                  # 公共组件 (迁移自现有代码)
│   ├── include/                            # 公共头文件
│   │   ├── git-types.h                     # Git 数据类型定义
│   │   ├── git-status-parser.h             # Git 状态解析器
│   │   └── git-utils.h                     # Git 工具函数
│   └── src/                                # 公共源文件
│       ├── git-status-parser.cpp           # 迁移自 gitstatusparser.cpp
│       └── git-utils.cpp                   # 迁移自 utils.cpp (部分)
├── daemon/                                 # Git 状态服务守护进程
│   ├── include/                            
│   │   ├── git-daemon.h                    # 主服务类
│   │   ├── git-status-cache.h              # 状态缓存管理
│   │   ├── git-repository-watcher.h        # 仓库监控服务
│   │   └── git-dbus-adaptor.h              # DBus 适配器
│   ├── src/
│   │   ├── main.cpp                        # 服务入口点
│   │   ├── git-daemon.cpp                  # 迁移自 gitoperationservice.cpp (部分)
│   │   ├── git-status-cache.cpp            # 迁移自 cache.cpp
│   │   ├── git-repository-watcher.cpp      # 迁移自 gitfilesystemwatcher.cpp
│   │   └── git-dbus-adaptor.cpp            # DBus 接口实现
│   ├── dbus/
│   │   └── org.deepin.FileManager.Git.xml  # DBus 接口定义
│   └── systemd/
│       ├── org.deepin.FileManager.Git.service          # DBus 服务配置
│       └── dde-file-manager-git-daemon.service        # Systemd 用户服务
├── dialog/                                 # Git 对话框进程
│   ├── include/
│   │   ├── git-dialog-launcher.h           # 对话框启动器
│   │   └── git-dialog-base.h               # 对话框基类
│   ├── src/
│   │   ├── main.cpp                        # 对话框进程入口
│   │   ├── git-dialog-launcher.cpp         # 命令行参数处理
│   │   └── git-dialog-base.cpp             # 基础对话框功能
│   └── dialogs/                            # 具体对话框实现 (迁移)
│       ├── git-commit-dialog.cpp           # 迁移自 gitcommitdialog.cpp
│       ├── git-log-dialog.cpp              # 迁移自 gitlogdialog.cpp
│       ├── git-push-dialog.cpp             # 迁移自 gitpushdialog.cpp
│       └── ...                             # 其他对话框
└── plugin/                                 # 文件管理器插件 (精简版)
    ├── include/
    │   ├── git-emblem-plugin.h             # 角标插件
    │   ├── git-menu-plugin.h               # 菜单插件
    │   ├── git-window-plugin.h             # 窗口插件
    │   └── git-local-cache.h               # 本地缓存
    ├── src/
    │   ├── dfm-extension-git.cpp           # 插件入口 (重构)
    │   ├── git-emblem-plugin.cpp           # 重构自 gitemblemiconplugin.cpp
    │   ├── git-menu-plugin.cpp             # 重构自 gitmenuplugin.cpp
    │   ├── git-window-plugin.cpp           # 重构自 gitwindowplugin.cpp
    │   └── git-local-cache.cpp             # 新增本地缓存实现
    └── dbus/
        └── git-dbus-client.cpp             # DBus 客户端封装
```

## 组件详细设计

### 1. Git Status Daemon (dde-file-manager-git-daemon)

#### 核心职责
- **仓库发现与注册**：自动发现和管理 Git 仓库
- **文件状态缓存**：维护全局文件状态缓存
- **文件系统监控**：实时监控仓库变化
- **DBus 服务提供**：为插件和对话框提供状态查询接口

#### DBus 接口设计
```xml
<!-- src/git2/daemon/dbus/org.deepin.FileManager.Git.xml -->
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.deepin.FileManager.Git">
    <!-- 仓库管理接口 -->
    <method name="RegisterRepository">
      <arg direction="in" type="s" name="repositoryPath"/>
      <arg direction="out" type="b" name="success"/>
    </method>
    
    <method name="UnregisterRepository">
      <arg direction="in" type="s" name="repositoryPath"/>
      <arg direction="out" type="b" name="success"/>
    </method>
    
    <!-- 状态查询接口 (批量优化) -->
    <method name="GetFileStatuses">
      <arg direction="in" type="as" name="filePaths"/>
      <arg direction="out" type="a{si}" name="statusMap"/>
    </method>
    
    <method name="GetRepositoryStatus">
      <arg direction="in" type="s" name="repositoryPath"/>
      <arg direction="out" type="a{si}" name="statusMap"/>
    </method>
    
    <!-- 操作触发接口 -->
    <method name="RefreshRepository">
      <arg direction="in" type="s" name="repositoryPath"/>
      <arg direction="out" type="b" name="success"/>
    </method>
    
    <method name="ClearRepositoryCache">
      <arg direction="in" type="s" name="repositoryPath"/>
      <arg direction="out" type="b" name="success"/>
    </method>
    
    <!-- 状态变更信号 -->
    <signal name="RepositoryStatusChanged">
      <arg type="s" name="repositoryPath"/>
      <arg type="a{si}" name="changedFiles"/>
    </signal>
    
    <signal name="RepositoryDiscovered">
      <arg type="s" name="repositoryPath"/>
    </signal>
  </interface>
</node>
```

#### 服务实现要点
```cpp
// daemon/include/git-status-cache.h
class GitStatusCache : public QObject
{
    Q_OBJECT
    
public:
    static GitStatusCache& instance();
    
    // 核心方法 - 迁移自 Global::Cache
    void resetVersion(const QString &repositoryPath, QHash<QString, ItemVersion> versionInfo);
    void removeVersion(const QString &repositoryPath);
    ItemVersion version(const QString &filePath);
    QStringList allRepositoryPaths();
    
    // 新增批量查询方法
    QHash<QString, ItemVersion> getFileStatuses(const QStringList &filePaths);
    QHash<QString, ItemVersion> getRepositoryStatus(const QString &repositoryPath);
    
signals:
    void repositoryStatusChanged(const QString &repositoryPath, const QHash<QString, ItemVersion> &changes);
    
private:
    QMutex m_mutex;
    QHash<QString, QHash<QString, ItemVersion>> m_repositories;
    
    // 性能优化: 批量操作支持
    void updateRepositoryBatch(const QString &repositoryPath, const QHash<QString, ItemVersion> &updates);
};
```

### 2. Git Dialog Process (dde-file-manager-git-dialog)

#### 命令行接口设计
```bash
# 提交对话框
dde-file-manager-git-dialog --action=commit --repository=/path/to/repo

# 日志查看器
dde-file-manager-git-dialog --action=log --repository=/path/to/repo [--file=/path/to/file]

# 推送对话框
dde-file-manager-git-dialog --action=push --repository=/path/to/repo

# 拉取对话框
dde-file-manager-git-dialog --action=pull --repository=/path/to/repo

# 状态对话框
dde-file-manager-git-dialog --action=status --repository=/path/to/repo

# 分支切换
dde-file-manager-git-dialog --action=checkout --repository=/path/to/repo

# Stash 管理
dde-file-manager-git-dialog --action=stash --repository=/path/to/repo

# 差异查看
dde-file-manager-git-dialog --action=diff --repository=/path/to/repo --file=/path/to/file

# 归属查看
dde-file-manager-git-dialog --action=blame --repository=/path/to/repo --file=/path/to/file

# 文件预览
dde-file-manager-git-dialog --action=preview --repository=/path/to/repo --file=/path/to/file [--commit=hash]

# 远程管理
dde-file-manager-git-dialog --action=remote --repository=/path/to/repo

# 分支比较
dde-file-manager-git-dialog --action=branch-compare --repository=/path/to/repo

# 清理对话框
dde-file-manager-git-dialog --action=clean --repository=/path/to/repo
```

#### 对话框基类设计
```cpp
// dialog/include/git-dialog-base.h
class GitDialogBase : public QDialog
{
    Q_OBJECT
    
public:
    explicit GitDialogBase(const QString &repositoryPath, QWidget *parent = nullptr);
    virtual ~GitDialogBase();
    
protected:
    // DBus 客户端访问
    QDBusInterface* gitService() const { return m_gitService; }
    
    // 工具方法 - 迁移自 Utils 类
    bool isInsideRepository(const QString &path) const;
    ItemVersion getFileStatus(const QString &path) const;
    QString repositoryPath() const { return m_repositoryPath; }
    
    // 状态更新通知
    virtual void onRepositoryStatusChanged(const QHash<QString, ItemVersion> &changes) {}
    
private slots:
    void handleRepositoryStatusChanged(const QString &repoPath, const QVariantMap &changes);
    
private:
    QString m_repositoryPath;
    QDBusInterface *m_gitService;
};
```

### 3. File Manager Plugin (精简接口层)

#### Git Emblem Plugin 重构
```cpp
// plugin/include/git-emblem-plugin.h
class GitEmblemPlugin : public DFMEXT::DFMExtEmblemIconPlugin
{
public:
    GitEmblemPlugin();
    DFMEXT::DFMExtEmblem locationEmblemIcons(const std::string &filePath, int systemIconCount) const override;

private:
    // 本地缓存管理
    mutable GitLocalCache m_localCache;
    mutable QDBusInterface *m_gitService;
    
    // 首次初始化逻辑 - 保留现有特殊处理
    static std::once_flag s_initOnceFlag;
    static void performFirstTimeInitialization(const QString &filePath);
    
    // 批量查询优化
    void requestBatchUpdate(const QString &filePath) const;
    bool isCacheValid(const QString &filePath) const;
};
```

#### 本地缓存设计
```cpp
// plugin/include/git-local-cache.h
class GitLocalCache : public QObject
{
    Q_OBJECT
    
public:
    explicit GitLocalCache(QObject *parent = nullptr);
    
    // 缓存查询
    ItemVersion getFileStatus(const QString &filePath);
    bool isCacheValid(const QString &filePath) const;
    
    // 批量更新
    void updateCache(const QHash<QString, ItemVersion> &statusMap);
    
    // 缓存管理
    void clearExpiredCache();
    void clearRepositoryCache(const QString &repositoryPath);
    
private slots:
    void onRepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changes);
    
private:
    struct CacheEntry {
        ItemVersion status;
        qint64 timestamp;
        QString repositoryPath;
    };
    
    mutable QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
    
    static const qint64 CACHE_TTL_MS = 100;  // 100ms TTL
    static const int MAX_CACHE_SIZE = 10000;
};
```

## 构建系统设计

### 主 CMakeLists.txt 配置
```cmake
# src/git2/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(dde-file-manager-git-plugin)

# Qt6 依赖
find_package(Qt6 REQUIRED COMPONENTS Core Widgets DBus)

# 设置全局包含目录 - 避免相对路径
set(COMMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
set(DAEMON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/daemon/include)
set(DIALOG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dialog/include)
set(PLUGIN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include)

# 添加子项目
add_subdirectory(common)
add_subdirectory(daemon)
add_subdirectory(dialog)
add_subdirectory(plugin)
```

### DBus 接口生成
```cmake
# daemon/CMakeLists.txt
# DBus 接口生成 - Qt6 方式
qt_generate_dbus_interface(
    include/git-dbus-adaptor.h
    org.deepin.FileManager.Git.xml
)

qt_add_dbus_adaptor(DAEMON_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/org.deepin.FileManager.Git.xml
    include/git-dbus-adaptor.h
    GitDBusAdaptor
)

add_executable(dde-file-manager-git-daemon ${DAEMON_SOURCES})

target_include_directories(dde-file-manager-git-daemon PRIVATE
    ${COMMON_INCLUDE_DIR}
    ${DAEMON_INCLUDE_DIR}
)

target_link_libraries(dde-file-manager-git-daemon
    Qt6::Core
    Qt6::DBus
    dde-git-common
)
```

### 系统服务安装

#### 包1: 文件管理器接口适配包安装
```cmake
# plugin/CMakeLists.txt
# 安装插件库文件
install(TARGETS dfm-extension-git 
    LIBRARY DESTINATION ${DFM_EXT_PLUGIN_DIR}
    COMPONENT extension)
```

#### 包2: Git 核心业务逻辑包安装  
```cmake
# daemon/CMakeLists.txt 和 dialog/CMakeLists.txt
# 安装核心服务二进制文件
install(TARGETS dde-file-manager-git-daemon 
    DESTINATION bin
    COMPONENT service)

install(TARGETS dde-file-manager-git-dialog 
    DESTINATION bin
    COMPONENT service)

# 安装 DBus 服务配置
install(FILES daemon/systemd/org.deepin.FileManager.Git.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/services
    COMPONENT service)

# 安装 Systemd 用户服务
install(FILES daemon/systemd/dde-file-manager-git-daemon.service
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user
    COMPONENT service)
```

## 代码迁移策略

### 迁移优先级和方法

#### 第一阶段：核心基础组件
1. **Git 状态解析器**
   - 源文件：`src/git/gitstatusparser.cpp/h`
   - 目标：`src/git2/common/src/git-status-parser.cpp`
   - 迁移方式：直接复制，调整头文件包含路径

2. **Cache 系统**
   - 源文件：`src/cache.cpp`, `include/cache.h`
   - 目标：`src/git2/daemon/src/git-status-cache.cpp`
   - 迁移方式：重构为独立服务，添加 DBus 接口

3. **文件系统监控**
   - 源文件：`src/git/gitfilesystemwatcher.cpp/h`
   - 目标：`src/git2/daemon/src/git-repository-watcher.cpp`
   - 迁移方式：保留核心逻辑，集成到服务中

#### 第二阶段：业务逻辑组件
1. **Git 操作工具**
   - 源文件：`src/git/utils.cpp/h`, `src/git/gitoperationutils.cpp/h`
   - 目标：`src/git2/common/src/git-utils.cpp`
   - 迁移方式：拆分为公共工具和服务专用功能

2. **对话框组件**
   - 源文件：`src/git/dialogs/*.cpp/h`
   - 目标：`src/git2/dialog/dialogs/`
   - 迁移方式：修改状态查询接口为 DBus 调用，其余逻辑保持不变

#### 第三阶段：插件接口层
1. **插件重构**
   - 源文件：`src/git/git*plugin.cpp/h`
   - 目标：`src/git2/plugin/src/`
   - 迁移方式：精简为接口层，添加本地缓存和 DBus 客户端

### 关键细节保留

#### GitEmblemIconPlugin 特殊逻辑保留
```cpp
// 保留首次初始化逻辑 - 这个设计有特殊作用
void GitEmblemPlugin::performFirstTimeInitialization(const QString &filePath)
{
    // 保留原始逻辑：获取文件所在目录并注册到服务
    QFileInfo fileInfo(filePath);
    const QString dirPath = fileInfo.absolutePath();
    
    qDebug() << "[GitEmblemPlugin] First-time initialization with directory:" << dirPath;
    
    // 通过 DBus 通知服务进行仓库发现
    QDBusInterface gitService("org.deepin.FileManager.Git",
                             "/org/deepin/FileManager/Git",
                             "org.deepin.FileManager.Git");
    gitService.call("RegisterRepository", dirPath);
}
```

#### Utils 函数的智能迁移
```cpp
// 现有的 Utils::isInsideRepositoryFile 逻辑保留
bool GitUtils::isInsideRepositoryFile(const QString &path)
{
    // 对于插件进程：查询本地缓存
    // 对于对话框进程：查询 DBus 服务
    // 对于服务进程：直接查询内部缓存
    
    #ifdef GIT_PLUGIN_PROCESS
        return GitLocalCache::instance().isInsideRepository(path);
    #elif defined(GIT_DIALOG_PROCESS)
        return GitDBusClient::instance().isInsideRepository(path);
    #else
        return GitStatusCache::instance().isInsideRepository(path);
    #endif
}
```

## 部署和打包

### Debian 包配置

#### 包1: 文件管理器接口适配包
```bash
# debian/control
Package: dde-file-manager-git-extension
Architecture: any
Depends: dde-file-manager (>= 6.0), dde-file-manager-git-service (>= ${binary:Version})
Description: Git extension interface for DDE File Manager
 Provides Git repository status emblems, context menus, and window
 integration for DDE File Manager. This package contains only the
 lightweight interface adapters that integrate with the file manager
 process, while the core Git functionality is provided by the
 dde-file-manager-git-service package.
 .
 This package includes:
  - Git emblem icon plugin for file status display
  - Git context menu plugin for right-click operations
  - Git window plugin for event handling
```

#### 包2: Git 核心业务逻辑包
```bash
# debian/control  
Package: dde-file-manager-git-service
Architecture: any
Depends: git, systemd, libqt6core6, libqt6widgets6, libqt6dbus6
Recommends: dde-file-manager-git-extension
Description: Git version control service for DDE File Manager
 Provides the core Git functionality including repository monitoring,
 status caching, and user interface dialogs for Git operations.
 This service runs independently from the file manager process to
 ensure system stability and performance.
 .
 This package includes:
  - Git status daemon (dde-file-manager-git-daemon)
  - Git dialog applications (dde-file-manager-git-dialog)
  - DBus service configuration
  - Systemd user service integration
 .
 The service provides Git repository discovery, file status monitoring,
 and comprehensive Git operation dialogs including commit, log, push,
 pull, branch management, and more.
```

### 服务配置文件

#### DBus 服务配置
```ini
# daemon/systemd/org.deepin.FileManager.Git.service
[D-BUS Service]
Name=org.deepin.FileManager.Git
Exec=/usr/bin/dde-file-manager-git-daemon
User=
SystemdService=dde-file-manager-git-daemon.service
```

#### Systemd 用户服务
```ini
# daemon/systemd/dde-file-manager-git-daemon.service
[Unit]
Description=DDE File Manager Git Status Service
After=graphical-session.target

[Service]
Type=dbus
BusName=org.deepin.FileManager.Git
ExecStart=/usr/bin/dde-file-manager-git-daemon
Restart=on-failure
RestartSec=3

[Install]
WantedBy=default.target
```

## 性能优化策略

### 高频调用优化
1. **批量查询**：一次 DBus 调用获取目录下所有文件状态
2. **本地缓存**：插件内 100ms TTL 缓存减少 IPC
3. **信号驱动更新**：状态变更时主动推送，避免轮询
4. **懒加载**：仅查询可见区域文件状态

### 内存管理
1. **服务进程**：独立内存空间，不影响文件管理器
2. **对话框进程**：按需启动，完成后退出
3. **缓存限制**：配置最大缓存大小和过期时间

## 开发里程碑

### 阶段一：基础设施搭建 (2周)
- [ ] 项目目录结构创建
- [ ] 构建系统配置
- [ ] DBus 接口定义和生成
- [ ] 基础服务框架

### 阶段二：核心服务开发 (3周)  
- [ ] Git 状态缓存服务实现
- [ ] 文件系统监控集成
- [ ] DBus 接口完整实现
- [ ] 单元测试编写

### 阶段三：插件重构 (2周)
- [ ] 角标插件重构和本地缓存
- [ ] 菜单插件精简为启动器
- [ ] 窗口插件重构
- [ ] 集成测试

### 阶段四：对话框迁移 (4周)
- [ ] 对话框基类和启动器
- [ ] 核心对话框迁移 (commit, log, push/pull)
- [ ] 其余对话框迁移
- [ ] 用户体验测试

### 阶段五：系统集成和优化 (2周)
- [ ] 系统服务配置
- [ ] 性能优化和调试
- [ ] 文档完善
- [ ] 发布准备

## 风险评估和缓解

### 技术风险
1. **DBus 性能问题**：通过本地缓存和批量操作缓解
2. **进程启动延迟**：对话框预加载和快速启动优化
3. **状态同步一致性**：信号机制确保实时更新

### 兼容性风险
1. **现有功能回归**：全面的集成测试覆盖
2. **用户体验变化**：保持现有交互模式不变
3. **系统依赖增加**：合理的降级处理机制

这个蓝图提供了完整的重构路径，既实现了进程分离的架构目标，又最大化保留了现有代码投入，是一个平衡技术收益和实施风险的优化方案。 