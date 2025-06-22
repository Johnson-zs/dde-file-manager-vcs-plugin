#pragma once

#include <QObject>
#include <QHash>
#include <QUrl>
#include "git-types.h"

/**
 * @brief Git版本工作器
 * 
 * 迁移自原始的GitVersionWorker，负责Git状态检索和解析
 * 集成到daemon进程中，作为状态检索引擎
 */
class GitVersionWorker : public QObject
{
    Q_OBJECT
    
public:
    explicit GitVersionWorker(QObject *parent = nullptr);
    ~GitVersionWorker();

public Q_SLOTS:
    /**
     * @brief 处理状态检索请求
     * @param url 目录URL
     */
    void onRetrieval(const QUrl &url);
    
    /**
     * @brief 处理状态检索请求（字符串路径版本）
     * @param directoryPath 目录路径
     */
    void onRetrieval(const QString &directoryPath);

Q_SIGNALS:
    /**
     * @brief 发现新仓库时发出的信号
     * @param repositoryPath 新发现的仓库路径
     */
    void newRepositoryAdded(const QString &repositoryPath);
    
    /**
     * @brief 状态检索完成时发出的信号
     * @param repositoryPath 仓库路径
     * @param versionInfoHash 文件状态映射
     */
    void retrievalCompleted(const QString &repositoryPath, const QHash<QString, ItemVersion> &versionInfoHash);

private:
    /**
     * @brief 核心状态检索函数（迁移自原始实现）
     * @param directory 目录路径
     * @return 文件状态映射
     */
    QHash<QString, ItemVersion> retrieval(const QString &directory);
    
    /**
     * @brief 计算仓库根目录状态（迁移自原始实现）
     * @param versionInfoHash 文件状态映射
     * @return 根目录状态
     */
    ItemVersion calculateRepositoryRootStatus(const QHash<QString, ItemVersion> &versionInfoHash);
}; 