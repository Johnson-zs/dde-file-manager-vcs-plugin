# DDE文件管理器Git版本控制插件 - 多进程架构重构计划

## 重构目标

将现有单体插件架构重构为三层进程分离架构：

1. **插件接口层**: 轻量级C++适配器，本地缓存
2. **状态服务层**: 独立DBus守护进程，核心缓存和文件监控 
3. **对话框进程层**: 独立Qt应用程序，按需启动

---

## 🎯 1.0版本 - 核心架构重构 (MVP)

### TASK001 - 项目架构重构和构建系统搭建

**版本**: 1.0  
**状态**: 计划中  
**优先级**: P0 - 阻塞级

#### 任务描述

建立新的项目目录结构，配置构建系统，搭建多进程架构基础设施。

#### 子任务清单

1. **创建新目录结构**
   - 建立src/git2/目录作为重构根目录
   - 创建common/、daemon/、dialog/、plugin/四个子模块
   - 设置各模块的include/src目录结构

2. **配置CMake构建系统**
   - 编写主CMakeLists.txt和各子模块的构建配置
   - 配置Qt6依赖和DBus接口生成
   - 设置全局包含目录，避免相对路径问题

3. **DBus接口定义**
   - 设计org.deepin.FileManager.Git.xml接口文档
   - 配置DBus适配器和代理生成
   - 定义状态查询和变更通知接口

#### 验收标准

- [x] src/git2/目录结构完整建立
- [x] 各子模块CMakeLists.txt配置正确，可独立编译
- [x] DBus接口XML文件定义完整
- [x] Qt6 DBus适配器和代理代码正确生成
- [x] 整体项目可以完成编译构建

#### 关键文件创建

```
src/git2/
├── CMakeLists.txt
├── common/CMakeLists.txt  
├── daemon/CMakeLists.txt
├── dialog/CMakeLists.txt
├── plugin/CMakeLists.txt
└── daemon/dbus/org.deepin.FileManager.Git.xml
```

#### AI编程助手提示词

```
你是资深的CMake构建系统专家，精通Qt6应用的项目配置和DBus接口设计。当前任务是为DDE文件管理器Git插件搭建多进程架构的构建基础设施。

**项目架构要求**:
- 四个独立模块：common(公共组件)、daemon(DBus服务)、dialog(对话框进程)、plugin(插件接口)
- 使用Qt6 Core、Widgets、DBus组件
- 避免相对路径，设置全局包含目录
- DBus接口需要同时生成适配器和代理代码

**DBus接口设计**:
需要支持以下核心功能：
- 批量文件状态查询：GetFileStatuses(filePaths) -> statusMap
- 仓库注册和状态通知：RepositoryStatusChanged信号
- 仓库发现和管理：RegisterRepository/UnregisterRepository

**技术要求**:
- 使用现代CMake 3.16+语法
- 正确配置Qt6的DBus组件
- 设置合理的安装目标和路径
- 支持Debug和Release构建配置

请提供完整的构建系统配置，包括主CMakeLists.txt和各子模块的构建文件。
```

---

### TASK002 - Git状态缓存服务核心重构

**版本**: 1.0  
**状态**: 计划中  
**优先级**: P0 - 阻塞级

#### 任务描述

将现有的Global::Cache重构为独立的DBus服务进程，实现高性能的Git状态缓存和查询服务。

#### 子任务清单

1. **GitStatusCache核心迁移**
   - 将src/cache.cpp中的Global::Cache逻辑迁移到daemon/src/git-status-cache.cpp
   - 保留原有的线程安全机制（QMutex）和LRU缓存逻辑
   - 实现批量状态查询接口，优化DBus调用性能

2. **GitRepositoryWatcher重构**
   - 迁移src/git/gitfilesystemwatcher.cpp到daemon/src/git-repository-watcher.cpp
   - 保留现有的仓库发现和文件变更监控逻辑
   - 集成到服务中，实现自动缓存刷新

