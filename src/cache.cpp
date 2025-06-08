#include "cache.h"

#include <QDebug>

namespace Global {

Cache &Cache::instance()
{
    static Cache ins;
    return ins;
}

void Cache::resetVersion(const QString &repositoryPath, QHash<QString, ItemVersion> versionInfo)
{
    QMutexLocker locker { &m_mutex };
    // 总是插入/更新仓库信息，即使versionInfo为空
    // 这确保干净仓库的路径也会被记录，便于后续查询
    if (!m_repositories.contains(repositoryPath) || m_repositories.value(repositoryPath) != versionInfo) {
        m_repositories.insert(repositoryPath, versionInfo);
        qDebug() << "[Cache::resetVersion] Updated repository:" << repositoryPath
                 << "with" << versionInfo.size() << "version entries";
    }
}

void Cache::removeVersion(const QString &repositoryPath)
{
    QMutexLocker locker { &m_mutex };
    if (m_repositories.contains(repositoryPath))
        m_repositories.remove(repositoryPath);
}

ItemVersion Cache::version(const QString &filePath)
{
    Q_ASSERT(!filePath.isEmpty());
    ItemVersion version { ItemVersion::NormalVersion };
    QMutexLocker locker { &m_mutex };
    const auto &allPaths { m_repositories.keys() };
    auto it = std::find_if(allPaths.begin(), allPaths.end(), [&filePath](const QString &repositoryPath) {
        return filePath.startsWith(repositoryPath + '/') || filePath == repositoryPath;
    });
    if (it == allPaths.end())
        return version;

    const auto &versionInfoHash { m_repositories.value(*it) };
    if (versionInfoHash.contains(filePath))
        version = versionInfoHash.value(filePath);

    return version;
}

QStringList Cache::allRepositoryPaths()
{
    QMutexLocker locker { &m_mutex };
    return m_repositories.keys();
}

Cache::Cache(QObject *parent)
    : QObject { parent }
{
}

}   // namespace Global
