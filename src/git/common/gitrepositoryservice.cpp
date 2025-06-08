#include "gitrepositoryservice.h"

#include <QDebug>
#include <QMutexLocker>

GitRepositoryService& GitRepositoryService::instance()
{
    static GitRepositoryService service;
    return service;
}

void GitRepositoryService::requestRepositoryUpdate(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    requestRepositoryUpdateInternal(repositoryPath);
}

void GitRepositoryService::requestRepositoryUpdateInternal(const QString &repositoryPath)
{
    // 注意：此方法假设调用者已经获得了锁
    if (!repositoryPath.isEmpty()) {
        qDebug() << "[GitRepositoryService] Repository update requested:" << repositoryPath;
        
        // 添加到待更新列表
        m_pendingRepositories.insert(repositoryPath);
        
        // 发射信号通知监听者
        emit repositoryUpdateRequested(repositoryPath);
    }
}

void GitRepositoryService::registerRepositoryDiscovered(const QString &repositoryPath)
{
    QMutexLocker locker(&m_mutex);
    
    if (!repositoryPath.isEmpty() && !m_trackedRepositories.contains(repositoryPath)) {
        qDebug() << "[GitRepositoryService] New repository registered:" << repositoryPath;
        
        m_trackedRepositories.insert(repositoryPath);
        
        // 自动请求更新新发现的仓库（使用内部方法避免死锁）
        requestRepositoryUpdateInternal(repositoryPath);
    }
}

bool GitRepositoryService::isRepositoryTracked(const QString &repositoryPath) const
{
    QMutexLocker locker(&m_mutex);
    return m_trackedRepositories.contains(repositoryPath);
} 