3. **DBus服务接口实现**
   - 实现GitDBusAdaptor类，提供外部调用接口
   - 实现批量查询方法GetFileStatuses和GetRepositoryStatus
   - 实现状态变更信号RepositoryStatusChanged

4. **服务进程主程序**
   - 编写daemon/src/main.cpp服务入口
   - 实现DBus服务注册和生命周期管理
   - 添加日志记录和错误处理机制

#### 验收标准

- [x] GitStatusCache迁移完成，保留原有核心逻辑
- [x] QMutex线程安全机制和缓存算法正确迁移
- [x] GitRepositoryWatcher迁移完成，文件监控功能正常
- [x] DBus服务可以正常启动并注册到系统
- [x] GetFileStatuses批量查询接口返回正确状态
- [x] RepositoryStatusChanged信号在文件变更时正确发送
- [x] 服务进程具备完整的错误处理和恢复机制

#### 关键技术实现

```cpp
// GitStatusCache批量查询接口
QHash<QString, ItemVersion> GitStatusCache::getFileStatuses(const QStringList &filePaths)
{
    QMutexLocker locker(&m_mutex);
    QHash<QString, ItemVersion> result;
    
    for (const QString &filePath : filePaths) {
        QString repoPath = findRepositoryPath(filePath);
        if (!repoPath.isEmpty() && m_repositories.contains(repoPath)) {
            auto repoCache = m_repositories[repoPath];
            if (repoCache.contains(filePath)) {
                result[filePath] = repoCache[filePath];
            }
        }
    }
    return result;
}
```

#### AI编程助手提示词

```
你是Git版本控制系统专家，精通高性能缓存设计和DBus服务开发。当前任务是将单体应用的缓存系统重构为独立的DBus服务。

**迁移约束**:
- 必须保留现有Cache类的核心逻辑和线程安全机制
- 保持原有的QMutex锁机制和LRU缓存算法
- 文件监控功能必须完整迁移且性能不能下降
- 确保向后兼容，不破坏现有的Git状态语义

**性能要求**:
- 批量查询接口支持一次查询100+文件状态
- DBus调用延迟控制在10ms以内
- 文件变更监控延迟不超过100ms
- 内存使用控制在合理范围（<100MB）

**关键代码迁移**:
1. Global::Cache的resetVersion/removeVersion/version方法
2. GitFileSystemWatcher的仓库发现和监控逻辑
3. ItemVersion枚举和Git状态解析逻辑

**服务设计要求**:
- 使用Qt6的QDBusAbstractAdaptor实现服务接口
- 实现优雅的服务启动和关闭流程
- 添加完整的英文日志记录
- 提供服务健康检查和自动恢复机制

请提供完整的服务实现代码，重点关注代码迁移的准确性和服务的稳定性。
```

---

### TASK003 - Git角标插件完整重构

**版本**: 1.0  
**状态**: 计划中  
**优先级**: P0 - 阻塞级

#### 任务描述

将现有的GitEmblemIconPlugin重构为轻量级的接口适配器，实现本地缓存和DBus客户端集成。

#### 子任务清单

1. **GitEmblemPlugin核心重构**
   - 重构src/git/gitemblemiconplugin.cpp为plugin/src/git-emblem-plugin.cpp
   - 保留现有的performFirstTimeInitialization特殊逻辑
   - 将直接的Cache调用替换为DBus客户端调用

2. **本地缓存层实现**
   - 实现GitLocalCache类，提供100ms TTL的本地缓存
   - 减少高频locationEmblemIcons调用的DBus开销
   - 实现缓存失效和批量更新机制

3. **DBus客户端封装**
   - 实现GitDBusClient类，封装对daemon服务的调用
   - 提供同步和异步的状态查询接口
   - 处理DBus连接异常和服务不可用情况

4. **性能优化策略**
   - 实现批量查询机制，一次获取目录下所有文件状态
   - 实现信号驱动的缓存更新，监听RepositoryStatusChanged
   - 添加智能的预加载策略

