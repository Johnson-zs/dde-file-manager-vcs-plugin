#include "git-emblem-plugin.h"
#include "git-local-cache.h"
#include "git-dbus-client.h"
#include "git-utils.h"
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <mutex>

// 静态成员变量定义
QHash<QString, GitEmblemPlugin::CacheEntry> GitEmblemPlugin::s_pathCache;
QStringList GitEmblemPlugin::s_cacheOrder;
std::once_flag GitEmblemPlugin::s_initOnceFlag;

GitEmblemPlugin::GitEmblemPlugin()
    : DFMEXT::DFMExtEmblemIconPlugin()
{
    registerLocationEmblemIcons([this](const std::string &filePath, int systemIconCount) {
        return locationEmblemIcons(filePath, systemIconCount);
    });
    
    // 连接DBus客户端信号到本地缓存
    QObject::connect(&GitDBusClient::instance(), &GitDBusClient::repositoryStatusChanged,
            &GitLocalCache::instance(), &GitLocalCache::onRepositoryStatusChanged);
    
    QObject::connect(&GitDBusClient::instance(), &GitDBusClient::fileStatusesReady,
            [](const QHash<QString, ItemVersion> &statuses) {
                GitLocalCache::instance().updateCache(statuses);
            });
    
    qDebug() << "[GitEmblemPlugin] Plugin initialized with DBus client integration";
}

void GitEmblemPlugin::performFirstTimeInitialization(const QString &filePath)
{
    // 保留原始逻辑：获取文件所在目录并注册到服务
    QFileInfo fileInfo(filePath);
    const QString dirPath = fileInfo.absolutePath();
    
    qDebug() << "[GitEmblemPlugin] First-time initialization with directory:" << dirPath;
    
    // 通过DBus通知服务进行仓库发现
    GitDBusClient::instance().registerRepository(dirPath);
}

DFMEXT::DFMExtEmblem GitEmblemPlugin::locationEmblemIcons(const std::string &filePath, int systemIconCount) const
{
    Q_UNUSED(systemIconCount)
    
    const QString path = QString::fromStdString(filePath);
    
    // 首次调用时执行初始化
    std::call_once(s_initOnceFlag, [&path]() {
        performFirstTimeInitialization(path);
    });
    
    DFMEXT::DFMExtEmblem emblem;
    
    // 检查是否在已知仓库中
    bool isInRepository = GitLocalCache::instance().isInsideRepository(path);
    bool isRepositoryRoot = false;
    
    if (!isInRepository) {
        // 检查本地路径缓存
        bool isRepository = false;
        if (isPathCached(path, isRepository)) {
            if (!isRepository) {
                // 已确认不是仓库，直接返回空
                return emblem;
            }
            // 如果是仓库根目录，标记为true并继续处理
            isRepositoryRoot = true;
        } else {
            // 执行轻量级仓库检测
            if (GitUtils::isGitRepositoryRoot(path)) {
                // 发现新仓库，添加到缓存
                addToCache(path, true);
                qDebug() << "[GitEmblemPlugin] Discovered new repository:" << path;
                
                // 通过DBus注册新发现的仓库
                GitDBusClient::instance().registerRepository(path);
                
                // 请求批量更新
                requestBatchUpdate(path);
                
                // 当前返回空，等下次刷新时显示真实状态
                return emblem;
            } else {
                // 确认不是仓库，添加到缓存
                addToCache(path, false);
                return emblem;
            }
        }
    }
    
    // 如果既不在仓库内也不是仓库根目录，返回空
    if (!isInRepository && !isRepositoryRoot) {
        return emblem;
    }
    
    // 从本地缓存获取文件状态
    ItemVersion status = GitLocalCache::instance().getFileStatus(path);
    
    // 如果本地缓存未命中，尝试请求批量更新
    if (status == UnversionedVersion && isInRepository) {
        requestBatchUpdate(path);
    }
    
    // 获取图标名称
    QString iconName = getIconNameForStatus(status);
    
    // 检查目录是否在Git意义上为空
    if (GitUtils::isDirectoryEmpty(path)) {
        iconName.clear();
    }
    
    if (!iconName.isEmpty()) {
        std::vector<DFMEXT::DFMExtEmblemIconLayout> layouts;
        DFMEXT::DFMExtEmblemIconLayout iconLayout{
            DFMEXT::DFMExtEmblemIconLayout::LocationType::BottomLeft, 
            iconName.toStdString()
        };
        layouts.push_back(iconLayout);
        emblem.setEmblem(layouts);
    }
    
    return emblem;
}

QString GitEmblemPlugin::getIconNameForStatus(ItemVersion status) const
{
    switch (status) {
    case NormalVersion:
        return QStringLiteral(""); // 无角标
    case LocallyModifiedVersion:
        return QStringLiteral("vcs-locally-modified");
    case LocallyModifiedUnstagedVersion:
        return QStringLiteral("vcs-locally-modified-unstaged");
    case AddedVersion:
        return QStringLiteral("vcs-added");
    case RemovedVersion:
        return QStringLiteral("vcs-removed");
    case ConflictingVersion:
        return QStringLiteral("vcs-conflicting");
    case UpdateRequiredVersion:
        return QStringLiteral("vcs-update-required");
    case MissingVersion:
        return QStringLiteral("vcs-missing");
    case UnversionedVersion:
    case IgnoredVersion:
    default:
        return QString();
    }
}

void GitEmblemPlugin::requestBatchUpdate(const QString &filePath) const
{
    // 请求目录级别的批量更新
    QFileInfo fileInfo(filePath);
    const QString dirPath = fileInfo.isDir() ? filePath : fileInfo.absolutePath();
    
    GitDBusClient::instance().requestDirectoryUpdate(dirPath);
}

bool GitEmblemPlugin::isPathCached(const QString &path, bool &isRepository) const
{
    cleanExpiredCache();
    
    auto it = s_pathCache.find(path);
    if (it != s_pathCache.end()) {
        isRepository = it.value().isRepository;
        
        // 更新LRU顺序
        s_cacheOrder.removeOne(path);
        s_cacheOrder.append(path);
        
        return true;
    }
    return false;
}

void GitEmblemPlugin::addToCache(const QString &path, bool isRepository) const
{
    CacheEntry entry(isRepository, QDateTime::currentMSecsSinceEpoch());
    
    s_pathCache[path] = entry;
    s_cacheOrder.removeOne(path);   // 移除可能的重复项
    s_cacheOrder.append(path);
    
    manageCacheSize();
}

void GitEmblemPlugin::cleanExpiredCache() const
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = s_pathCache.begin();
    
    while (it != s_pathCache.end()) {
        if (now - it.value().timestamp > CACHE_EXPIRE_MS) {
            QString path = it.key();
            it = s_pathCache.erase(it);
            s_cacheOrder.removeOne(path);
        } else {
            ++it;
        }
    }
}

void GitEmblemPlugin::manageCacheSize() const
{
    while (s_pathCache.size() > MAX_CACHE_SIZE) {
        if (!s_cacheOrder.isEmpty()) {
            QString oldestPath = s_cacheOrder.takeFirst();
            s_pathCache.remove(oldestPath);
        } else {
            break;
        }
    }
}
