#pragma once

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QHash>
#include <QStringList>
#include <QTimer>
#include "git-types.h"

/**
 * @brief Git状态缓存管理器
 * 
 * 迁移自Global::Cache，提供高性能的Git文件状态缓存服务
 * 核心功能：
 * 1. 线程安全的文件状态缓存
 * 2. 批量查询优化
 * 3. 内存管理和LRU清理
 * 4. 状态变更通知
 */
class GitStatusCache : public QObject
{
    Q_OBJECT

public:
    static GitStatusCache& instance();

    // 核心方法 - 迁移自 Global::Cache
    void resetVersion(const QString &repositoryPath, const QHash<QString, ItemVersion> &versionInfo);
    void removeVersion(const QString &repositoryPath);
    ItemVersion version(const QString &filePath);
    QStringList allRepositoryPaths();

    // 新增批量查询方法
    QHash<QString, ItemVersion> getFileStatuses(const QStringList &filePaths);
    QHash<QString, ItemVersion> getRepositoryStatus(const QString &repositoryPath);
    
    // 仓库管理
    bool registerRepository(const QString &repositoryPath);
    bool unregisterRepository(const QString &repositoryPath);
    
    // 缓存管理
    void clearCache();
    void clearRepositoryCache(const QString &repositoryPath);
    void performCleanup();
    
    // 状态信息
    int getCacheSize() const;
    QStringList getCachedRepositories() const;

Q_SIGNALS:
    /**
     * @brief 仓库状态发生变化时发出的信号
     * @param repositoryPath 仓库路径
     * @param changedFiles 变化的文件和其状态
     */
    void repositoryStatusChanged(const QString &repositoryPath, const QHash<QString, ItemVersion> &changedFiles);
    
    /**
     * @brief 发现新仓库时发出的信号
     * @param repositoryPath 仓库路径
     */
    void repositoryDiscovered(const QString &repositoryPath);

private Q_SLOTS:
    /**
     * @brief 定期清理过期缓存
     */
    void onCleanupCache();

private:
    explicit GitStatusCache(QObject *parent = nullptr);
    
    // 内部工具方法
    QString findRepositoryPath(const QString &filePath) const;
    void updateRepositoryBatch(const QString &repositoryPath, const QHash<QString, ItemVersion> &updates);
    bool isValidRepository(const QString &repositoryPath) const;
    
private:
    mutable QMutex m_mutex;  ///< 线程安全锁
    QHash<QString, QHash<QString, ItemVersion>> m_repositories;  ///< 仓库缓存数据
    QTimer *m_cleanupTimer;  ///< 清理定时器
    
    // 性能统计
    mutable int m_cacheHits;
    mutable int m_cacheMisses;
    
    // 配置常量
    static constexpr int CLEANUP_INTERVAL_MS = 300000;  // 5分钟清理一次
    static constexpr int MAX_REPOSITORIES = 100;        // 最大仓库数量
};
