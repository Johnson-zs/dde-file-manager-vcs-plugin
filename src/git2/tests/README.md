# DDE File Manager Git Plugin - 单元测试

## 概述

这个目录包含了DDE文件管理器Git插件的完整单元测试套件。测试框架基于Qt Test，覆盖了插件的所有核心组件和功能。

## 测试结构

```
tests/
├── CMakeLists.txt              # 测试框架配置
├── README.md                   # 本文档
├── utils/                      # 测试工具和Mock对象
│   ├── test-utils.h           # 测试工具函数头文件
│   ├── test-utils.cpp         # 测试工具函数实现
│   └── mock-git-repository.cpp # Git仓库模拟器
├── test-git-status-cache.cpp   # GitStatusCache单元测试
├── test-git-version-worker.cpp # GitVersionWorker单元测试
├── test-git-utils.cpp          # Git工具函数测试
├── test-git-status-parser.cpp  # Git状态解析器测试
└── test-git-dbus-service.cpp   # DBus服务集成测试
```

## 测试覆盖范围

### 1. GitStatusCache测试 (`test-git-status-cache.cpp`)

- **基础功能**：版本信息存储和检索
- **并发安全**：多线程访问的线程安全性
- **缓存管理**：LRU清理机制和内存管理
- **批量操作**：批量查询接口的正确性
- **性能测试**：大仓库处理和查询性能

### 2. GitVersionWorker测试 (`test-git-version-worker.cpp`)

- **核心功能**：retrieval函数的状态检索逻辑
- **状态计算**：calculateRepositoryRootStatus的计算规则
- **文件状态**：各种Git状态的正确识别
- **异常处理**：边界情况和错误恢复
- **性能测试**：大仓库检索性能

### 3. Git工具函数测试 (`test-git-utils.cpp`)

- **路径检测**：isInsideRepositoryFile的各种情况
- **状态查询**：getFileGitStatus的准确性
- **路径处理**：相对路径、绝对路径和特殊字符
- **错误处理**：权限问题和损坏仓库的处理
- **性能测试**：大目录树和重复调用的性能

### 4. Git状态解析器测试 (`test-git-status-parser.cpp`)

- **解析准确性**：各种Git状态的正确解析
- **格式支持**：porcelain、short、long格式
- **特殊情况**：Unicode文件名、空格、引号
- **错误处理**：无效输入和损坏输出的处理
- **性能测试**：大量文件的解析性能

### 5. DBus服务集成测试 (`test-git-dbus-service.cpp`)

- **服务启动**：DBus服务的启动和注册
- **接口调用**：所有DBus接口的功能测试
- **信号机制**：异步信号的正确性
- **错误处理**：服务重连和异常恢复
- **性能测试**：并发请求和大数据处理

## 运行测试

### 使用测试脚本（推荐）

```bash
# 运行基础测试
./scripts/run-tests.sh

# 运行测试并生成覆盖率报告
./scripts/run-tests.sh --coverage

# 清理构建并运行Release模式测试
./scripts/run-tests.sh --clean -t Release

# 查看所有选项
./scripts/run-tests.sh --help
```

### 手动运行

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. 配置CMake
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON ..

# 3. 编译
make -j$(nproc)

# 4. 运行所有测试
ctest --output-on-failure

# 5. 运行特定测试
./test-git-status-cache
./test-git-version-worker
./test-git-utils
./test-git-status-parser
./test-git-dbus-service
```

### 在IDE中运行

大多数支持CMake的IDE（如Qt Creator、CLion、VS Code）都可以直接运行和调试单元测试。

## 测试工具和Mock对象

### TestUtils类

提供了一系列测试工具函数：

- `createTestGitRepository()` - 创建临时Git仓库
- `createTestFile()` - 创建测试文件
- `executeGitCommand()` - 执行Git命令
- `waitForFileSystemEvents()` - 等待文件系统事件传播
- `cleanupTestData()` - 清理测试数据

### MockGitRepository类

模拟Git仓库的完整功能：

- `initialize()` - 初始化仓库
- `addFile()` - 添加文件
- `modifyFile()` - 修改文件
- `removeFile()` - 删除文件
- `commit()` - 提交更改
- `getFileStatus()` - 获取文件状态

## 代码覆盖率

启用覆盖率分析：

```bash
# 使用脚本（推荐）
./scripts/run-tests.sh --coverage

# 手动方式
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCMAKE_CXX_FLAGS="--coverage" ..
make -j$(nproc)
ctest
lcov --directory . --capture --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

覆盖率报告将生成在 `build/coverage_report/index.html`。

## 持续集成

项目配置了GitHub Actions自动运行测试：

- **触发条件**：push到main/develop分支或pull request
- **测试环境**：Ubuntu Latest + Qt6
- **覆盖率**：自动上传到Codecov

查看CI配置：`.github/workflows/tests.yml`

## 性能基准

测试包含了性能基准测试，确保各组件在以下场景下的性能：

- **大仓库**：10,000+文件的仓库处理
- **批量查询**：1,000+文件的批量状态查询
- **并发访问**：多线程并发访问缓存
- **重复调用**：频繁的状态查询操作

性能要求：

- GitStatusCache批量查询：< 10ms平均响应时间
- GitVersionWorker大仓库检索：< 30秒
- Git工具函数：< 1ms平均响应时间
- DBus服务接口：< 100ms平均响应时间

## 故障排除

### 常见问题

1. **Qt6未找到**
   ```bash
   sudo apt-get install qt6-base-dev qt6-tools-dev libqt6dbus6-dev
   ```

2. **DBus测试失败**
   - 确保DBus会话总线运行
   - 检查服务二进制文件是否正确编译

3. **覆盖率工具缺失**
   ```bash
   sudo apt-get install lcov gcov
   ```

4. **Git命令失败**
   - 确保系统安装了Git
   - 检查测试目录的写权限

### 调试测试

```bash
# 详细输出
ctest --verbose

# 运行特定测试并调试
gdb ./test-git-status-cache
```

## 贡献指南

### 添加新测试

1. 在适当的测试文件中添加测试方法
2. 使用`QVERIFY`、`QCOMPARE`等Qt Test宏
3. 确保测试具有描述性名称
4. 添加必要的测试文档

### 测试最佳实践

- **独立性**：每个测试应该独立运行
- **清理**：在`cleanup()`中清理测试数据
- **Mock使用**：使用MockGitRepository而不是真实仓库
- **性能意识**：避免长时间运行的测试
- **错误处理**：测试正常和异常情况

### 代码风格

遵循项目的代码风格指南：

- 使用Qt命名约定
- 添加适当的注释
- 保持测试代码简洁明了

## 相关文档

- [项目README](../../../README.md)
- [重构计划](../../../Refactor-Plan.md)
- [架构文档](../../../docs/architecture.md)
- [API文档](../../../docs/api.md) 