#### 验收标准

- [x] GitEmblemPlugin重构完成，接口保持不变
- [x] performFirstTimeInitialization逻辑正确保留
- [x] 本地缓存有效减少DBus调用频次（减少80%以上）
- [x] DBus客户端正确处理服务不可用等异常情况
- [x] 角标显示功能与原版本完全一致
- [x] 批量查询机制正确实现，性能提升明显
- [x] 信号驱动的缓存更新机制工作正常

#### 关键技术实现

```cpp
// GitLocalCache实现要点
class GitLocalCache : public QObject
{
public:
    ItemVersion getFileStatus(const QString &filePath) {
        QMutexLocker locker(&m_mutex);
        
        // 检查缓存有效性
        if (isCacheValid(filePath)) {
            return m_cache[filePath].status;
        }
        
        // 缓存未命中，请求批量更新
        requestBatchUpdate(QFileInfo(filePath).absolutePath());
        
        return m_cache.value(filePath, {UnversionedVersion, 0}).status;
    }
    
private:
    static const qint64 CACHE_TTL_MS = 100;
    QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
};
```

#### AI编程助手提示词

```
你是dfm-extension框架专家，精通DFMExtEmblemIconPlugin的性能优化和缓存设计。当前任务是重构Git角标插件为高性能的DBus客户端。

**重构约束**:
- 必须保持DFMExtEmblemIconPlugin接口完全不变
- 保留performFirstTimeInitialization的特殊初始化逻辑
- locationEmblemIcons方法的调用频率极高，必须高度优化
- 确保Git状态显示的准确性和实时性

**性能关键点**:
- locationEmblemIcons在每次paintEvent时都会调用
- 需要通过本地缓存将DBus调用减少80%以上
- 批量查询策略：一次获取整个目录的状态
- TTL缓存：100ms内的重复查询直接返回缓存结果

**DBus集成要求**:
- 实现异步状态查询，避免UI线程阻塞
- 处理DBus服务启动延迟和连接失败
- 监听RepositoryStatusChanged信号进行缓存更新
- 提供服务不可用时的降级策略

**关键代码保留**:
- performFirstTimeInitialization中的目录注册逻辑
- 原有的Git状态到角标图标的映射关系
- 线程安全的缓存访问机制

请提供完整的插件重构实现，重点关注性能优化和稳定性。
```

---

### TASK004 - 公共组件模块重构

**版本**: 1.0  
**状态**: 计划中  
**优先级**: P1 - 重要

#### 任务描述

将现有的Utils、GitStatusParser等公共组件重构为独立的共享模块，供各进程复用。

#### 子任务清单

1. **Git工具函数迁移**
   - 将src/git/utils.cpp中的核心工具函数迁移到common/src/git-utils.cpp
   - 保留isInsideRepositoryFile、getFileGitStatus等关键方法
   - 重构为进程无关的纯函数实现

2. **Git状态解析器迁移**
   - 迁移src/git/gitstatusparser.cpp到common/src/git-status-parser.cpp
   - 保留现有的Git命令解析逻辑
   - 优化解析性能和错误处理

3. **Git数据类型定义**
   - 创建common/include/git-types.h统一数据类型定义
   - 迁移ItemVersion枚举和相关常量定义
   - 确保各进程使用统一的数据类型

4. **进程适配接口**
   - 实现进程相关的适配层，处理不同进程的调用差异
   - 插件进程使用本地缓存，服务进程直接访问缓存
   - 对话框进程通过DBus客户端调用

#### 验收标准

- [x] Utils工具函数完整迁移，功能保持一致
- [x] GitStatusParser迁移完成，解析准确性不变
- [x] 统一的数据类型定义被各进程正确使用
- [x] 进程适配接口正确处理不同进程的调用需求
- [x] 公共组件库可以被各进程正确链接和使用
- [x] 原有功能的兼容性测试通过

