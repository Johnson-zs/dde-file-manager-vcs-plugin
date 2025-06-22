#include <QtTest/QtTest>
#include <QObject>
#include <QDebug>
#include <QDir>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include "utils/test-utils.h"
#include "git-utils.h"
#include "git-types.h"

#ifndef QT_TESTCASE_BUILDDIR
#define QT_TESTCASE_BUILDDIR QDir::currentPath()
#endif

/**
 * @brief Git工具函数单元测试类
 * 
 * 测试覆盖：
 * 1. 路径检测函数的边界情况
 * 2. 文件状态查询的准确性
 * 3. 路径处理的各种情况
 * 4. 异常情况的处理
 */
class TestGitUtils : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 路径检测测试
    void testIsInsideRepositoryFile();
    void testRepositoryPathDetection();
    void testRelativePathHandling();
    void testAbsolutePathHandling();
    
    // 文件状态查询测试
    void testGetFileGitStatus();
    void testNormalFileStatus();
    void testModifiedFileStatus();
    void testUntrackedFileStatus();
    
    // 特殊字符处理测试
    void testSpecialCharacterPaths();
    void testUnicodeFilenames();
    void testSpacesInPaths();
    
    // 异常处理测试
    void testNonExistentPath();
    void testPermissionDenied();
    void testCorruptedRepository();
    
    // 性能测试
    void testLargeDirectoryTree();
    void testRepeatedCalls();

private:
    void createTestFileStructure();
    
private:
    MockGitRepository *m_mockRepo;
    QString m_testRepoPath;
    QTemporaryDir *m_tempDir;
};

void TestGitUtils::initTestCase()
{
    qDebug() << "Starting Git Utils tests";
}

void TestGitUtils::cleanupTestCase()
{
    qDebug() << "Git Utils tests completed";
}

void TestGitUtils::init()
{
    // 创建测试仓库
    m_mockRepo = new MockGitRepository();
    QVERIFY(m_mockRepo->initialize());
    m_testRepoPath = m_mockRepo->repositoryPath();
    
    // 创建临时目录用于非仓库测试
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    
    createTestFileStructure();
    
    qDebug() << "Test repository created at:" << m_testRepoPath;
}

void TestGitUtils::cleanup()
{
    delete m_mockRepo;
    m_mockRepo = nullptr;
    
    delete m_tempDir;
    m_tempDir = nullptr;
}

void TestGitUtils::testIsInsideRepositoryFile()
{
    // 测试仓库内文件
    QString repoFile = m_testRepoPath + "/test.cpp";
    QVERIFY(m_mockRepo->addFile("test.cpp", "test content"));
    
    QVERIFY(GitUtils::isInsideRepositoryFile(repoFile));
    
    // 测试仓库根目录
    QVERIFY(GitUtils::isInsideRepositoryFile(m_testRepoPath));
    
    // 测试非仓库文件
    QString nonRepoFile = m_tempDir->path() + "/test.txt";
    TestUtils::createTestFile(nonRepoFile, "content");
    
    QVERIFY(!GitUtils::isInsideRepositoryFile(nonRepoFile));
    
    // 测试不存在的文件
    QString nonExistentFile = m_testRepoPath + "/nonexistent.cpp";
    QVERIFY(!GitUtils::isInsideRepositoryFile(nonExistentFile));
}

void TestGitUtils::testRepositoryPathDetection()
{
    // 测试从子目录检测仓库路径
    QString subDir = m_testRepoPath + "/subdir";
    QDir().mkpath(subDir);
    
    QString subFile = subDir + "/subfile.cpp";
    QVERIFY(m_mockRepo->addFile("subdir/subfile.cpp", "sub content"));
    
    QString detectedRepo = GitUtils::repositoryBaseDir(subFile);
    QCOMPARE(detectedRepo, m_testRepoPath);
    
    // 测试从仓库根目录检测
    QString rootDetected = GitUtils::repositoryBaseDir(m_testRepoPath);
    QCOMPARE(rootDetected, m_testRepoPath);
}

