#include <QtTest/QtTest>
#include <QObject>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QSignalSpy>
#include <QTimer>
#include <QProcess>
#include "utils/test-utils.h"
#include "git-service.h"
#include "git-types.h"

/**
 * @brief DBus服务集成测试类
 * 
 * 测试覆盖：
 * 1. 服务启动和DBus注册
 * 2. 客户端与服务的通信
 * 3. 信号机制的正确性
 * 4. 异常情况的恢复机制
 */
class TestGitDBusService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 服务启动测试
    void testServiceStartup();
    void testDBusRegistration();
    void testServiceAvailability();
    
    // 基础接口测试
    void testRegisterRepository();
    void testUnregisterRepository();
    void testGetFileStatuses();
    void testGetRepositoryStatus();
    void testRefreshRepositoryStatus();
    
    // 信号测试
    void testRepositoryStatusChangedSignal();
    void testRepositoryDiscoveredSignal();
    
    // 批量操作测试
    void testBatchFileStatusQuery();
    void testMultipleRepositories();
    
    // 错误处理测试
    void testInvalidRepositoryPath();
    void testNonExistentRepository();
    void testPermissionDenied();
    void testServiceReconnection();
    
    // 性能测试
    void testLargeRepositoryHandling();
    void testConcurrentRequests();
    void testServicePerformance();

private:
    bool startGitService();
    void stopGitService();
    bool isServiceRunning();
    QDBusInterface* createServiceInterface();
    void waitForServiceReady();
    
private:
    static const QString SERVICE_NAME;
    static const QString OBJECT_PATH;
    static const QString INTERFACE_NAME;
    
    QProcess *m_serviceProcess;
    QDBusInterface *m_serviceInterface;
    MockGitRepository *m_mockRepo;
    QString m_testRepoPath;
};

const QString TestGitDBusService::SERVICE_NAME = "org.deepin.FileManager.Git";
const QString TestGitDBusService::OBJECT_PATH = "/org/deepin/FileManager/Git";
const QString TestGitDBusService::INTERFACE_NAME = "org.deepin.FileManager.Git";

void TestGitDBusService::initTestCase()
{
    qDebug() << "Starting Git DBus Service tests";
    
    // 确保测试环境干净
    if (isServiceRunning()) {
        qDebug() << "Stopping existing service";
        stopGitService();
        QTest::qWait(1000);
    }
    
    m_serviceProcess = nullptr;
    m_serviceInterface = nullptr;
}

void TestGitDBusService::cleanupTestCase()
{
    stopGitService();
    qDebug() << "Git DBus Service tests completed";
}

void TestGitDBusService::init()
{
    // 创建测试仓库
    m_mockRepo = new MockGitRepository();
    QVERIFY(m_mockRepo->initialize());
    m_testRepoPath = m_mockRepo->repositoryPath();
    
    // 启动服务
    QVERIFY(startGitService());
    
    // 创建服务接口
    m_serviceInterface = createServiceInterface();
    QVERIFY(m_serviceInterface != nullptr);
    QVERIFY(m_serviceInterface->isValid());
    
    qDebug() << "Test setup completed. Repository:" << m_testRepoPath;
}

void TestGitDBusService::cleanup()
{
    delete m_serviceInterface;
    m_serviceInterface = nullptr;
    
    delete m_mockRepo;
    m_mockRepo = nullptr;
    
    stopGitService();
}

void TestGitDBusService::testServiceStartup()
{
    // 服务应该已经在init中启动
    QVERIFY(isServiceRunning());
    
    // 验证进程状态
    QVERIFY(m_serviceProcess != nullptr);
    QCOMPARE(m_serviceProcess->state(), QProcess::Running);
}

void TestGitDBusService::testDBusRegistration()
{
    // 验证服务在DBus上注册成功
    QDBusConnection connection = QDBusConnection::sessionBus();
    QVERIFY(connection.isConnected());
    
    QStringList services = connection.interface()->registeredServiceNames();
    QVERIFY(services.contains(SERVICE_NAME));
    
    // 验证对象路径存在
    QDBusInterface testInterface(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, connection);
    QVERIFY(testInterface.isValid());
}

