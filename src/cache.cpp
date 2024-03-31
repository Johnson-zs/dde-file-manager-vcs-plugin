#include "cache.h"

namespace Global {

Cache &Cache::instance()
{
    static Cache ins;
    return ins;
}

void Cache::addVersion(const QString &repositoryPath, QHash<QString, ItemVersion> versionInfo)
{
    QMutexLocker locker { &m_mutex };
    m_repositories.insert(repositoryPath, versionInfo);
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
        return filePath.startsWith(repositoryPath + "/");
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
