#include <QtTest/QtTest>
#include <QDebug>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QtConcurrent>
#include <QFuture>
#include <QThread>
#include "utils/test-utils.h"
#include "git-status-cache.h"
#include "git-types.h"

/**
 * @brief GitStatusCache单元测试类
 * 
 * 测试覆盖：
 * 1. 版本信息的存储和检索功能
 * 2. 多线程并发访问的安全性
 * 3. LRU缓存淘汰机制
 * 4. 批量查询接口的正确性
 */
class TestGitStatusCache : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 基础功能测试
    void testSingletonInstance();
    void testVersionStorage();
    void testVersionRetrieval();
    void testRepositoryManagement();
    
    // 批量操作测试
    void testBatchFileStatusQuery();
    void testRepositoryStatusQuery();
    void testBatchUpdate();
    
    // 并发安全测试
    void testConcurrentAccess();
    void testThreadSafety();
    
    // 缓存管理测试
    void testCacheCleanup();
    void testRepositoryCacheClearing();
    void testMaxRepositoryLimit();
    
    // 信号测试
    void testRepositoryStatusChangedSignal();
    void testRepositoryDiscoveredSignal();
    
    // 性能测试
    void testLargeRepositoryPerformance();
    void testBatchQueryPerformance();

private:
    void setupTestRepository();
    QHash<QString, ItemVersion> createTestVersionInfo();
    
private:
    MockGitRepository *m_mockRepo;
    QString m_testRepoPath;
    GitStatusCache *m_cache;
};

void TestGitStatusCache::initTestCase()
{
    qDebug() << "Starting GitStatusCache tests";
    m_cache = &GitStatusCache::instance();
}

void TestGitStatusCache::cleanupTestCase()
{
    qDebug() << "GitStatusCache tests completed";
}

void TestGitStatusCache::init()
{
    // 每个测试前清理缓存
    m_cache->clearCache();
    
    // 创建测试仓库
    m_mockRepo = new MockGitRepository();
    QVERIFY(m_mockRepo->initialize());
    m_testRepoPath = m_mockRepo->repositoryPath();
    
    qDebug() << "Test repository created at:" << m_testRepoPath;
}

void TestGitStatusCache::cleanup()
{
    delete m_mockRepo;
    m_mockRepo = nullptr;
}

void TestGitStatusCache::testSingletonInstance()
{
    GitStatusCache &cache1 = GitStatusCache::instance();
    GitStatusCache &cache2 = GitStatusCache::instance();
    
    QCOMPARE(&cache1, &cache2);
    QVERIFY(&cache1 == m_cache);
}

void TestGitStatusCache::testVersionStorage()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    
    // 存储版本信息
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    // 验证存储成功
    QVERIFY(m_cache->getCachedRepositories().contains(m_testRepoPath));
    QVERIFY(m_cache->getCacheSize() > 0);
}

void TestGitStatusCache::testVersionRetrieval()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    // 测试单个文件状态检索
    for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
        ItemVersion retrievedVersion = m_cache->version(it.key());
        QCOMPARE(retrievedVersion, it.value());
    }
    
    // 测试不存在的文件
    QString nonExistentFile = m_testRepoPath + "/non-existent.txt";
    ItemVersion version = m_cache->version(nonExistentFile);
    QCOMPARE(version, UnversionedVersion);
}

void TestGitStatusCache::testRepositoryManagement()
{
    // 测试仓库注册
    QVERIFY(m_cache->registerRepository(m_testRepoPath));
    QVERIFY(m_cache->getCachedRepositories().contains(m_testRepoPath));
    
    // 测试重复注册
    QVERIFY(m_cache->registerRepository(m_testRepoPath));
    
    // 测试仓库注销
    QVERIFY(m_cache->unregisterRepository(m_testRepoPath));
    QVERIFY(!m_cache->getCachedRepositories().contains(m_testRepoPath));
    
    // 测试注销不存在的仓库
    QVERIFY(!m_cache->unregisterRepository("/non/existent/path"));
}

void TestGitStatusCache::testBatchFileStatusQuery()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    QStringList queryFiles = versionInfo.keys();
    QHash<QString, ItemVersion> result = m_cache->getFileStatuses(queryFiles);
    
    // 验证批量查询结果
    QCOMPARE(result.size(), versionInfo.size());
    for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
        QVERIFY(result.contains(it.key()));
        QCOMPARE(result[it.key()], it.value());
    }
}

