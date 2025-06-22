#pragma once

#include "git-types.h"
#include <QObject>
#include <QDBusInterface>
#include <QHash>
#include <QVariantMap>
#include <QTimer>

/**
 * @brief Git DBus客户端类
 * 
 * 封装对Git daemon服务的DBus调用，提供同步和异步接口
 * 处理DBus连接异常和服务不可用情况
 */
class GitDBusClient : public QObject
{
    Q_OBJECT
    
public:
    static GitDBusClient& instance();
    
    // 服务连接管理
    bool isServiceAvailable() const;
    bool connectToService();
    void disconnectFromService();
    
    // 仓库管理接口
    bool registerRepository(const QString &repositoryPath);
    bool unregisterRepository(const QString &repositoryPath);
    
    // 状态查询接口
    QHash<QString, ItemVersion> getFileStatuses(const QStringList &filePaths);
    QHash<QString, ItemVersion> getRepositoryStatus(const QString &repositoryPath);
    ItemVersion getFileStatus(const QString &filePath);
    
    // 操作触发接口
    bool refreshRepository(const QString &repositoryPath);
    bool clearRepositoryCache(const QString &repositoryPath);
    bool clearAllResources();
    bool triggerRetrieval(const QString &directoryPath);
    
    // 服务状态查询
    QVariantMap getServiceStatus();
    QStringList getRegisteredRepositories();
    
    // 批量更新请求
    void requestDirectoryUpdate(const QString &directoryPath);
    
    // 异步接口
    void getFileStatusesAsync(const QStringList &filePaths);
    void getRepositoryStatusAsync(const QString &repositoryPath);
    
Q_SIGNALS:
    void repositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changes);
    void repositoryDiscovered(const QString &repositoryPath);
    void serviceAvailabilityChanged(bool available);
    void fileStatusesReady(const QHash<QString, ItemVersion> &statuses);
    void repositoryStatusReady(const QString &repositoryPath, const QHash<QString, ItemVersion> &statuses);
    
private Q_SLOTS:
    void onRepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changes);
    void onRepositoryDiscovered(const QString &repositoryPath);
    void onServiceOwnerChanged(const QString &serviceName, const QString &oldOwner, const QString &newOwner);
    void onConnectionCheck();
    
private:
    explicit GitDBusClient(QObject *parent = nullptr);
    ~GitDBusClient();
    
    QDBusInterface *m_interface;
    QTimer *m_connectionTimer;
    bool m_serviceAvailable;
    bool m_signalsConnected;
    
    // 配置常量
    static const QString SERVICE_NAME;
    static const QString OBJECT_PATH;
    static const QString INTERFACE_NAME;
    static const int CONNECTION_CHECK_INTERVAL = 5000; // 5秒检查一次
    
    // 辅助方法
    void setupSignalConnections();
    void cleanupSignalConnections();
    QHash<QString, ItemVersion> convertFromVariantMap(const QVariantMap &variantMap);
    QVariantMap convertToVariantMap(const QHash<QString, ItemVersion> &statusMap);
    void handleDBusError(const QString &method, const QDBusError &error);
}; 