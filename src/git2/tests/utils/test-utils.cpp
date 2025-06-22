#include "test-utils.h"
#include <QDebug>
#include <QThread>
#include <QCoreApplication>

bool TestUtils::createTestGitRepository(const QString &repoPath)
{
    QDir dir;
    if (!dir.mkpath(repoPath)) {
        qWarning() << "Failed to create directory:" << repoPath;
        return false;
    }
    
    qDebug() << "Creating git repository at:" << repoPath;
    
    // 初始化Git仓库
    QProcess process;
    process.setWorkingDirectory(repoPath);
    
    // 设置环境变量，确保能找到git
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    process.setProcessEnvironment(env);
    
    qDebug() << "Starting git init...";
    process.start("git", {"init"});
    if (!process.waitForFinished(5000)) {
        qWarning() << "Git init failed:" << process.errorString();
        return false;
    }
    
    qDebug() << "Git init exit code:" << process.exitCode();
    QString stdout = QString::fromLocal8Bit(process.readAllStandardOutput());
    QString stderr = QString::fromLocal8Bit(process.readAllStandardError());
    if (!stdout.isEmpty()) qDebug() << "Git init stdout:" << stdout;
    if (!stderr.isEmpty()) qDebug() << "Git init stderr:" << stderr;
    
    if (process.exitCode() != 0) {
        qWarning() << "Git init failed with exit code:" << process.exitCode();
        return false;
    }
    
    // 配置用户信息
    qDebug() << "Configuring git user...";
    process.start("git", {"config", "user.name", "Test User"});
    if (!process.waitForFinished(3000)) {
        qWarning() << "Git config user.name timeout";
        return false;
    }
    qDebug() << "Git config user.name exit code:" << process.exitCode();
    
    process.start("git", {"config", "user.email", "test@example.com"});  
    if (!process.waitForFinished(3000)) {
        qWarning() << "Git config user.email timeout";
        return false;
    }
    qDebug() << "Git config user.email exit code:" << process.exitCode();
    
    bool success = (process.exitCode() == 0);
    qDebug() << "Git repository creation" << (success ? "succeeded" : "failed");
    return success;
}

bool TestUtils::createTestFile(const QString &repoPath, const QString &fileName, const QString &content)
{
    QString filePath = repoPath + "/" + fileName;
    
    // 确保父目录存在
    QFileInfo fileInfo(filePath);
    QDir parentDir = fileInfo.dir();
    if (!parentDir.exists()) {
        if (!parentDir.mkpath(".")) {
            qWarning() << "Failed to create parent directory for:" << filePath;
            return false;
        }
    }
    
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create file:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    return true;
}

bool TestUtils::modifyTestFile(const QString &filePath, const QString &content)
{
    QFile file(filePath);
    
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to modify file:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    return true;
}

QString TestUtils::executeGitCommand(const QString &repoPath, const QString &command, const QStringList &args)
{
    QProcess process;
    process.setWorkingDirectory(repoPath);
    
    QStringList fullArgs;
    fullArgs << command << args;
    
    qDebug() << "Executing git command:" << "git" << fullArgs << "in" << repoPath;
    
    process.start("git", fullArgs);
    if (!process.waitForFinished(10000)) {
        qWarning() << "Git command timeout:" << command << args;
        return QString();
    }
    
    QString stderr = QString::fromLocal8Bit(process.readAllStandardError());
    QString stdout = QString::fromLocal8Bit(process.readAllStandardOutput());
    
    qDebug() << "Git command exit code:" << process.exitCode();
    if (!stderr.isEmpty()) {
        qDebug() << "Git stderr:" << stderr;
    }
    if (!stdout.isEmpty()) {
        qDebug() << "Git stdout:" << stdout;
    }
    
    if (process.exitCode() != 0) {
        qWarning() << "Git command failed:" << command << args 
                   << "Exit code:" << process.exitCode()
                   << "Error:" << stderr;
        // 对于某些命令（如add, commit），即使失败也可能需要返回输出
        // 但为了调试，我们先返回空字符串
        return QString();
    }
    
    return stdout.trimmed();
}

void TestUtils::waitForFileSystemEvents(int ms)
{
    QThread::msleep(ms);
    QCoreApplication::processEvents();
}

bool TestUtils::verifyFileStatus(const QString &filePath, int expectedStatus)
{
    // 这里需要根据实际的状态枚举来实现
    // 暂时返回true，具体实现需要依赖GitUtils
    Q_UNUSED(filePath)
    Q_UNUSED(expectedStatus)
    return true;
}

void TestUtils::cleanupTestData(const QString &path)
{
    QDir dir(path);
    if (dir.exists()) {
        dir.removeRecursively();
    }
} 