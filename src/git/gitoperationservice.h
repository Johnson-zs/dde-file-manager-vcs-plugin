#ifndef GITOPERATIONSERVICE_H
#define GITOPERATIONSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <list>
#include <string>

#include "gitcommandexecutor.h"

/**
 * @brief Git操作服务类
 * 
 * 负责所有Git操作的执行，包括文件操作、仓库操作等
 * 提供统一的操作接口和结果通知
 */
class GitOperationService : public QObject
{
    Q_OBJECT

public:
    explicit GitOperationService(QObject *parent = nullptr);
    ~GitOperationService();

    // === 文件操作 ===
    void addFile(const std::string &filePath);
    void removeFile(const std::string &filePath);
    void revertFile(const std::string &filePath);
    void showFileDiff(const std::string &filePath);
    void showFileBlame(const std::string &filePath);
    void showFileLog(const std::string &repositoryPath, const std::string &filePath = std::string());

    // === 批量文件操作 ===
    void addMultipleFiles(const std::list<std::string> &pathList);
    void removeMultipleFiles(const std::list<std::string> &pathList);
    void revertMultipleFiles(const std::list<std::string> &pathList);

    // === 仓库操作 ===
    void showRepositoryStatus(const std::string &repositoryPath);
    void checkoutBranch(const std::string &repositoryPath);
    void pushRepository(const std::string &repositoryPath);
    void pullRepository(const std::string &repositoryPath);
    void commitChanges(const std::string &repositoryPath);

Q_SIGNALS:
    /**
     * @brief Git操作完成信号
     * @param operation 操作名称
     * @param success 是否成功
     * @param message 结果消息
     */
    void operationCompleted(const QString &operation, bool success, const QString &message = QString());

    /**
     * @brief 需要刷新文件管理器信号
     */
    void fileManagerRefreshRequested();

private Q_SLOTS:
    void onCommandFinished(const QString &command, GitCommandExecutor::Result result,
                          const QString &output, const QString &error);

private:
    // === 内部辅助方法 ===
    void executeSilentOperation(const QString &operation, const QString &workingDir, 
                               const QStringList &arguments);
    void executeInteractiveOperation(const QString &operation, const QString &workingDir, 
                                   const QStringList &arguments);
    void showSuccessNotification(const QString &operation);

    // === 多文件操作辅助 ===
    QString resolveRepositoryPath(const std::string &filePath);
    QStringList buildFileArguments(const QString &operation, const QString &repoPath, 
                                  const std::list<std::string> &pathList);

    GitCommandExecutor *m_executor;
};

#endif // GITOPERATIONSERVICE_H 