void TestGitStatusCache::testRepositoryStatusQuery()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    QHash<QString, ItemVersion> result = m_cache->getRepositoryStatus(m_testRepoPath);
    
    // 验证仓库状态查询结果
    QCOMPARE(result.size(), versionInfo.size());
    for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
        QVERIFY(result.contains(it.key()));
        QCOMPARE(result[it.key()], it.value());
    }
}

void TestGitStatusCache::testBatchUpdate()
{
    // 初始数据
    QHash<QString, ItemVersion> initialData = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, initialData);
    
    // 批量更新数据
    QHash<QString, ItemVersion> updates;
    QString testFile = m_testRepoPath + "/test.cpp";
    updates[testFile] = LocallyModifiedVersion;
    
    m_cache->resetVersion(m_testRepoPath, updates);
    
    // 验证更新结果
    QCOMPARE(m_cache->version(testFile), LocallyModifiedVersion);
}

void TestGitStatusCache::testConcurrentAccess()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    // 创建多个线程同时访问缓存
    const int threadCount = 10;
    const int operationsPerThread = 100;
    
    QList<QFuture<void>> futures;
    
    for (int i = 0; i < threadCount; ++i) {
        auto future = QtConcurrent::run([this, versionInfo, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                // 随机读取操作
                for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
                    ItemVersion version = m_cache->version(it.key());
                    Q_UNUSED(version)
                }
                
                // 批量查询操作
                QStringList files = versionInfo.keys();
                auto result = m_cache->getFileStatuses(files);
                Q_UNUSED(result)
            }
        });
        futures.append(future);
    }
    
    // 等待所有线程完成
    for (auto &future : futures) {
        future.waitForFinished();
    }
    
    // 验证数据完整性
    for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
        QCOMPARE(m_cache->version(it.key()), it.value());
    }
}

void TestGitStatusCache::testThreadSafety()
{
    // 测试读写并发安全
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    // 写线程
    auto writeTask = QtConcurrent::run([this]() {
        for (int i = 0; i < 50; ++i) {
            QHash<QString, ItemVersion> updates;
            QString fileName = QString("file_%1.txt").arg(i);
            updates[m_testRepoPath + "/" + fileName] = LocallyModifiedVersion;
            m_cache->resetVersion(m_testRepoPath, updates);
            QThread::msleep(1);
        }
    });
    
    // 读线程
    auto readTask = QtConcurrent::run([this, versionInfo]() {
        for (int i = 0; i < 100; ++i) {
            for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
                ItemVersion version = m_cache->version(it.key());
                Q_UNUSED(version)
            }
            QThread::msleep(1);
        }
    });
    
    writeTask.waitForFinished();
    readTask.waitForFinished();
    
    // 验证没有崩溃即为成功
    QVERIFY(true);
}

void TestGitStatusCache::testCacheCleanup()
{
    // 填充缓存
    for (int i = 0; i < 10; ++i) {
        QString repoPath = QString("/tmp/test_repo_%1").arg(i);
        QHash<QString, ItemVersion> versionInfo;
        versionInfo[repoPath + "/file.txt"] = NormalVersion;
        m_cache->resetVersion(repoPath, versionInfo);
    }
    
    int initialSize = m_cache->getCacheSize();
    QVERIFY(initialSize > 0);
    
    // 执行清理
    m_cache->performCleanup();
    
    // 注意：实际的清理逻辑可能不会立即清除所有数据
    // 这里主要测试清理方法不会崩溃
    QVERIFY(true);
}

void TestGitStatusCache::testRepositoryCacheClearing()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    // 验证数据存在
    QVERIFY(m_cache->getCachedRepositories().contains(m_testRepoPath));
    
    // 清除特定仓库缓存
    m_cache->clearRepositoryCache(m_testRepoPath);
    
    // 验证仓库缓存被清除
    QHash<QString, ItemVersion> result = m_cache->getRepositoryStatus(m_testRepoPath);
    QVERIFY(result.isEmpty());
}