#### AI编程助手提示词

```
你是C++模块化设计专家，精通跨进程的代码重构和公共组件设计。当前任务是将Git插件的公共代码重构为独立的共享模块。

**模块化要求**:
- 创建进程无关的纯函数实现
- 保持现有API的兼容性，避免破坏性变更
- 设计合理的进程适配层，屏蔽进程差异
- 确保线程安全和异常安全

**关键代码迁移**:
1. Utils::isInsideRepositoryFile - Git仓库检测逻辑
2. Utils::getFileGitStatus - 文件状态查询逻辑  
3. GitStatusParser::parseGitStatus - Git命令输出解析
4. ItemVersion枚举 - Git文件状态定义

**进程适配设计**:
```cpp
// 进程适配接口示例
namespace GitUtils {
    // 插件进程：查询本地缓存
    // 服务进程：直接访问内部缓存  
    // 对话框进程：通过DBus调用
    ItemVersion getFileStatus(const QString &filePath);
    
    bool isInsideRepository(const QString &path);
}
```

**技术要求**:

- 使用现代C++17特性
- 提供完整的错误处理
- 添加详细的英文注释
- 确保跨平台兼容性

请提供完整的公共组件重构方案，重点关注模块化设计和API兼容性。

### TASK005 - 单元测试框架搭建和核心模块测试
**版本**: 1.0  
**状态**: 计划中  
**优先级**: P1 - 重要

#### 任务描述
从零搭建单元测试框架，为核心角标业务模块编写完整的单元测试。

#### 子任务清单
1. **测试框架搭建**
   - 集成Qt Test框架，配置CMake测试目标
   - 建立tests/目录结构，设置测试运行环境
   - 配置GitHub Actions自动化测试流水线
   - 设置测试覆盖率工具(gcov/lcov)

2. **GitStatusCache测试套件**
   - 测试版本信息的存储和检索功能
   - 测试多线程并发访问的安全性
   - 测试LRU缓存淘汰机制
   - 测试批量查询接口的正确性

3. **GitRepositoryWatcher测试套件**
   - 测试仓库发现和注册功能
   - 测试文件变更监控的实时性
   - 测试监控器的启动和停止流程
   - 测试异常情况的处理机制

4. **Git工具函数测试套件**
   - 测试isInsideRepositoryFile的边界情况
   - 测试getFileGitStatus的各种状态返回
   - 测试GitStatusParser的解析准确性
   - 测试Utils函数的错误处理

5. **DBus接口集成测试**
   - 测试服务启动和DBus注册
   - 测试客户端与服务的通信
   - 测试信号机制的正确性
   - 测试异常情况的恢复机制

#### 验收标准
- [x] Qt Test框架正确集成，测试可以正常运行
- [x] CMake测试目标配置完整，支持ctest运行
- [x] GitStatusCache测试覆盖率达到90%以上
- [x] GitRepositoryWatcher测试覆盖率达到85%以上
- [x] Git工具函数测试覆盖率达到95%以上
- [x] DBus接口集成测试能发现通信问题
- [x] 所有测试在CI环境中可以稳定通过
- [x] 测试覆盖率报告可以正确生成

#### 测试用例示例
```cpp
// GitStatusCache测试示例
class TestGitStatusCache : public QObject
{
    Q_OBJECT
private slots:
    void testVersionStorage();
    void testConcurrentAccess();
    void testBatchQuery();
    void testCacheEviction();
    void testRepositoryRemoval();
};

void TestGitStatusCache::testVersionStorage()
{
    GitStatusCache cache;
    QString repoPath = "/tmp/test-repo";
    QHash<QString, ItemVersion> versions;
    versions["/tmp/test-repo/file1.txt"] = LocallyModifiedVersion;
    
    cache.resetVersion(repoPath, versions);
    
    QCOMPARE(cache.version("/tmp/test-repo/file1.txt"), LocallyModifiedVersion);
    QCOMPARE(cache.version("/tmp/test-repo/nonexistent.txt"), UnversionedVersion);
}
```

#### AI编程助手提示词

```
你是软件质量保证专家，精通Qt Test框架和C++单元测试最佳实践。当前任务是为Git插件重构项目建立完整的测试体系。