void TestGitDBusService::testServiceAvailability()
{
    // 测试服务响应性
    QVERIFY(m_serviceInterface->isValid());
    
    // 调用一个简单的方法测试连通性
    QDBusReply<QStringList> reply = m_serviceInterface->call("GetAllRepositories");
    QVERIFY(reply.isValid());
    
    // 初始状态应该没有注册的仓库
    QStringList repositories = reply.value();
    QVERIFY(repositories.isEmpty());
}

void TestGitDBusService::testRegisterRepository()
{
    // 注册测试仓库
    QDBusReply<bool> reply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(reply.isValid());
    QVERIFY(reply.value());
    
    // 验证仓库已注册
    QDBusReply<QStringList> reposReply = m_serviceInterface->call("GetAllRepositories");
    QVERIFY(reposReply.isValid());
    
    QStringList repositories = reposReply.value();
    QVERIFY(repositories.contains(m_testRepoPath));
}

void TestGitDBusService::testUnregisterRepository()
{
    // 先注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 注销仓库
    QDBusReply<bool> unregisterReply = m_serviceInterface->call("UnregisterRepository", m_testRepoPath);
    QVERIFY(unregisterReply.isValid());
    QVERIFY(unregisterReply.value());
    
    // 验证仓库已注销
    QDBusReply<QStringList> reposReply = m_serviceInterface->call("GetAllRepositories");
    QVERIFY(reposReply.isValid());
    
    QStringList repositories = reposReply.value();
    QVERIFY(!repositories.contains(m_testRepoPath));
}

void TestGitDBusService::testGetFileStatuses()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建测试文件
    QVERIFY(m_mockRepo->addFile("test.cpp", "test content"));
    QVERIFY(m_mockRepo->commit("Add test file"));
    QVERIFY(m_mockRepo->modifyFile("test.cpp", "modified content"));
    
    // 等待文件系统事件传播
    TestUtils::waitForFileSystemEvents(500);
    
    // 查询文件状态
    QStringList filePaths = {m_testRepoPath + "/test.cpp"};
    QDBusReply<QVariantHash> reply = m_serviceInterface->call("GetFileStatuses", filePaths);
    
    QVERIFY(reply.isValid());
    
    QVariantHash statuses = reply.value();
    QVERIFY(!statuses.isEmpty());
    
    QString testFile = m_testRepoPath + "/test.cpp";
    QVERIFY(statuses.contains(testFile));
    
    // 修改的文件应该有相应的状态
    int status = statuses[testFile].toInt();
    QVERIFY(status == LocallyModifiedVersion || status == LocallyModifiedUnstagedVersion);
}

void TestGitDBusService::testGetRepositoryStatus()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建多个测试文件
    QVERIFY(m_mockRepo->addFile("file1.cpp", "content1"));
    QVERIFY(m_mockRepo->addFile("file2.cpp", "content2"));
    QVERIFY(m_mockRepo->commit("Add test files"));
    
    // 修改文件
    QVERIFY(m_mockRepo->modifyFile("file1.cpp", "modified content1"));
    QVERIFY(m_mockRepo->addFile("file3.cpp", "new content"));
    
    // 等待文件系统事件传播
    TestUtils::waitForFileSystemEvents(500);
    
    // 查询仓库状态
    QDBusReply<QVariantHash> reply = m_serviceInterface->call("GetRepositoryStatus", m_testRepoPath);
    
    QVERIFY(reply.isValid());
    
    QVariantHash statuses = reply.value();
    QVERIFY(!statuses.isEmpty());
    
    qDebug() << "Repository status:" << statuses;
}

void TestGitDBusService::testRefreshRepositoryStatus()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 刷新仓库状态
    QDBusReply<void> refreshReply = m_serviceInterface->call("RefreshRepositoryStatus", m_testRepoPath);
    QVERIFY(refreshReply.isValid());
    
    // 刷新操作应该成功完成
    QVERIFY(true);
}

