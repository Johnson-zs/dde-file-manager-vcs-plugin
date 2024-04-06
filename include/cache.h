#ifndef CACHE_H
#define CACHE_H

#include <QMutexLocker>

#include <global.h>

namespace Global {
class Cache : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Cache)

public:
    static Cache &instance();

    void resetVersion(const QString &repositoryPath, QHash<QString, ItemVersion> versionInfo);
    void removeVersion(const QString &repositoryPath);
    ItemVersion version(const QString &filePath);
    QStringList allRepositoryPaths();

private:
    explicit Cache(QObject *parent = nullptr);

private:
    QMutex m_mutex;   // 一把大锁保平安
    // repository path -> { file path, version }
    QHash<QString, QHash<QString, ItemVersion>> m_repositories;
};

}   // namespace Global

#endif   // CACHE_H
