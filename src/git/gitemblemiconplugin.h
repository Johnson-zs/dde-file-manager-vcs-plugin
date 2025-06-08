#ifndef GITEMBLEMICONPLUGIN_H
#define GITEMBLEMICONPLUGIN_H

#include <dfm-extension/emblemicon/dfmextemblemiconplugin.h>
#include <QHash>
#include <QStringList>
#include <QString>

class GitEmblemIconPlugin : public DFMEXT::DFMExtEmblemIconPlugin
{
public:
    GitEmblemIconPlugin();
    DFMEXT::DFMExtEmblem locationEmblemIcons(const std::string &filePath, int systemIconCount) const DFM_FAKE_OVERRIDE;

private:
    // LRU缓存条目结构
    struct CacheEntry {
        bool isRepository;
        qint64 timestamp;
    };
    
    // 本地仓库发现缓存 (LRU + 过期机制)
    static QHash<QString, CacheEntry> s_pathCache;
    static QStringList s_cacheOrder; // LRU 顺序
    static const int MAX_CACHE_SIZE = 1000;
    static const qint64 CACHE_EXPIRE_MS = 60000; // 1分钟过期
    
    // 缓存管理方法
    bool isPathCached(const QString &path, bool &isRepository) const;
    void addToCache(const QString &path, bool isRepository) const;
    void cleanExpiredCache() const;
    void manageCacheSize() const;
};

#endif   // GITEMBLEMICONPLUGIN_H
