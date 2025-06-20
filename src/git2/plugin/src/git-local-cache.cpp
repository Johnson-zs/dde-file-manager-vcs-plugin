#include "git-local-cache.h"
#include "git-dbus-client.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

GitLocalCache& GitLocalCache::instance()
{
    static GitLocalCache ins;
    return ins;
}

GitLocalCache::GitLocalCache(QObject *parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
    , m_cacheHits(0)
    , m_cacheMisses(0)
{
    // 设置清理定时器
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL_MS);
    m_cleanupTimer->setSingleShot(false);
    connect(m_cleanupTimer, &QTimer::timeout, this, &GitLocalCache::onCleanupTimer);
    m_cleanupTimer->start();
    
    qDebug() << "[GitLocalCache] Local cache initialized with TTL" << CACHE_TTL_MS << "ms";
}

GitLocalCache::~GitLocalCache()
{
    qDebug() << "[GitLocalCache] Destroyed - Cache hits:" << m_cacheHits 
             << "misses:" << m_cacheMisses;
}

ItemVersion GitLocalCache::getFileStatus(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    
    // 检查缓存有效性
    if (isCacheValid(filePath)) {
        m_cacheHits++;
        return m_cache[filePath].status;
    }
    
    // 缓存未命中或过期，请求批量更新
    QFileInfo fileInfo(filePath);
    const QString dirPath = fileInfo.absolutePath();
    
    // 释放锁后请求更新，避免死锁
    locker.unlock();
    requestBatchUpdate(dirPath);
    locker.relock();
    
    // 再次检查缓存（可能在批量更新中已经填充）
    if (m_cache.contains(filePath) && !isExpired(m_cache[filePath])) {
        m_cacheHits++;
        return m_cache[filePath].status;
    }
    
    m_cacheMisses++;
    return UnversionedVersion;
}

bool GitLocalCache::isCacheValid(const QString &filePath) const
{
    if (!m_cache.contains(filePath)) {
        return false;
    }
    
    const CacheEntry &entry = m_cache[filePath];
    return !isExpired(entry);
}

bool GitLocalCache::isInsideRepository(const QString &filePath) const
{
    QMutexLocker locker(&m_mutex);
    
    // 检查缓存中是否有此文件的仓库信息
    if (m_cache.contains(filePath) && !isExpired(m_cache[filePath])) {
        return !m_cache[filePath].repositoryPath.isEmpty();
    }
    
    // 检查是否有任何父目录在缓存中
    QString checkPath = filePath;
    while (!checkPath.isEmpty() && checkPath != "/") {
        if (m_cache.contains(checkPath) && !isExpired(m_cache[checkPath])) {
            return !m_cache[checkPath].repositoryPath.isEmpty();
        }
        
        QFileInfo fileInfo(checkPath);
        checkPath = fileInfo.absolutePath();
        if (checkPath == fileInfo.absoluteFilePath()) {
            break; // 避免无限循环
        }
    }
    
    return false;
}

void GitLocalCache::updateCache(const QHash<QString, ItemVersion> &statusMap)
{
    QMutexLocker locker(&m_mutex);
    
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    for (auto it = statusMap.begin(); it != statusMap.end(); ++it) {
        const QString &filePath = it.key();
        ItemVersion status = it.value();
        
        QString repoPath = findRepositoryPath(filePath);
        m_cache.insert(filePath, CacheEntry(status, currentTime, repoPath));
    }
    
    qDebug() << "[GitLocalCache::updateCache] Updated" << statusMap.size() << "entries";
    
    // 检查缓存大小限制
    if (m_cache.size() > MAX_CACHE_SIZE) {
        clearExpiredCache();
    }
}

void GitLocalCache::updateCacheFromVariantMap(const QVariantMap &statusMap)
{
    QHash<QString, ItemVersion> convertedMap;
    
    for (auto it = statusMap.begin(); it != statusMap.end(); ++it) {
        ItemVersion status = static_cast<ItemVersion>(it.value().toInt());
        convertedMap.insert(it.key(), status);
    }
    
    updateCache(convertedMap);
}

void GitLocalCache::clearExpiredCache()
{
    QMutexLocker locker(&m_mutex);
    
    QStringList toRemove;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (isExpired(it.value())) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString &key : toRemove) {
        m_cache.remove(key);
    }
    
    if (!toRemove.isEmpty()) {
        qDebug() << "[GitLocalCache::clearExpiredCache] Removed" << toRemove.size() << "expired entries";
    }
}

void GitLocalCache::clearRepositoryCache(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    
    QStringList toRemove;
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it.value().repositoryPath == repositoryPath) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString &key : toRemove) {
        m_cache.remove(key);
    }
    
    qDebug() << "[GitLocalCache::clearRepositoryCache] Cleared" << toRemove.size() 
             << "entries for repository:" << repositoryPath;
}

void GitLocalCache::clearAllCache()
{
    QMutexLocker locker(&m_mutex);
    int oldSize = m_cache.size();
    m_cache.clear();
    m_cacheHits = 0;
    m_cacheMisses = 0;
    
    qDebug() << "[GitLocalCache::clearAllCache] Cleared" << oldSize << "entries";
}

int GitLocalCache::getCacheSize() const
{
    QMutexLocker locker(&m_mutex);
    return m_cache.size();
}

void GitLocalCache::onRepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changes)
{
    qDebug() << "[GitLocalCache::onRepositoryStatusChanged] Repository:" << repositoryPath 
             << "changed files:" << changes.size();
    
    // 更新变更的文件状态
    updateCacheFromVariantMap(changes);
}

QString GitLocalCache::findRepositoryPath(const QString &filePath) const
{
    // 从缓存中查找文件所属的仓库路径
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        const QString &repoPath = it.value().repositoryPath;
        if (!repoPath.isEmpty() && 
            (filePath.startsWith(repoPath + '/') || filePath == repoPath)) {
            return repoPath;
        }
    }
    
    return QString();
}

void GitLocalCache::requestBatchUpdate(const QString &directoryPath)
{
    // 请求DBus客户端进行批量更新
    GitDBusClient::instance().requestDirectoryUpdate(directoryPath);
}

bool GitLocalCache::isExpired(const CacheEntry &entry) const
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    return (currentTime - entry.timestamp) > CACHE_TTL_MS;
}

void GitLocalCache::onCleanupTimer()
{
    clearExpiredCache();
    
    // 输出统计信息
    qDebug() << "[GitLocalCache::onCleanupTimer] Cache stats - Size:" << getCacheSize()
             << "Hits:" << m_cacheHits << "Misses:" << m_cacheMisses;
}
