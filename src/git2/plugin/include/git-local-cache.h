#pragma once

#include "git-types.h"
#include <QObject>
#include <QMutex>
#include <QHash>
#include <QTimer>
#include <QVariantMap>

/**
 * @brief Git状态本地缓存类
 * 
 * 为插件进程提供高性能的本地缓存，减少DBus调用频次
 * 使用100ms TTL缓存策略，支持批量更新和信号驱动刷新
 */
class GitLocalCache : public QObject
{
    Q_OBJECT
    
public:
    static GitLocalCache& instance();
    
    // 缓存查询
    ItemVersion getFileStatus(const QString &filePath);
    bool isCacheValid(const QString &filePath) const;
    bool isInsideRepository(const QString &filePath) const;
    
    // 批量更新
    void updateCache(const QHash<QString, ItemVersion> &statusMap);
    void updateCacheFromVariantMap(const QVariantMap &statusMap);
    
    // 缓存管理
    void clearExpiredCache();
    void clearRepositoryCache(const QString &repositoryPath);
    void clearAllCache();
    
    // 统计信息
    int getCacheSize() const;
    int getCacheHits() const { return m_cacheHits; }
    int getCacheMisses() const { return m_cacheMisses; }
    
public Q_SLOTS:
    void onRepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changes);
    
private:
    explicit GitLocalCache(QObject *parent = nullptr);
    ~GitLocalCache();
    
    struct CacheEntry {
        ItemVersion status;
        qint64 timestamp;
        QString repositoryPath;
        
        CacheEntry() : status(UnversionedVersion), timestamp(0) {}
        CacheEntry(ItemVersion s, qint64 t, const QString &repo = QString()) 
            : status(s), timestamp(t), repositoryPath(repo) {}
    };
    
    mutable QMutex m_mutex;
    QHash<QString, CacheEntry> m_cache;
    QTimer *m_cleanupTimer;
    
    // 统计信息
    mutable int m_cacheHits;
    mutable int m_cacheMisses;
    
    // 配置常量
    static const qint64 CACHE_TTL_MS = 100;  // 100ms TTL
    static const int MAX_CACHE_SIZE = 10000;
    static const int CLEANUP_INTERVAL_MS = 5000; // 5秒清理一次
    
    // 辅助方法
    QString findRepositoryPath(const QString &filePath) const;
    void requestBatchUpdate(const QString &directoryPath);
    bool isExpired(const CacheEntry &entry) const;
    
private Q_SLOTS:
    void onCleanupTimer();
};
