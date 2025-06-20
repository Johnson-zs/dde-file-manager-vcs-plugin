#include "git-status-cache.h"
#include "git-utils.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>

GitStatusCache& GitStatusCache::instance()
{
    static GitStatusCache ins;
    return ins;
}

GitStatusCache::GitStatusCache(QObject *parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
    , m_cacheHits(0)
    , m_cacheMisses(0)
{
    // 设置清理定时器
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL_MS);
    m_cleanupTimer->setSingleShot(false);
    connect(m_cleanupTimer, &QTimer::timeout, this, &GitStatusCache::onCleanupCache);
    m_cleanupTimer->start();
    
    qDebug() << "[GitStatusCache] Service initialized with cleanup interval" << CLEANUP_INTERVAL_MS << "ms";
}

void GitStatusCache::resetVersion(const QString &repositoryPath, const QHash<QString, ItemVersion> &versionInfo)
{
    QMutexLocker locker(&m_mutex);
    
    // 保存旧的状态用于比较
    QHash<QString, ItemVersion> oldVersionInfo;
    if (m_repositories.contains(repositoryPath)) {
        oldVersionInfo = m_repositories.value(repositoryPath);
    }
    
    // 总是插入/更新仓库信息，即使versionInfo为空
    // 这确保干净仓库的路径也会被记录，便于后续查询
    if (!m_repositories.contains(repositoryPath) || m_repositories.value(repositoryPath) != versionInfo) {
        m_repositories.insert(repositoryPath, versionInfo);
        qDebug() << "[GitStatusCache::resetVersion] Updated repository:" << repositoryPath
                 << "with" << versionInfo.size() << "version entries";
        
        // 查找变化的文件
        QHash<QString, ItemVersion> changedFiles;
        
        // 检查新增或修改的文件
        for (auto it = versionInfo.begin(); it != versionInfo.end(); ++it) {
            if (!oldVersionInfo.contains(it.key()) || oldVersionInfo.value(it.key()) != it.value()) {
                changedFiles.insert(it.key(), it.value());
            }
        }
        
        // 检查删除的文件（设为UnversionedVersion）
        for (auto it = oldVersionInfo.begin(); it != oldVersionInfo.end(); ++it) {
            if (!versionInfo.contains(it.key())) {
                changedFiles.insert(it.key(), UnversionedVersion);
            }
        }
        
        // 发出状态变更信号
        if (!changedFiles.isEmpty()) {
            Q_EMIT repositoryStatusChanged(repositoryPath, changedFiles);
        }
    }
}

void GitStatusCache::removeVersion(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    if (m_repositories.contains(repositoryPath)) {
        m_repositories.remove(repositoryPath);
        qDebug() << "[GitStatusCache::removeVersion] Removed repository:" << repositoryPath;
    }
}

ItemVersion GitStatusCache::version(const QString &filePath)
{
    Q_ASSERT(!filePath.isEmpty());
    QMutexLocker locker(&m_mutex);
    
    ItemVersion version = UnversionedVersion;
    
    // 查找文件所属的仓库
    QString repositoryPath = findRepositoryPath(filePath);
    if (repositoryPath.isEmpty()) {
        m_cacheMisses++;
        return version;
    }
    
    const auto &versionInfoHash = m_repositories.value(repositoryPath);
    if (versionInfoHash.contains(filePath)) {
        version = versionInfoHash.value(filePath);
        m_cacheHits++;
    } else {
        m_cacheMisses++;
    }
    
    return version;
}

QStringList GitStatusCache::allRepositoryPaths()
{
    QMutexLocker locker(&m_mutex);
    return m_repositories.keys();
}

QHash<QString, ItemVersion> GitStatusCache::getFileStatuses(const QStringList &filePaths)
{
    QMutexLocker locker(&m_mutex);
    QHash<QString, ItemVersion> result;
    
    for (const QString &filePath : filePaths) {
        QString repositoryPath = findRepositoryPath(filePath);
        if (!repositoryPath.isEmpty() && m_repositories.contains(repositoryPath)) {
            const auto &repoCache = m_repositories[repositoryPath];
            if (repoCache.contains(filePath)) {
                result[filePath] = repoCache[filePath];
                m_cacheHits++;
            } else {
                result[filePath] = UnversionedVersion;
                m_cacheMisses++;
            }
        } else {
            result[filePath] = UnversionedVersion;
            m_cacheMisses++;
        }
    }
    
    qDebug() << "[GitStatusCache::getFileStatuses] Processed" << filePaths.size() 
             << "files, returned" << result.size() << "results";
    
    return result;
}

QHash<QString, ItemVersion> GitStatusCache::getRepositoryStatus(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_repositories.contains(repositoryPath)) {
        m_cacheHits++;
        return m_repositories.value(repositoryPath);
    } else {
        m_cacheMisses++;
        return QHash<QString, ItemVersion>();
    }
}