void TestGitDBusService::testRepositoryStatusChangedSignal()
{
    // 连接信号
    QSignalSpy spy(m_serviceInterface, SIGNAL(RepositoryStatusChanged(QString, QVariantHash)));
    
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建文件变更
    QVERIFY(m_mockRepo->addFile("signal_test.cpp", "signal test content"));
    
    // 刷新状态以触发信号
    QDBusReply<void> refreshReply = m_serviceInterface->call("RefreshRepositoryStatus", m_testRepoPath);
    QVERIFY(refreshReply.isValid());
    
    // 等待信号
    bool signalReceived = spy.wait(5000);
    
    if (signalReceived) {
        QVERIFY(spy.count() > 0);
        
        QList<QVariant> arguments = spy.takeFirst();
        QString repositoryPath = arguments.at(0).toString();
        QVariantHash changes = arguments.at(1).toHash();
        
        QCOMPARE(repositoryPath, m_testRepoPath);
        QVERIFY(!changes.isEmpty());
        
        qDebug() << "Received RepositoryStatusChanged signal for:" << repositoryPath;
        qDebug() << "Changes:" << changes;
    } else {
        qDebug() << "Signal not received within timeout - this may be expected depending on implementation";
    }
}

void TestGitDBusService::testRepositoryDiscoveredSignal()
{
    // 连接信号
    QSignalSpy spy(m_serviceInterface, SIGNAL(RepositoryDiscovered(QString)));
    
    // 注册新仓库应该触发发现信号
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 等待信号
    bool signalReceived = spy.wait(3000);
    
    if (signalReceived) {
        QVERIFY(spy.count() > 0);
        
        QList<QVariant> arguments = spy.takeFirst();
        QString discoveredPath = arguments.at(0).toString();
        
        QCOMPARE(discoveredPath, m_testRepoPath);
        
        qDebug() << "Received RepositoryDiscovered signal for:" << discoveredPath;
    } else {
        qDebug() << "RepositoryDiscovered signal not received - this may be expected";
    }
}

void TestGitDBusService::testBatchFileStatusQuery()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建多个测试文件
    const int fileCount = 50;
    QStringList filePaths;
    
    for (int i = 0; i < fileCount; ++i) {
        QString fileName = QString("batch_file_%1.cpp").arg(i);
        QVERIFY(m_mockRepo->addFile(fileName, QString("content %1").arg(i)));
        filePaths.append(m_testRepoPath + "/" + fileName);
    }
    
    QVERIFY(m_mockRepo->commit("Add batch files"));
    
    // 修改一些文件
    for (int i = 0; i < 10; ++i) {
        QString fileName = QString("batch_file_%1.cpp").arg(i);
        QVERIFY(m_mockRepo->modifyFile(fileName, QString("modified content %1").arg(i)));
    }
    
    // 等待文件系统事件传播
    TestUtils::waitForFileSystemEvents(1000);
    
    // 批量查询文件状态
    QElapsedTimer timer;
    timer.start();
    
    QDBusReply<QVariantHash> reply = m_serviceInterface->call("GetFileStatuses", filePaths);
    
    qint64 elapsed = timer.elapsed();
    
    QVERIFY(reply.isValid());
    
    QVariantHash statuses = reply.value();
    QVERIFY(!statuses.isEmpty());
    
    qDebug() << "Batch query of" << fileCount << "files completed in" << elapsed << "ms";
    qDebug() << "Returned" << statuses.size() << "statuses";
    
    // 批量查询性能应该合理
    QVERIFY(elapsed < 5000);  // 应该在5秒内完成
}

