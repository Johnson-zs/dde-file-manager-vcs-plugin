#pragma once

#include "git-types.h"
#include <dfm-extension/dfm-extension.h>
#include <mutex>
#include <QHash>
#include <QStringList>

/**
 * @brief Git角标插件重构版
 * 
 * 重构为轻量级的DBus客户端，使用本地缓存优化性能
 * 保留原有的performFirstTimeInitialization特殊逻辑
 */
class GitEmblemPlugin : public DFMEXT::DFMExtEmblemIconPlugin
{
public:
    GitEmblemPlugin();
    
    DFMEXT::DFMExtEmblem locationEmblemIcons(const std::string &filePath, int systemIconCount) const DFM_FAKE_OVERRIDE;
    
private:
    // 首次初始化逻辑 - 保留现有特殊处理
    static void performFirstTimeInitialization(const QString &filePath);
    static std::once_flag s_initOnceFlag;
    
    // 缓存管理结构
    struct CacheEntry {
        bool isRepository;
        qint64 timestamp;
        
        CacheEntry() : isRepository(false), timestamp(0) {}
        CacheEntry(bool repo, qint64 time) : isRepository(repo), timestamp(time) {}
    };
    
    // 静态缓存 - 保持与原版本兼容
    static QHash<QString, CacheEntry> s_pathCache;
    static QStringList s_cacheOrder;
    
    // 缓存配置
    static const int MAX_CACHE_SIZE = 1000;
    static const qint64 CACHE_EXPIRE_MS = 30000; // 30秒过期
    
    // 缓存管理方法
    bool isPathCached(const QString &path, bool &isRepository) const;
    void addToCache(const QString &path, bool isRepository) const;
    void cleanExpiredCache() const;
    void manageCacheSize() const;
    
    // 状态到图标的映射
    QString getIconNameForStatus(ItemVersion status) const;
    
    // 批量查询优化
    void requestBatchUpdate(const QString &filePath) const;
};