bool GitStatusCache::registerRepository(const QString &repositoryPath)
{
    if (!isValidRepository(repositoryPath)) {
        qWarning() << "[GitStatusCache::registerRepository] Invalid repository path:" << repositoryPath;
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    if (m_repositories.size() >= MAX_REPOSITORIES) {
        qWarning() << "[GitStatusCache::registerRepository] Maximum repository limit reached:" << MAX_REPOSITORIES;
        return false;
    }
    
    if (!m_repositories.contains(repositoryPath)) {
        m_repositories.insert(repositoryPath, QHash<QString, ItemVersion>());
        qDebug() << "[GitStatusCache::registerRepository] Registered repository:" << repositoryPath;
        Q_EMIT repositoryDiscovered(repositoryPath);
    }
    
    return true;
}

bool GitStatusCache::unregisterRepository(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_repositories.contains(repositoryPath)) {
        m_repositories.remove(repositoryPath);
        qDebug() << "[GitStatusCache::unregisterRepository] Unregistered repository:" << repositoryPath;
        return true;
    }
    
    return false;
}

void GitStatusCache::clearCache()
{
    QMutexLocker locker(&m_mutex);
    int oldSize = m_repositories.size();
    m_repositories.clear();
    m_cacheHits = 0;
    m_cacheMisses = 0;
    qDebug() << "[GitStatusCache::clearCache] Cleared" << oldSize << "repositories from cache";
}

void GitStatusCache::clearRepositoryCache(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_repositories.contains(repositoryPath)) {
        m_repositories[repositoryPath].clear();
        qDebug() << "[GitStatusCache::clearRepositoryCache] Cleared cache for repository:" << repositoryPath;
    }
}

int GitStatusCache::getCacheSize() const
{
    QMutexLocker locker(&m_mutex);
    int totalFiles = 0;
    for (const auto &repo : m_repositories) {
        totalFiles += repo.size();
    }
    return totalFiles;
}

QStringList GitStatusCache::getCachedRepositories() const
{
    QMutexLocker locker(&m_mutex);
    return m_repositories.keys();
}

void GitStatusCache::onCleanupCache()
{
    QMutexLocker locker(&m_mutex);
    
    // 清理无效的仓库路径
    QStringList toRemove;
    for (auto it = m_repositories.begin(); it != m_repositories.end(); ++it) {
        if (!isValidRepository(it.key())) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString &repoPath : toRemove) {
        m_repositories.remove(repoPath);
    }
    
    if (!toRemove.isEmpty()) {
        qDebug() << "[GitStatusCache::onCleanupCache] Removed" << toRemove.size() << "invalid repositories";
    }
    
    // 输出性能统计
    qDebug() << "[GitStatusCache::onCleanupCache] Cache stats - Hits:" << m_cacheHits 
             << "Misses:" << m_cacheMisses << "Total files:" << getCacheSize();
}

QString GitStatusCache::findRepositoryPath(const QString &filePath) const
{
    const auto &allPaths = m_repositories.keys();
    auto it = std::find_if(allPaths.begin(), allPaths.end(), [&filePath](const QString &repositoryPath) {
        return filePath.startsWith(repositoryPath + '/') || filePath == repositoryPath;
    });
    
    return (it == allPaths.end()) ? QString() : *it;
}

void GitStatusCache::updateRepositoryBatch(const QString &repositoryPath, const QHash<QString, ItemVersion> &updates)
{
    // 此方法假设已经在锁保护下调用
    if (m_repositories.contains(repositoryPath)) {
        auto &repoCache = m_repositories[repositoryPath];
        for (auto it = updates.begin(); it != updates.end(); ++it) {
            repoCache.insert(it.key(), it.value());
        }
    }
}

bool GitStatusCache::isValidRepository(const QString &repositoryPath) const
{
    if (repositoryPath.isEmpty()) {
        return false;
    }
    
    QFileInfo fileInfo(repositoryPath);
    if (!fileInfo.exists() || !fileInfo.isDir()) {
        return false;
    }
    
    // 检查是否为Git仓库
    QDir repoDir(repositoryPath);
    return repoDir.exists(".git");
}

void GitStatusCache::performCleanup()
{
    QMutexLocker locker(&m_mutex);
    
    // 清理无效的仓库
    QStringList toRemove;
    for (auto it = m_repositories.begin(); it != m_repositories.end(); ++it) {
        if (!isValidRepository(it.key())) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString &repoPath : toRemove) {
        m_repositories.remove(repoPath);
    }
    
    qDebug() << "[GitStatusCache::performCleanup] Removed" << toRemove.size() << "invalid repositories";
}
