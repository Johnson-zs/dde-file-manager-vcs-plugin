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

    // === 高级Push/Pull对话框 ===
    void showAdvancedPushDialog(const QString &repositoryPath);
    void showAdvancedPullDialog(const QString &repositoryPath);
    void showRemoteManager(const QString &repositoryPath);

    // === 高级Push/Pull操作 ===
    void pushWithOptions(const QString &repositoryPath, const QString &remoteName, 
                        const QString &localBranch, const QString &remoteBranch,
                        bool forceWithLease = false, bool pushTags = false, 
                        bool setUpstream = false, bool dryRun = false);
    void pullWithOptions(const QString &repositoryPath, const QString &remoteName,
                        const QString &remoteBranch, const QString &strategy = "merge",
                        bool prune = false, bool autoStash = false, bool dryRun = false);

    // === 远程仓库管理 ===
    void addRemote(const QString &repositoryPath, const QString &name, const QString &url);
    void removeRemote(const QString &repositoryPath, const QString &name);
    void renameRemote(const QString &repositoryPath, const QString &oldName, const QString &newName);
    void setRemoteUrl(const QString &repositoryPath, const QString &name, const QString &url);
    bool testRemoteConnection(const QString &repositoryPath, const QString &remoteName);
    void testRemoteConnectionAsync(const QString &repositoryPath, const QString &remoteName);

    // === Stash操作 ===
    void createStash(const std::string &repositoryPath, const QString &message = QString());
    void applyStash(const std::string &repositoryPath, int stashIndex, bool keepStash = false);
    void deleteStash(const std::string &repositoryPath, int stashIndex);
    void createBranchFromStash(const std::string &repositoryPath, int stashIndex, const QString &branchName);
    void showStashDiff(const std::string &repositoryPath, int stashIndex);
    void showStashManager(const std::string &repositoryPath);
    QStringList listStashes(const QString &repositoryPath);
    bool hasStashes(const QString &repositoryPath);

    // === 分支和状态查询 ===
    QStringList getRemotes(const QString &repositoryPath);
    QStringList getLocalBranches(const QString &repositoryPath);
    QStringList getRemoteBranches(const QString &repositoryPath, const QString &remoteName = QString());
    QString getCurrentBranch(const QString &repositoryPath);
    QStringList getUnpushedCommits(const QString &repositoryPath, const QString &remoteBranch);
    QStringList getRemoteUpdates(const QString &repositoryPath, const QString &remoteBranch);
    bool hasLocalChanges(const QString &repositoryPath);
    bool hasUncommittedChanges(const QString &repositoryPath);

Q_SIGNALS:
    /**
     * @brief Git操作完成信号
     * @param operation 操作名称
     * @param success 是否成功
     * @param message 结果消息
     */
    void operationCompleted(const QString &operation, bool success, const QString &message = QString());

    /**
     * @brief 远程连接测试完成信号
     * @param remoteName 远程仓库名称
     * @param success 是否连接成功
     * @param message 结果消息
     */
    void remoteConnectionTestCompleted(const QString &remoteName, bool success, const QString &message = QString());

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
    QString m_currentTestingRemote; // 当前正在测试的远程名称
};

#endif // GITOPERATIONSERVICE_H 