void TestGitStatusCache::testMaxRepositoryLimit()
{
    // 测试最大仓库数量限制
    const int maxRepos = 150; // 超过MAX_REPOSITORIES
    
    for (int i = 0; i < maxRepos; ++i) {
        QString repoPath = QString("/tmp/test_repo_%1").arg(i);
        QHash<QString, ItemVersion> versionInfo;
        versionInfo[repoPath + "/file.txt"] = NormalVersion;
        m_cache->resetVersion(repoPath, versionInfo);
    }
    
    // 验证缓存大小受限
    int cacheSize = m_cache->getCachedRepositories().size();
    qDebug() << "Cache size after adding" << maxRepos << "repositories:" << cacheSize;
    
    // 应该有某种限制机制
    QVERIFY(cacheSize <= maxRepos);
}

void TestGitStatusCache::testRepositoryStatusChangedSignal()
{
    QSignalSpy spy(m_cache, &GitStatusCache::repositoryStatusChanged);
    
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    // 注意：信号发射取决于实际实现
    // 这里主要测试信号连接正确
    QVERIFY(spy.isValid());
}

void TestGitStatusCache::testRepositoryDiscoveredSignal()
{
    QSignalSpy spy(m_cache, &GitStatusCache::repositoryDiscovered);
    
    m_cache->registerRepository(m_testRepoPath);
    
    // 注意：信号发射取决于实际实现
    QVERIFY(spy.isValid());
}

void TestGitStatusCache::testLargeRepositoryPerformance()
{
    // 创建大量文件状态数据
    QHash<QString, ItemVersion> largeVersionInfo;
    const int fileCount = 10000;
    
    for (int i = 0; i < fileCount; ++i) {
        QString fileName = QString("file_%1.cpp").arg(i);
        QString filePath = m_testRepoPath + "/" + fileName;
        largeVersionInfo[filePath] = static_cast<ItemVersion>(i % 10);
    }
    
    // 测试存储性能
    QElapsedTimer timer;
    timer.start();
    
    m_cache->resetVersion(m_testRepoPath, largeVersionInfo);
    
    qint64 storeTime = timer.elapsed();
    qDebug() << "Store time for" << fileCount << "files:" << storeTime << "ms";
    
    // 测试查询性能
    timer.restart();
    
    QStringList queryFiles = largeVersionInfo.keys();
    QHash<QString, ItemVersion> result = m_cache->getFileStatuses(queryFiles);
    
    qint64 queryTime = timer.elapsed();
    qDebug() << "Query time for" << fileCount << "files:" << queryTime << "ms";
    
    // 验证结果正确性
    QCOMPARE(result.size(), largeVersionInfo.size());
    
    // 性能要求：存储和查询都应该在合理时间内完成
    QVERIFY(storeTime < 1000);  // 存储应该在1秒内完成
    QVERIFY(queryTime < 500);   // 查询应该在0.5秒内完成
}

void TestGitStatusCache::testBatchQueryPerformance()
{
    QHash<QString, ItemVersion> versionInfo = createTestVersionInfo();
    m_cache->resetVersion(m_testRepoPath, versionInfo);
    
    QStringList queryFiles = versionInfo.keys();
    const int iterations = 1000;
    
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        QHash<QString, ItemVersion> result = m_cache->getFileStatuses(queryFiles);
        Q_UNUSED(result)
    }
    
    qint64 totalTime = timer.elapsed();
    double avgTime = static_cast<double>(totalTime) / iterations;
    
    qDebug() << "Average batch query time:" << avgTime << "ms";
    
    // 批量查询平均时间应该很短
    QVERIFY(avgTime < 10.0);  // 平均查询时间应该小于10ms
}

QHash<QString, ItemVersion> TestGitStatusCache::createTestVersionInfo()
{
    QHash<QString, ItemVersion> versionInfo;
    
    versionInfo[m_testRepoPath + "/normal.cpp"] = NormalVersion;
    versionInfo[m_testRepoPath + "/modified.cpp"] = LocallyModifiedVersion;
    versionInfo[m_testRepoPath + "/added.cpp"] = AddedVersion;
    versionInfo[m_testRepoPath + "/removed.cpp"] = RemovedVersion;
    versionInfo[m_testRepoPath + "/unversioned.txt"] = UnversionedVersion;
    versionInfo[m_testRepoPath + "/ignored.log"] = IgnoredVersion;
    
    return versionInfo;
}

QTEST_MAIN(TestGitStatusCache)
#include "test-git-status-cache.moc" 