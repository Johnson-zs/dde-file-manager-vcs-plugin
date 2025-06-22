#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>

class GitDBusClient;

/**
 * @brief Git版本控制器
 * 
 * 重构为轻量级的DBus客户端触发器，负责触发daemon服务进行状态更新
 * 保留定时器和文件系统监控集成，实现新仓库发现和注册逻辑
 */
class GitVersionController : public QObject
{
    Q_OBJECT
    
public:
    explicit GitVersionController(QObject *parent = nullptr);
    ~GitVersionController();

Q_SIGNALS:
    /**
     * @brief 请求状态检索的信号
     * @param url 目录URL
     */
    void requestRetrieval(const QUrl &url);

public Q_SLOTS:
    /**
     * @brief 处理新仓库添加
     * @param repositoryPath 仓库路径
     */
    void onNewRepositoryAdded(const QString &repositoryPath);
    
    /**
     * @brief 处理仓库变更
     * @param repositoryPath 仓库路径
     */
    void onRepositoryChanged(const QString &repositoryPath);
    
    /**
     * @brief 处理服务请求的仓库更新
     * @param repositoryPath 仓库路径
     */
    void onRepositoryUpdateRequested(const QString &repositoryPath);

private Q_SLOTS:
    /**
     * @brief 定时器超时处理
     */
    void onTimeout();

private:
    GitDBusClient *m_dbusClient;
    QTimer *m_timer;
    bool m_useFileSystemWatcher;
}; 