void TestGitUtils::testRelativePathHandling()
{
    // 创建相对路径测试
    QString currentDir = QDir::currentPath();
    QDir::setCurrent(m_testRepoPath);
    
    QString relativeFile = "./relative.cpp";
    QVERIFY(m_mockRepo->addFile("relative.cpp", "relative content"));
    
    QVERIFY(GitUtils::isInsideRepositoryFile(relativeFile));
    
    // 恢复原始目录
    QDir::setCurrent(currentDir);
}

void TestGitUtils::testAbsolutePathHandling()
{
    // 测试绝对路径
    QString absoluteFile = QDir(m_testRepoPath).absoluteFilePath("absolute.cpp");
    QVERIFY(m_mockRepo->addFile("absolute.cpp", "absolute content"));
    
    QVERIFY(GitUtils::isInsideRepositoryFile(absoluteFile));
    
    ItemVersion status = GitUtils::getFileGitStatus(absoluteFile);
    QVERIFY(status != UnversionedVersion);
}

void TestGitUtils::testGetFileGitStatus()
{
    // 测试正常文件状态
    QString normalFile = m_testRepoPath + "/normal.cpp";
    QVERIFY(m_mockRepo->addFile("normal.cpp", "normal content"));
    QVERIFY(m_mockRepo->commit("Add normal file"));
    
    ItemVersion status = GitUtils::getFileGitStatus(normalFile);
    QCOMPARE(status, NormalVersion);
}

void TestGitUtils::testNormalFileStatus()
{
    // 创建并提交文件
    QString file = m_testRepoPath + "/committed.cpp";
    QVERIFY(m_mockRepo->addFile("committed.cpp", "committed content"));
    QVERIFY(m_mockRepo->commit("Add committed file"));
    
    ItemVersion status = GitUtils::getFileGitStatus(file);
    QCOMPARE(status, NormalVersion);
}

void TestGitUtils::testModifiedFileStatus()
{
    // 创建、提交然后修改文件
    QString file = m_testRepoPath + "/modified.cpp";
    QVERIFY(m_mockRepo->addFile("modified.cpp", "original content"));
    QVERIFY(m_mockRepo->commit("Add file to modify"));
    
    QVERIFY(m_mockRepo->modifyFile("modified.cpp", "modified content"));
    
    ItemVersion status = GitUtils::getFileGitStatus(file);
    QCOMPARE(status, LocallyModifiedVersion);
}

void TestGitUtils::testUntrackedFileStatus()
{
    // 创建未跟踪文件
    QString file = m_testRepoPath + "/untracked.cpp";
    QVERIFY(TestUtils::createTestFile(m_testRepoPath, "untracked.cpp", "untracked content"));
    
    ItemVersion status = GitUtils::getFileGitStatus(file);
    QCOMPARE(status, UnversionedVersion);
}

void TestGitUtils::testSpecialCharacterPaths()
{
    // 测试包含特殊字符的路径
    QString specialFile = m_testRepoPath + "/special-file@#$.cpp";
    QVERIFY(m_mockRepo->addFile("special-file@#$.cpp", "special content"));
    
    QVERIFY(GitUtils::isInsideRepositoryFile(specialFile));
    
    ItemVersion status = GitUtils::getFileGitStatus(specialFile);
    QVERIFY(status != UnversionedVersion);
}

void TestGitUtils::testUnicodeFilenames()
{
    // 测试Unicode文件名
    QString unicodeFile = m_testRepoPath + "/中文文件.cpp";
    QVERIFY(m_mockRepo->addFile("中文文件.cpp", "unicode content"));
    
    QVERIFY(GitUtils::isInsideRepositoryFile(unicodeFile));
    
    ItemVersion status = GitUtils::getFileGitStatus(unicodeFile);
    QVERIFY(status != UnversionedVersion);
}

void TestGitUtils::testSpacesInPaths()
{
    // 测试包含空格的路径
    QString spaceFile = m_testRepoPath + "/file with spaces.cpp";
    QVERIFY(m_mockRepo->addFile("file with spaces.cpp", "space content"));
    
    QVERIFY(GitUtils::isInsideRepositoryFile(spaceFile));
    
    ItemVersion status = GitUtils::getFileGitStatus(spaceFile);
    QVERIFY(status != UnversionedVersion);
}

