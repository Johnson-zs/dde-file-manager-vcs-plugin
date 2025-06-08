#include "gitemblemiconplugin.h"

#include <filesystem>

#include <QString>
#include <QHash>
#include <QStringList>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>

#include <cache.h>
#include "utils.h"
#include "common/gitrepositoryservice.h"

USING_DFMEXT_NAMESPACE

// 静态成员变量定义
QHash<QString, GitEmblemIconPlugin::CacheEntry> GitEmblemIconPlugin::s_pathCache;
QStringList GitEmblemIconPlugin::s_cacheOrder;
std::once_flag GitEmblemIconPlugin::s_initOnceFlag;

GitEmblemIconPlugin::GitEmblemIconPlugin()
    : DFMEXT::DFMExtEmblemIconPlugin()
{
    registerLocationEmblemIcons([this](const std::string &filePath, int systemIconCount) {
        return locationEmblemIcons(filePath, systemIconCount);
    });
}

// 首次初始化函数
void GitEmblemIconPlugin::performFirstTimeInitialization(const QString &filePath)
{
    // 获取文件所在目录
    QFileInfo fileInfo(filePath);
    const QString dirPath = fileInfo.absolutePath();

    qDebug() << "[GitEmblemIconPlugin] First-time initialization with directory:" << dirPath;

    // 注册目录到仓库服务进行发现
    GitRepositoryService::instance().registerRepositoryDiscovered(dirPath);
}

// work in thread
DFMExtEmblem GitEmblemIconPlugin::locationEmblemIcons(const std::string &filePath, int systemIconCount) const
{
    Q_UNUSED(systemIconCount)

    const QString &path { QString::fromStdString(filePath) };

    // 首次调用时执行初始化
    std::call_once(s_initOnceFlag, [&path]() {
        performFirstTimeInitialization(path);
    });

    using Global::ItemVersion;
    DFMExtEmblem emblem;

    // 检查是否在已知仓库中（包括仓库根目录本身）
    bool isInRepository = Utils::isInsideRepositoryFile(path);
    bool isRepositoryRoot = false;

    if (!isInRepository) {
        // 检查本地缓存
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
            if (Utils::isGitRepositoryRoot(path)) {
                // 发现新仓库，添加到缓存
                addToCache(path, true);
                qDebug() << "[GitEmblemIconPlugin] Discovered new repository:" << path;

                // 通过服务注册新发现的仓库并触发异步更新
                GitRepositoryService::instance().registerRepositoryDiscovered(path);

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

    auto state { Global::Cache::instance().version(path) };
    std::vector<DFMExtEmblemIconLayout> layouts;
    QString iconName;

    switch (state) {
    case ItemVersion::NormalVersion:
        iconName = QStringLiteral("vcs-normal");
        break;
    case ItemVersion::UpdateRequiredVersion:
        iconName = QStringLiteral("vcs-update-required");
        break;
    case ItemVersion::LocallyModifiedVersion:
        iconName = QStringLiteral("vcs-locally-modified");
        break;
    case ItemVersion::LocallyModifiedUnstagedVersion:
        iconName = QStringLiteral("vcs-locally-modified-unstaged");
        break;
    case ItemVersion::AddedVersion:
        iconName = QStringLiteral("vcs-added");
        break;
    case ItemVersion::RemovedVersion:
        iconName = QStringLiteral("vcs-removed");
        break;
    case ItemVersion::ConflictingVersion:
        iconName = QStringLiteral("vcs-conflicting");
        break;
    case ItemVersion::UnversionedVersion:
    case ItemVersion::IgnoredVersion:
    case ItemVersion::MissingVersion:
        break;
    default:
        Q_ASSERT(false);
        break;
    }

    // 检查目录是否在Git意义上为空（只包含空子目录）
    // 这样可以正确处理包含空子目录的情况，Git不会跟踪纯空目录
    if (Utils::isDirectoryEmpty(path)) {
        iconName.clear();
        // qDebug() << "[GitEmblemIconPlugin] Directory is Git-empty, clearing icon:" << path;
    }

    DFMExtEmblemIconLayout iconLayout { DFMExtEmblemIconLayout::LocationType::BottomLeft, iconName.toStdString() };
    layouts.push_back(iconLayout);
    emblem.setEmblem(layouts);

    return emblem;
}

// 缓存管理方法实现
bool GitEmblemIconPlugin::isPathCached(const QString &path, bool &isRepository) const
{
    cleanExpiredCache();

    auto it = s_pathCache.find(path);
    if (it != s_pathCache.end()) {
        isRepository = it.value().isRepository;

        // 更新 LRU 顺序
        s_cacheOrder.removeOne(path);
        s_cacheOrder.append(path);

        return true;
    }
    return false;
}

void GitEmblemIconPlugin::addToCache(const QString &path, bool isRepository) const
{
    CacheEntry entry;
    entry.isRepository = isRepository;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();

    s_pathCache[path] = entry;
    s_cacheOrder.removeOne(path);   // 移除可能的重复项
    s_cacheOrder.append(path);

    manageCacheSize();
}

void GitEmblemIconPlugin::cleanExpiredCache() const
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

void GitEmblemIconPlugin::manageCacheSize() const
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