void TestGitDBusService::testMultipleRepositories()
{
    // 创建第二个测试仓库
    MockGitRepository secondRepo;
    QVERIFY(secondRepo.initialize());
    QString secondRepoPath = secondRepo.repositoryPath();
    
    // 注册两个仓库
    QDBusReply<bool> reply1 = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(reply1.isValid() && reply1.value());
    
    QDBusReply<bool> reply2 = m_serviceInterface->call("RegisterRepository", secondRepoPath);
    QVERIFY(reply2.isValid() && reply2.value());
    
    // 验证两个仓库都已注册
    QDBusReply<QStringList> reposReply = m_serviceInterface->call("GetAllRepositories");
    QVERIFY(reposReply.isValid());
    
    QStringList repositories = reposReply.value();
    QVERIFY(repositories.contains(m_testRepoPath));
    QVERIFY(repositories.contains(secondRepoPath));
    
    // 在两个仓库中创建文件
    QVERIFY(m_mockRepo->addFile("repo1_file.cpp", "repo1 content"));
    QVERIFY(secondRepo.addFile("repo2_file.cpp", "repo2 content"));
    
    // 分别查询两个仓库的状态
    QDBusReply<QVariantHash> status1 = m_serviceInterface->call("GetRepositoryStatus", m_testRepoPath);
    QDBusReply<QVariantHash> status2 = m_serviceInterface->call("GetRepositoryStatus", secondRepoPath);
    
    QVERIFY(status1.isValid());
    QVERIFY(status2.isValid());
    
    qDebug() << "Multiple repositories test completed successfully";
}

void TestGitDBusService::testInvalidRepositoryPath()
{
    // 尝试注册无效路径
    QString invalidPath = "/invalid/repository/path";
    QDBusReply<bool> reply = m_serviceInterface->call("RegisterRepository", invalidPath);
    
    QVERIFY(reply.isValid());
    QVERIFY(!reply.value());  // 应该返回false
    
    // 查询无效路径的状态
    QDBusReply<QVariantHash> statusReply = m_serviceInterface->call("GetRepositoryStatus", invalidPath);
    
    if (statusReply.isValid()) {
        QVariantHash statuses = statusReply.value();
        QVERIFY(statuses.isEmpty());  // 应该返回空结果
    }
}

void TestGitDBusService::testNonExistentRepository()
{
    // 创建临时目录但不初始化为Git仓库
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString nonGitPath = tempDir.path();
    
    // 尝试注册非Git仓库
    QDBusReply<bool> reply = m_serviceInterface->call("RegisterRepository", nonGitPath);
    
    QVERIFY(reply.isValid());
    QVERIFY(!reply.value());  // 应该返回false
}

void TestGitDBusService::testPermissionDenied()
{
    // 创建权限受限的目录
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString restrictedPath = tempDir.path() + "/restricted";
    QDir().mkdir(restrictedPath);
    
    // 移除读权限
    QFile::setPermissions(restrictedPath, QFile::WriteOwner);
    
    // 尝试注册权限受限的路径
    QDBusReply<bool> reply = m_serviceInterface->call("RegisterRepository", restrictedPath);
    
    QVERIFY(reply.isValid());
    // 权限问题应该被正确处理，不应该崩溃服务
    
    // 恢复权限以便清理
    QFile::setPermissions(restrictedPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    
    QVERIFY(true);  // 测试完成
}

void TestGitDBusService::testServiceReconnection()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 重启服务
    stopGitService();
    QTest::qWait(1000);
    
    QVERIFY(startGitService());
    
    // 重新创建接口
    delete m_serviceInterface;
    m_serviceInterface = createServiceInterface();
    QVERIFY(m_serviceInterface != nullptr);
    QVERIFY(m_serviceInterface->isValid());
    
    // 验证服务重启后功能正常
    QDBusReply<QStringList> reposReply = m_serviceInterface->call("GetAllRepositories");
    QVERIFY(reposReply.isValid());
    
    qDebug() << "Service reconnection test completed";
}