**测试框架要求**:
- 使用Qt Test框架作为主要测试工具
- 集成CMake的CTest功能
- 支持测试覆盖率统计和报告生成
- 配置CI/CD自动化测试流水线

**重点测试模块**:
1. GitStatusCache - 缓存核心逻辑和线程安全
2. GitRepositoryWatcher - 文件系统监控和仓库发现
3. Git工具函数 - 状态查询和仓库检测
4. DBus接口 - 进程间通信和错误处理

**测试策略设计**:
- 单元测试：测试单个类和函数的正确性
- 集成测试：测试DBus服务和客户端的协作
- 性能测试：验证缓存和监控的性能指标
- 边界测试：测试异常输入和错误情况

**测试环境要求**:
- 模拟Git仓库环境进行测试
- 多线程并发测试
- DBus服务的Mock和Stub
- 内存泄漏检测

**质量目标**:
- 核心模块测试覆盖率>90%
- 所有测试在CI环境稳定通过
- 测试执行时间<2分钟
- 发现和预防回归问题

请提供完整的测试框架搭建方案和核心模块的测试用例实现。
```

---

### TASK006 - 系统服务集成和部署配置

**版本**: 1.0  
**状态**: 计划中  
**优先级**: P1 - 重要

#### 任务描述

配置DBus服务和Systemd用户服务，实现Git daemon的系统级集成和自动启动。

#### 子任务清单

1. **DBus服务配置**
   - 编写org.deepin.FileManager.Git.service配置文件
   - 配置服务的启动参数和用户权限
   - 测试DBus服务的注册和激活

2. **Systemd用户服务配置**
   - 编写dde-file-manager-git-daemon.service配置
   - 配置服务的依赖关系和重启策略
   - 实现服务的优雅启动和关闭

3. **Debian包配置**
   - 设计两个包的分离策略：extension和service
   - 编写postinst/prerm脚本处理服务安装
   - 配置包的依赖关系和冲突处理

4. **安装脚本和部署**
   - 编写CMake安装目标
   - 配置文件和二进制文件的正确部署
   - 测试完整的安装和卸载流程

#### 验收标准

- [x] DBus服务可以正确注册到系统
- [x] Systemd用户服务可以自动启动和重启
- [x] 服务进程在异常退出后能自动恢复
- [x] Debian包安装后服务配置正确
- [x] 服务卸载时能正确清理资源
- [x] 多用户环境下服务隔离正常
- [x] 服务启动时间控制在3秒以内

#### AI编程助手提示词

```
你是Linux系统集成专家，精通DBus服务配置和Systemd服务管理。当前任务是为Git插件的daemon服务配置完整的系统集成。

**服务集成要求**:
- DBus服务：按需激活，用户级别，自动注册
- Systemd服务：用户服务，依赖图形会话，自动重启
- 权限控制：普通用户权限，访问用户Git仓库
- 生命周期：随用户会话启动，会话结束时清理

**部署策略**:
- 分包部署：插件包(轻量级) + 服务包(完整功能)
- 依赖管理：服务包包含所有核心依赖
- 升级兼容：服务可以独立升级，不影响文件管理器
- 故障恢复：服务异常时自动重启，避免影响用户体验

**技术实现要点**:
```ini
# DBus service configuration
[D-BUS Service]
Name=org.deepin.FileManager.Git
Exec=/usr/bin/dde-file-manager-git-daemon
User=
SystemdService=dde-file-manager-git-daemon.service

# Systemd user service
[Unit]
Description=DDE File Manager Git Status Service
After=graphical-session.target

[Service]
Type=dbus
BusName=org.deepin.FileManager.Git
Restart=on-failure
```

**质量要求**:

- 服务启动时间<3秒
- 内存占用<50MB
- CPU占用<5%（空闲时）
- 支持优雅关闭和资源清理

请提供完整的系统集成配置和部署脚本。



## 🚀 2.0版本 - 对话框进程迁移

### TASK007 - Git对话框进程架构搭建
**版本**: 2.0  
**状态**: 计划中  
**优先级**: P1 - 重要

#### 任务描述
建立独立的Git对话框进程架构，实现命令行参数解析和对话框基类。

#### 子任务清单
1. **对话框进程框架**
   - 实现main.cpp入口和命令行参数解析
   - 设计对话框类型路由和实例化机制
   - 实现进程间通信和状态同步

2. **对话框基类设计**
   - 创建GitDialogBase基类，封装公共功能
   - 集成DBus客户端访问daemon服务
   - 实现统一的错误处理和日志记录

3. **命令行接口设计**
   - 设计完整的命令行参数体系
   - 支持所有Git操作的参数传递
   - 实现参数验证和错误提示

#### 验收标准
- [x] 对话框进程可以正确解析命令行参数
- [x] 基类提供完整的daemon服务访问接口
- [x] 进程启动时间控制在1秒以内
- [x] 错误处理机制完整，用户友好

#### AI编程助手提示词

你是Qt应用架构专家，精通进程间通信和命令行应用设计。当前任务是设计Git对话框的独立进程架构。

**架构设计要求**:

- 单一进程支持多种对话框类型
- 命令行参数驱动的对话框选择
- DBus客户端集成到基类
- 统一的错误处理和用户反馈

**命令行接口设计**:

```bash
dde-file-manager-git-dialog --action=commit --repository=/path/to/repo
dde-file-manager-git-dialog --action=log --repository=/path/to/repo --file=/path/to/file
```

**技术实现要点**:

- QCommandLineParser处理参数解析
- 工厂模式创建不同类型对话框
- DBus客户端的异步调用和错误处理
- Qt Application的优雅启动和退出

请提供完整的对话框进程架构实现。



### TASK008 - 核心Git对话框迁移
**版本**: 2.0  
**状态**: 计划中  
**优先级**: P1 - 重要

#### 任务描述
将现有的Git对话框迁移到独立进程，保持原有功能和用户体验。

#### 子任务清单
1. **GitCommitDialog迁移**
   - 迁移提交对话框到独立进程
   - 保持原有的UI布局和交互逻辑
   - 通过DBus获取文件状态信息

2. **GitLogDialog迁移**
   - 迁移日志查看器到独立进程
   - 保持完整的commit历史显示功能
   - 优化大型仓库的性能表现

3. **GitPushDialog和GitPullDialog迁移**
   - 迁移推送和拉取对话框
   - 保持网络操作的进度显示
   - 实现操作结果的状态同步

#### 验收标准
- [x] 所有迁移的对话框功能完全保持
- [x] UI响应性和用户体验不下降
- [x] 操作完成后正确通知daemon更新状态
- [x] 内存使用合理，对话框关闭后资源释放

---

### TASK009 - Git菜单和窗口插件重构
**版本**: 2.0  
**状态**: 计划中  
**优先级**: P2 - 一般

#### 任务描述
重构Git菜单插件和窗口插件为轻量级的进程启动器。

#### 子任务清单
1. **GitMenuPlugin重构**
   - 重构为对话框进程的启动器
   - 保持原有的菜单项和交互逻辑
   - 优化菜单响应速度

2. **GitWindowPlugin重构**
   - 重构窗口事件处理逻辑
   - 实现与daemon服务的状态同步
   - 保持原有的窗口集成功能

#### 验收标准
- [x] 菜单项点击正确启动对应对话框
- [x] 窗口事件处理功能保持不变
- [x] 插件资源占用最小化

## 🌟 3.0版本 - 系统优化和完善

### TASK010 - 性能优化和稳定性增强
**版本**: 3.0  
**状态**: 计划中  
**优先级**: P2 - 一般

