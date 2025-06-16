#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class GitService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.FileManager.Git")

public:
    explicit GitService(QObject *parent = nullptr);

public Q_SLOTS:
    // 仓库管理接口
    bool RegisterRepository(const QString &repositoryPath);
    bool UnregisterRepository(const QString &repositoryPath);
    
    // 状态查询接口 (批量优化)
    QVariantMap GetFileStatuses(const QStringList &filePaths);
    QVariantMap GetRepositoryStatus(const QString &repositoryPath);
    
    // 操作触发接口
    bool RefreshRepository(const QString &repositoryPath);
    bool ClearRepositoryCache(const QString &repositoryPath);

Q_SIGNALS:
    // 状态变更信号
    void RepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changedFiles);
    void RepositoryDiscovered(const QString &repositoryPath);

private:
    // 内部实现将在cpp文件中提供
}; 