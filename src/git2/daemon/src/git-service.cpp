#include "git-service.h"
#include "git-status-cache.h"
#include "git-repository-watcher.h"
#include "git-status-parser.h"
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QTimer>

GitService::GitService(QObject *parent)
    : QObject(parent)
    , m_serviceReady(false)
{
    // 连接缓存和监控器的信号
    connect(&GitStatusCache::instance(), &GitStatusCache::repositoryStatusChanged,
            this, &GitService::onRepositoryStatusChanged);
    connect(&GitStatusCache::instance(), &GitStatusCache::repositoryDiscovered,
            this, &GitService::onRepositoryDiscovered);

    // 初始化仓库监控器（作为成员变量需要在daemon中管理）
    // 这里我们通过信号连接而不是直接持有监控器实例
    
    m_serviceReady = true;
    qDebug() << "[GitService] DBus service initialized and ready";
}

GitService::~GitService()
{
    qDebug() << "[GitService] DBus service destroyed";
}

bool GitService::RegisterRepository(const QString &repositoryPath)
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::RegisterRepository] Service not ready";
        return false;
    }

    qDebug() << "[GitService::RegisterRepository] Registering repository:" << repositoryPath;
    
    bool result = GitStatusCache::instance().registerRepository(repositoryPath);
    
    if (result) {
        // 立即刷新仓库状态
        QTimer::singleShot(0, this, [this, repositoryPath]() {
            refreshRepositoryInternal(repositoryPath);
        });
    }
    
    return result;
}

bool GitService::UnregisterRepository(const QString &repositoryPath)
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::UnregisterRepository] Service not ready";
        return false;
    }

    qDebug() << "[GitService::UnregisterRepository] Unregistering repository:" << repositoryPath;
    return GitStatusCache::instance().unregisterRepository(repositoryPath);
}

QVariantMap GitService::GetFileStatuses(const QStringList &filePaths)
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::GetFileStatuses] Service not ready";
        return QVariantMap();
    }

    qDebug() << "[GitService::GetFileStatuses] Getting statuses for" << filePaths.size() << "files";
    
    QHash<QString, ItemVersion> statusMap = GitStatusCache::instance().getFileStatuses(filePaths);
    return convertToVariantMap(statusMap);
}

QVariantMap GitService::GetRepositoryStatus(const QString &repositoryPath)
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::GetRepositoryStatus] Service not ready";
        return QVariantMap();
    }

    qDebug() << "[GitService::GetRepositoryStatus] Getting status for repository:" << repositoryPath;
    
    QHash<QString, ItemVersion> statusMap = GitStatusCache::instance().getRepositoryStatus(repositoryPath);
    return convertToVariantMap(statusMap);
}

bool GitService::RefreshRepository(const QString &repositoryPath)
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::RefreshRepository] Service not ready";
        return false;
    }

    qDebug() << "[GitService::RefreshRepository] Refreshing repository:" << repositoryPath;
    
    // 异步执行刷新操作
    QTimer::singleShot(0, this, [this, repositoryPath]() {
        refreshRepositoryInternal(repositoryPath);
    });
    
    return true;
}

bool GitService::ClearRepositoryCache(const QString &repositoryPath)
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::ClearRepositoryCache] Service not ready";
        return false;
    }

    qDebug() << "[GitService::ClearRepositoryCache] Clearing cache for repository:" << repositoryPath;
    GitStatusCache::instance().clearRepositoryCache(repositoryPath);
    return true;
}

QStringList GitService::GetRegisteredRepositories()
{
    if (!m_serviceReady) {
        qWarning() << "[GitService::GetRegisteredRepositories] Service not ready";
        return QStringList();
    }

    return GitStatusCache::instance().getCachedRepositories();
}

QVariantMap GitService::GetServiceStatus()
{
    QVariantMap status;
    status["serviceReady"] = m_serviceReady;
    status["cacheSize"] = GitStatusCache::instance().getCacheSize();
    status["registeredRepositories"] = GitStatusCache::instance().getCachedRepositories().size();
    
    return status;
}

void GitService::onRepositoryChanged(const QString &repositoryPath)
{
    qDebug() << "[GitService::onRepositoryChanged] Repository changed:" << repositoryPath;
    
    // 异步刷新仓库状态
    QTimer::singleShot(0, this, [this, repositoryPath]() {
        refreshRepositoryInternal(repositoryPath);
    });
}

void GitService::onRepositoryStatusChanged(const QString &repositoryPath, const QHash<QString, ItemVersion> &changedFiles)
{
    qDebug() << "[GitService::onRepositoryStatusChanged] Repository status changed:" 
             << repositoryPath << "with" << changedFiles.size() << "changed files";
    
    QVariantMap variantMap = convertToVariantMap(changedFiles);
    Q_EMIT RepositoryStatusChanged(repositoryPath, variantMap);
}

void GitService::onRepositoryDiscovered(const QString &repositoryPath)
{
    qDebug() << "[GitService::onRepositoryDiscovered] Repository discovered:" << repositoryPath;
    Q_EMIT RepositoryDiscovered(repositoryPath);
}

QVariantMap GitService::convertToVariantMap(const QHash<QString, ItemVersion> &versionMap) const
{
    QVariantMap result;
    
    for (auto it = versionMap.begin(); it != versionMap.end(); ++it) {
        result.insert(it.key(), static_cast<int>(it.value()));
    }
    
    return result;
}

void GitService::refreshRepositoryInternal(const QString &repositoryPath)
{
    if (repositoryPath.isEmpty()) {
        qWarning() << "[GitService::refreshRepositoryInternal] Empty repository path";
        return;
    }

    QDir repoDir(repositoryPath);
    if (!repoDir.exists() || !repoDir.exists(".git")) {
        qWarning() << "[GitService::refreshRepositoryInternal] Invalid repository:" << repositoryPath;
        return;
    }

    qDebug() << "[GitService::refreshRepositoryInternal] Refreshing repository:" << repositoryPath;

    // 使用Git命令获取状态
    QProcess gitProcess;
    gitProcess.setWorkingDirectory(repositoryPath);
    gitProcess.setProgram("git");
    gitProcess.setArguments({"status", "--porcelain", "-z", "--ignored"});
    
    gitProcess.start();
    if (!gitProcess.waitForFinished(10000)) { // 10秒超时
        qWarning() << "[GitService::refreshRepositoryInternal] Git process timeout for repository:" << repositoryPath;
        return;
    }

    if (gitProcess.exitCode() != 0) {
        qWarning() << "[GitService::refreshRepositoryInternal] Git process failed for repository:" 
                   << repositoryPath << "Error:" << gitProcess.readAllStandardError();
        return;
    }

    // 解析Git状态输出
    QByteArray output = gitProcess.readAllStandardOutput();
    QHash<QString, ItemVersion> statusMap;
    
    if (!output.isEmpty()) {
        statusMap = GitStatusParser::parseGitStatus(QString::fromUtf8(output));
    }

    // 更新缓存
    GitStatusCache::instance().resetVersion(repositoryPath, statusMap);
    
    qDebug() << "[GitService::refreshRepositoryInternal] Updated repository:" 
             << repositoryPath << "with" << statusMap.size() << "file statuses";
} 