#### 任务描述
针对大型仓库和长时间运行进行性能优化，增强系统稳定性。

#### 子任务清单
1. **大型仓库优化**
   - 优化Git状态查询的性能
   - 实现增量更新和智能缓存
   - 支持10万+文件的仓库

2. **内存和资源管理**
   - 优化长时间运行的内存使用
   - 实现资源泄漏检测和修复
   - 添加系统资源监控

3. **错误恢复机制**
   - 实现服务异常的自动恢复
   - 添加完整的错误诊断信息
   - 提供用户友好的错误解决建议

#### 验收标准
- [x] 大型仓库(10万+文件)响应时间<5秒
- [x] 长时间运行(24小时+)无内存泄漏
- [x] 服务异常后能在30秒内自动恢复
- [x] 错误情况下提供明确的解决指导

---

### TASK011 - 文档和部署完善
**版本**: 3.0  
**状态**: 计划中  
**优先级**: P3 - 可选

#### 任务描述
完善项目文档，优化部署流程，提供完整的用户和开发者指南。

#### 子任务清单
1. **用户文档**
   - 编写用户使用指南
   - 提供常见问题解答
   - 录制功能演示视频

2. **开发者文档**
   - 编写架构设计文档
   - 提供API接口文档
   - 创建开发环境搭建指南

3. **部署和运维**
   - 优化安装和配置流程
   - 提供故障排查指南
   - 创建监控和日志分析工具

#### 验收标准
- [x] 文档覆盖所有主要功能和配置
- [x] 新用户可以通过文档快速上手
- [x] 开发者可以基于文档进行二次开发
- [x] 运维人员可以独立部署和维护

---

## 📋 实施计划总结

### 版本发布策略
- **1.0版本 (6-8周)**: 完整的角标功能重构，包含完备的插件、服务代码和单元测试框架
- **2.0版本 (4-5周)**: Git对话框迁移到独立进程，菜单和窗口插件重构
- **3.0版本 (3-4周)**: 性能优化、稳定性增强和文档完善

### 关键里程碑
1. **架构基础完成**: DBus服务可用，角标功能正常
2. **核心功能迁移**: 所有关键模块重构完成
3. **系统集成完成**: 服务部署和自动化测试就绪
4. **对话框迁移完成**: 所有Git操作在独立进程中运行
5. **性能优化完成**: 大型仓库支持，长时间稳定运行

### 风险控制
- **技术风险**: 通过完整的单元测试和集成测试覆盖
- **性能风险**: 分阶段性能测试和优化
- **兼容性风险**: 保持现有API和用户体验不变
- **部署风险**: 提供回滚机制和故障恢复

这个重构计划专注于多进程架构的实现，确保1.0版本就能看到完整的角标功能重构，包含完备的插件和服务代码，并建立了完整的单元测试框架。每个任务都有明确的验收标准和技术实现要点。

IMPLEMENTATION CHECKLIST:
1. 创建src/git2/目录结构和CMake构建系统配置
2. 设计和实现DBus接口XML定义文件
3. 迁移Global::Cache到GitStatusCache独立服务
4. 迁移GitFileSystemWatcher到GitRepositoryWatcher
5. 实现GitDBusAdaptor和daemon服务主程序
6. 重构GitEmblemIconPlugin为轻量级DBus客户端
7. 实现GitLocalCache本地缓存层
8. 迁移Utils和GitStatusParser到公共组件模块
9. 搭建Qt Test单元测试框架
10. 编写GitStatusCache、GitRepositoryWatcher等核心模块测试
11. 配置DBus服务和Systemd用户服务
12. 设置Debian包分离和部署脚本
13. 实现对话框进程架构和命令行接口
14. 迁移GitCommitDialog、GitLogDialog等核心对话框
15. 重构GitMenuPlugin和GitWindowPlugin为进程启动器
16. 进行集成测试和性能优化
