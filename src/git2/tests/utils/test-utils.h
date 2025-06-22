#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include "git-types.h"

/**
 * @brief 测试工具函数类
 */
class TestUtils
{
public:
    /**
     * @brief 创建临时Git仓库用于测试
     * @param repoPath 仓库路径
     * @return 是否创建成功
     */
    static bool createTestGitRepository(const QString &repoPath);
    
    /**
     * @brief 在仓库中创建测试文件
     * @param repoPath 仓库路径  
     * @param fileName 文件名
     * @param content 文件内容
     * @return 是否创建成功
     */
    static bool createTestFile(const QString &repoPath, const QString &fileName, const QString &content = "test content");
    
    /**
     * @brief 修改测试文件
     * @param filePath 文件路径
     * @param content 新内容
     * @return 是否修改成功
     */
    static bool modifyTestFile(const QString &filePath, const QString &content);
    
    /**
     * @brief 执行Git命令
     * @param repoPath 仓库路径
     * @param command Git命令
     * @param args 命令参数
     * @return 命令输出
     */
    static QString executeGitCommand(const QString &repoPath, const QString &command, const QStringList &args = {});
    
    /**
     * @brief 等待文件系统事件传播
     * @param ms 等待毫秒数
     */
    static void waitForFileSystemEvents(int ms = 100);
    
    /**
     * @brief 验证文件状态
     * @param filePath 文件路径
     * @param expectedStatus 期望状态
     * @return 是否匹配
     */
    static bool verifyFileStatus(const QString &filePath, int expectedStatus);
    
    /**
     * @brief 清理测试数据
     * @param path 要清理的路径
     */
    static void cleanupTestData(const QString &path);
};

/**
 * @brief 测试用的临时Git仓库管理器
 */
class MockGitRepository
{
public:
    explicit MockGitRepository();
    ~MockGitRepository();
    
    /**
     * @brief 获取仓库路径
     */
    QString repositoryPath() const { return m_tempDir.path(); }
    
    /**
     * @brief 初始化仓库
     */
    bool initialize();
    
    /**
     * @brief 添加文件到仓库
     */
    bool addFile(const QString &fileName, const QString &content = "test content");
    
    /**
     * @brief 修改文件
     */
    bool modifyFile(const QString &fileName, const QString &content);
    
    /**
     * @brief 删除文件
     */
    bool removeFile(const QString &fileName);
    
    /**
     * @brief 提交更改
     */
    bool commit(const QString &message = "test commit");
    
    /**
     * @brief 获取文件状态
     */
    QString getFileStatus(const QString &fileName);
    
private:
    QTemporaryDir m_tempDir;
    bool m_initialized;
};

#endif // TEST_UTILS_H 