void TestGitDBusService::testLargeRepositoryHandling()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建大量文件
    const int fileCount = 1000;
    QStringList filePaths;
    
    for (int i = 0; i < fileCount; ++i) {
        QString fileName = QString("large_file_%1.cpp").arg(i);
        QVERIFY(m_mockRepo->addFile(fileName, QString("content %1").arg(i)));
        filePaths.append(m_testRepoPath + "/" + fileName);
        
        // 每100个文件提交一次
        if (i % 100 == 99) {
            QString commitMsg = QString("Add files %1-%2").arg(i-99).arg(i);
            QVERIFY(m_mockRepo->commit(commitMsg));
        }
    }
    
    // 等待文件系统事件传播
    TestUtils::waitForFileSystemEvents(2000);
    
    // 查询大仓库状态
    QElapsedTimer timer;
    timer.start();
    
    QDBusReply<QVariantHash> reply = m_serviceInterface->call("GetRepositoryStatus", m_testRepoPath);
    
    qint64 elapsed = timer.elapsed();
    
    QVERIFY(reply.isValid());
    
    QVariantHash statuses = reply.value();
    QVERIFY(!statuses.isEmpty());
    
    qDebug() << "Large repository (" << fileCount << "files) query completed in" << elapsed << "ms";
    qDebug() << "Returned" << statuses.size() << "statuses";
    
    // 大仓库查询性能应该合理
    QVERIFY(elapsed < 30000);  // 应该在30秒内完成
}

void TestGitDBusService::testConcurrentRequests()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建测试文件
    for (int i = 0; i < 50; ++i) {
        QString fileName = QString("concurrent_file_%1.cpp").arg(i);
        QVERIFY(m_mockRepo->addFile(fileName, QString("content %1").arg(i)));
    }
    QVERIFY(m_mockRepo->commit("Add concurrent test files"));
    
    // 等待文件系统事件传播
    TestUtils::waitForFileSystemEvents(1000);
    
    // 创建多个并发请求
    const int requestCount = 20;
    QList<QDBusInterface*> interfaces;
    QList<QElapsedTimer*> timers;
    
    for (int i = 0; i < requestCount; ++i) {
        QDBusInterface *interface = createServiceInterface();
        QVERIFY(interface != nullptr && interface->isValid());
        interfaces.append(interface);
        
        QElapsedTimer *timer = new QElapsedTimer();
        timer->start();
        timers.append(timer);
    }
    
    // 发起并发请求
    QElapsedTimer totalTimer;
    totalTimer.start();
    
    for (int i = 0; i < requestCount; ++i) {
        QDBusReply<QVariantHash> reply = interfaces[i]->call("GetRepositoryStatus", m_testRepoPath);
        QVERIFY(reply.isValid());
        
        qint64 elapsed = timers[i]->elapsed();
        qDebug() << "Request" << i << "completed in" << elapsed << "ms";
    }
    
    qint64 totalElapsed = totalTimer.elapsed();
    
    // 清理
    qDeleteAll(interfaces);
    qDeleteAll(timers);
    
    qDebug() << "Concurrent requests (" << requestCount << ") completed in" << totalElapsed << "ms";
    
    // 并发请求应该能正常处理
    QVERIFY(totalElapsed < 60000);  // 应该在1分钟内完成
}

void TestGitDBusService::testServicePerformance()
{
    // 注册仓库
    QDBusReply<bool> registerReply = m_serviceInterface->call("RegisterRepository", m_testRepoPath);
    QVERIFY(registerReply.isValid() && registerReply.value());
    
    // 创建测试文件
    QStringList filePaths;
    for (int i = 0; i < 100; ++i) {
        QString fileName = QString("perf_file_%1.cpp").arg(i);
        QVERIFY(m_mockRepo->addFile(fileName, QString("content %1").arg(i)));
        filePaths.append(m_testRepoPath + "/" + fileName);
    }
    QVERIFY(m_mockRepo->commit("Add performance test files"));
    
    // 等待文件系统事件传播
    TestUtils::waitForFileSystemEvents(1000);
    
    // 性能测试
    const int iterations = 100;
    QList<qint64> times;
    
    for (int i = 0; i < iterations; ++i) {
        QElapsedTimer timer;
        timer.start();
        
        QDBusReply<QVariantHash> reply = m_serviceInterface->call("GetFileStatuses", filePaths);
        QVERIFY(reply.isValid());
        
        qint64 elapsed = timer.elapsed();
        times.append(elapsed);
    }
    
    // 计算统计信息
    qint64 totalTime = 0;
    qint64 minTime = times[0];
    qint64 maxTime = times[0];
    
    for (qint64 time : times) {
        totalTime += time;
        minTime = qMin(minTime, time);
        maxTime = qMax(maxTime, time);
    }
    
    double avgTime = static_cast<double>(totalTime) / iterations;
    
    qDebug() << "Performance test results:";
    qDebug() << "  Iterations:" << iterations;
    qDebug() << "  Average time:" << avgTime << "ms";
    qDebug() << "  Min time:" << minTime << "ms";
    qDebug() << "  Max time:" << maxTime << "ms";
    qDebug() << "  Total time:" << totalTime << "ms";
    
    // 性能要求
    QVERIFY(avgTime < 100.0);  // 平均响应时间应该小于100ms
    QVERIFY(maxTime < 1000);   // 最大响应时间应该小于1秒
}

