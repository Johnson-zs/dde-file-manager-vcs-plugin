#ifndef GITREPOSITORYSERVICE_H
#define GITREPOSITORYSERVICE_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QMutex>

#include "gitserviceinterface.h"

/**
 * @brief Git仓库服务实现
 * 
 * 单例模式的服务类，负责：
 * - 管理已发现仓库的全局状态
 * - 协调仓库发现与状态更新
 * - 提供线程安全的仓库管理接口
 */
class GitRepositoryService : public QObject, public GitServiceInterface
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return 服务实例引用
     */
    static GitRepositoryService& instance();
    
    // GitServiceInterface implementation
    void requestRepositoryUpdate(const QString &repositoryPath) override;
    void registerRepositoryDiscovered(const QString &repositoryPath) override;
    bool isRepositoryTracked(const QString &repositoryPath) const override;

private:
    // 内部方法，不加锁版本
    void requestRepositoryUpdateInternal(const QString &repositoryPath);

signals:
    /**
     * @brief 仓库更新请求信号
     * @param repositoryPath 需要更新的仓库路径
     */
    void repositoryUpdateRequested(const QString &repositoryPath);

private:
    GitRepositoryService() = default;
    ~GitRepositoryService() override = default;
    
    // 禁用拷贝和赋值
    GitRepositoryService(const GitRepositoryService&) = delete;
    GitRepositoryService& operator=(const GitRepositoryService&) = delete;

private:
    mutable QMutex m_mutex;                    // 线程安全保护
    QSet<QString> m_trackedRepositories;       // 已跟踪的仓库路径
    QSet<QString> m_pendingRepositories;       // 等待更新的仓库路径
};

#endif // GITREPOSITORYSERVICE_H 