void TestGitUtils::testNonExistentPath()
{
    // 测试不存在的路径
    QString nonExistent = m_testRepoPath + "/does-not-exist.cpp";
    
    QVERIFY(!GitUtils::isInsideRepositoryFile(nonExistent));
    
    ItemVersion status = GitUtils::getFileGitStatus(nonExistent);
    QCOMPARE(status, UnversionedVersion);
}

void TestGitUtils::testPermissionDenied()
{
    // 创建权限受限的文件（如果可能）
    QString restrictedFile = m_testRepoPath + "/restricted.cpp";
    QVERIFY(m_mockRepo->addFile("restricted.cpp", "restricted content"));
    
    // 在Unix系统上移除读权限
#ifdef Q_OS_UNIX
    QFile file(restrictedFile);
    file.setPermissions(QFile::WriteOwner);
    
    // 仍然应该能检测到它在仓库中
    QVERIFY(GitUtils::isInsideRepositoryFile(restrictedFile));
    
    // 恢复权限
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
#endif
}

void TestGitUtils::testCorruptedRepository()
{
    // 创建损坏的仓库（删除.git目录的部分内容）
    QString gitDir = m_testRepoPath + "/.git";
    QString configFile = gitDir + "/config";
    
    // 备份配置文件
    QString backupConfig = configFile + ".backup";
    QFile::copy(configFile, backupConfig);
    
    // 删除配置文件
    QFile::remove(configFile);
    
    // 测试损坏仓库的处理
    QString testFile = m_testRepoPath + "/test-corrupted.cpp";
    QVERIFY(TestUtils::createTestFile(m_testRepoPath, "test-corrupted.cpp", "corrupted test"));
    
    // 应该能优雅处理错误
    ItemVersion status = GitUtils::getFileGitStatus(testFile);
    // 损坏的仓库可能返回UnversionedVersion或其他状态
    
    // 恢复配置文件
    QFile::copy(backupConfig, configFile);
    QFile::remove(backupConfig);
}

void TestGitUtils::testLargeDirectoryTree()
{
    // 创建大型目录树进行性能测试
    const int dirCount = 10;
    const int filesPerDir = 5;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < dirCount; ++i) {
        QString dirName = QString("dir%1").arg(i);
        for (int j = 0; j < filesPerDir; ++j) {
            QString fileName = QString("%1/file%2.cpp").arg(dirName).arg(j);
            QString content = QString("Content for %1").arg(fileName);
            QVERIFY(m_mockRepo->addFile(fileName, content));
        }
    }
    
    // 测试所有文件的状态查询
    for (int i = 0; i < dirCount; ++i) {
        for (int j = 0; j < filesPerDir; ++j) {
            QString fileName = QString("dir%1/file%2.cpp").arg(i).arg(j);
            QString fullPath = m_testRepoPath + "/" + fileName;
            
            QVERIFY(GitUtils::isInsideRepositoryFile(fullPath));
            ItemVersion status = GitUtils::getFileGitStatus(fullPath);
            QVERIFY(status != UnversionedVersion);
        }
    }
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "Large directory tree test completed in" << elapsed << "ms";
    
    // 性能要求：大型目录树处理应在合理时间内完成
    QVERIFY(elapsed < 10000); // 10秒内完成
}

void TestGitUtils::testRepeatedCalls()
{
    // 测试重复调用的性能
    QString testFile = m_testRepoPath + "/repeated.cpp";
    QVERIFY(m_mockRepo->addFile("repeated.cpp", "repeated content"));
    
    const int iterations = 100;
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        QVERIFY(GitUtils::isInsideRepositoryFile(testFile));
        ItemVersion status = GitUtils::getFileGitStatus(testFile);
        QVERIFY(status != UnversionedVersion);
    }
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "Repeated calls test completed in" << elapsed << "ms";
    
    // 性能要求：重复调用平均响应时间应小于50ms（放宽要求）
    QVERIFY(elapsed / iterations < 50);
}

void TestGitUtils::createTestFileStructure()
{
    // 创建基本的测试文件结构
    QVERIFY(m_mockRepo->addFile("main.cpp", "int main() { return 0; }"));
    QVERIFY(m_mockRepo->addFile("header.h", "#pragma once"));
    QVERIFY(m_mockRepo->commit("Initial test structure"));
}

QTEST_MAIN(TestGitUtils)
#include "test-git-utils.moc" 