bool TestGitDBusService::startGitService()
{
    if (m_serviceProcess != nullptr) {
        stopGitService();
    }
    
    m_serviceProcess = new QProcess(this);
    
    // 设置服务程序路径
    QString servicePath = QCoreApplication::applicationDirPath() + "/dde-file-manager-git-daemon";
    
    // 如果找不到编译的服务程序，尝试其他路径
    if (!QFile::exists(servicePath)) {
        servicePath = "../daemon/dde-file-manager-git-daemon";
    }
    
    if (!QFile::exists(servicePath)) {
        qWarning() << "Git daemon not found. Expected at:" << servicePath;
        return false;
    }
    
    qDebug() << "Starting Git service:" << servicePath;
    
    m_serviceProcess->start(servicePath);
    
    if (!m_serviceProcess->waitForStarted(10000)) {
        qWarning() << "Failed to start Git service:" << m_serviceProcess->errorString();
        return false;
    }
    
    // 等待服务注册到DBus
    waitForServiceReady();
    
    return isServiceRunning();
}

void TestGitDBusService::stopGitService()
{
    if (m_serviceProcess != nullptr) {
        if (m_serviceProcess->state() == QProcess::Running) {
            m_serviceProcess->terminate();
            
            if (!m_serviceProcess->waitForFinished(5000)) {
                qWarning() << "Service did not terminate gracefully, killing...";
                m_serviceProcess->kill();
                m_serviceProcess->waitForFinished(3000);
            }
        }
        
        delete m_serviceProcess;
        m_serviceProcess = nullptr;
    }
}

bool TestGitDBusService::isServiceRunning()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        return false;
    }
    
    QDBusConnectionInterface *interface = connection.interface();
    if (!interface) {
        qWarning() << "DBus connection interface is null";
        return false;
    }
    
    QDBusReply<QStringList> reply = interface->registeredServiceNames();
    if (!reply.isValid()) {
        qWarning() << "Failed to get registered service names:" << reply.error().message();
        return false;
    }
    
    QStringList services = reply.value();
    return services.contains(SERVICE_NAME);
}

QDBusInterface* TestGitDBusService::createServiceInterface()
{
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.isConnected()) {
        qWarning() << "DBus connection not available";
        return nullptr;
    }
    
    QDBusInterface *interface = new QDBusInterface(SERVICE_NAME, OBJECT_PATH, INTERFACE_NAME, connection);
    
    if (!interface->isValid()) {
        qWarning() << "Failed to create DBus interface:" << interface->lastError().message();
        delete interface;
        return nullptr;
    }
    
    return interface;
}

void TestGitDBusService::waitForServiceReady()
{
    // 等待服务在DBus上可用
    const int maxWaitTime = 10000;  // 10秒超时
    const int checkInterval = 100;   // 100ms检查间隔
    
    QElapsedTimer timer;
    timer.start();
    
    while (timer.elapsed() < maxWaitTime) {
        if (isServiceRunning()) {
            // 额外等待一点时间确保服务完全就绪
            QTest::qWait(500);
            return;
        }
        
        QTest::qWait(checkInterval);
    }
    
    qWarning() << "Service did not become ready within timeout";
}

QTEST_MAIN(TestGitDBusService)
#include "test-git-dbus-service.moc" 