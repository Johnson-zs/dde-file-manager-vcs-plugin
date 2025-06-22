#pragma once

#include <QObject>
#include <QHash>
#include <QStringList>
#include <QVariantMap>
#include "git-types.h"

/**
 * @brief Git DBus服务接口类
 * 
 * 此类用于Qt6的qt_generate_dbus_interface和qt_add_dbus_adaptor自动生成DBus适配器
 * 提供Git状态查询和仓库管理的DBus接口
 */
class GitService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.FileManager.Git")

public:
    explicit GitService(QObject *parent = nullptr);
    ~GitService();

public Q_SLOTS:
    /**
     * @brief 注册Git仓库到监控列表
     * @param repositoryPath Git仓库根目录路径
     * @return 注册是否成功
     */
    bool RegisterRepository(const QString &repositoryPath);

    /**
     * @brief 从监控列表中注销Git仓库
     * @param repositoryPath Git仓库根目录路径
     * @return 注销是否成功
     */
    bool UnregisterRepository(const QString &repositoryPath);

    /**
     * @brief 批量获取文件的Git状态
     * @param filePaths 文件路径列表
     * @return 文件路径到状态码的映射 (QVariantMap for DBus compatibility)
     */
    QVariantMap GetFileStatuses(const QStringList &filePaths);

    /**
     * @brief 获取整个仓库的文件状态
     * @param repositoryPath 仓库路径
     * @return 文件路径到状态码的映射
     */
    QVariantMap GetRepositoryStatus(const QString &repositoryPath);

    /**
     * @brief 刷新指定仓库的状态缓存
     * @param repositoryPath 仓库路径
     * @return 刷新是否成功
     */
    bool RefreshRepository(const QString &repositoryPath);

    /**
     * @brief 清理指定仓库的状态缓存
     * @param repositoryPath 仓库路径
     * @return 清理是否成功
     */
    bool ClearRepositoryCache(const QString &repositoryPath);

    /**
     * @brief 获取所有已注册的仓库列表
     * @return 仓库路径列表
     */
    QStringList GetRegisteredRepositories();

    /**
     * @brief 获取服务状态信息
     * @return 状态信息映射
     */
    QVariantMap GetServiceStatus();

    /**
     * @brief 清理所有资源（缓存和监控）
     * 当文件管理器所有窗口关闭时调用，释放所有资源
     * @return 清理是否成功
     */
    bool ClearAllResources();

    /**
     * @brief 触发目录状态检索
     * @param directoryPath 目录路径
     * @return 触发是否成功
     */
    bool TriggerRetrieval(const QString &directoryPath);

Q_SIGNALS:
    /**
     * @brief 仓库状态发生变化时发出的信号
     * @param repositoryPath 仓库路径
     * @param changedFiles 变化的文件和其状态的映射
     */
    void RepositoryStatusChanged(const QString &repositoryPath, const QVariantMap &changedFiles);

    /**
     * @brief 发现新仓库时发出的信号
     * @param repositoryPath 新发现的仓库路径
     */
    void RepositoryDiscovered(const QString &repositoryPath);

    /**
     * @brief 请求清理所有资源的信号
     * 此信号会通知daemon清理所有缓存和监控资源
     */
    void ClearAllResourcesRequested();

    /**
     * @brief 请求添加仓库监控的信号
     * @param repositoryPath 仓库路径
     */
    void RepositoryWatchRequested(const QString &repositoryPath);

    /**
     * @brief 请求移除仓库监控的信号
     * @param repositoryPath 仓库路径
     */
    void RepositoryUnwatchRequested(const QString &repositoryPath);

    /**
     * @brief 请求状态检索的信号
     * @param directoryPath 目录路径
     */
    void RetrievalRequested(const QString &directoryPath);

private Q_SLOTS:
    /**
     * @brief 处理仓库变化事件
     * @param repositoryPath 仓库路径
     */
    void onRepositoryChanged(const QString &repositoryPath);

    /**
     * @brief 处理缓存状态变化事件
     * @param repositoryPath 仓库路径
     * @param changedFiles 变化的文件映射
     */
    void onRepositoryStatusChanged(const QString &repositoryPath, const QHash<QString, ItemVersion> &changedFiles);

    /**
     * @brief 处理仓库发现事件
     * @param repositoryPath 仓库路径
     */
    void onRepositoryDiscovered(const QString &repositoryPath);

private:
    /**
     * @brief 转换ItemVersion映射为QVariantMap（用于DBus传输）
     * @param versionMap ItemVersion映射
     * @return QVariantMap
     */
    QVariantMap convertToVariantMap(const QHash<QString, ItemVersion> &versionMap) const;

    /**
     * @brief 刷新仓库状态（内部实现）
     * @param repositoryPath 仓库路径
     */
    void refreshRepositoryInternal(const QString &repositoryPath);

private:
    bool m_serviceReady;    ///< 服务是否就绪
}; 