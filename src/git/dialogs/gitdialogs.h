#ifndef GITDIALOGS_H
#define GITDIALOGS_H

#include <QString>
#include <QWidget>
#include <QStringList>
#include <functional>

// Forward declarations
class GitFilePreviewDialog;

/**
 * @brief Git对话框统一管理器
 *
 * 提供统一的接口来创建和管理所有Git相关对话框，
 * 避免对话框之间的直接依赖和include问题
 */
class GitDialogManager
{
public:
    static GitDialogManager *instance();

    // 对话框创建接口
    void showCommitDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent = nullptr);
    void showCommitDialog(const QString &repositoryPath, QWidget *parent, std::function<void(bool)> onFinished);
    void showStatusDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showLogDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    void showLogDialog(const QString &repositoryPath, const QString &filePath, const QString &initialBranch, QWidget *parent = nullptr);
    void showBlameDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    void showDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    void showCheckoutDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showBranchComparisonDialog(const QString &repositoryPath, const QString &baseBranch, 
                                    const QString &compareBranch, QWidget *parent = nullptr);
    void showOperationDialog(const QString &operation, QWidget *parent = nullptr);
    void showOperationDialog(const QString &operation, const QString &workingDir,
                             const QStringList &arguments, QWidget *parent = nullptr);

    // === 高级Push/Pull对话框 ===
    void showPushDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showPullDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showRemoteManager(const QString &repositoryPath, QWidget *parent = nullptr);

    // === Stash管理对话框 ===
    void showStashDialog(const QString &repositoryPath, QWidget *parent = nullptr);

    // === Git Clean对话框 ===
    void showCleanDialog(const QString &repositoryPath, QWidget *parent = nullptr);

    // 文件预览接口
    GitFilePreviewDialog* showFilePreview(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    GitFilePreviewDialog* showFilePreviewAtCommit(const QString &repositoryPath, const QString &filePath, 
                                                  const QString &commitHash, QWidget *parent = nullptr);

    // 文件操作接口
    void openFile(const QString &filePath, QWidget *parent = nullptr);
    void showFileInFolder(const QString &filePath, QWidget *parent = nullptr);
    void deleteFile(const QString &filePath, QWidget *parent = nullptr);

    // === 新增：支持commit文件差异的方法 ===
    void showCommitFileDiffDialog(const QString &repositoryPath, const QString &filePath, 
                                  const QString &commitHash, QWidget *parent = nullptr);

private:
    GitDialogManager() = default;
    static GitDialogManager *s_instance;
};

#endif   // GITDIALOGS_H
