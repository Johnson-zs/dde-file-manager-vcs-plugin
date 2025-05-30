#ifndef GITDIALOGS_H
#define GITDIALOGS_H

#include <QString>
#include <QWidget>
#include <QStringList>

/**
 * @brief Git对话框统一管理器
 * 
 * 提供统一的接口来创建和管理所有Git相关对话框，
 * 避免对话框之间的直接依赖和include问题
 */
class GitDialogManager
{
public:
    static GitDialogManager* instance();

    // 对话框创建接口
    void showCommitDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent = nullptr);
    void showStatusDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showLogDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    void showBlameDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    void showDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    void showCheckoutDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    void showOperationDialog(const QString &operation, QWidget *parent = nullptr);
    void showOperationDialog(const QString &operation, const QString &workingDir, 
                            const QStringList &arguments, QWidget *parent = nullptr);

    // 文件操作接口
    void openFile(const QString &filePath, QWidget *parent = nullptr);
    void showFileInFolder(const QString &filePath, QWidget *parent = nullptr);
    void deleteFile(const QString &filePath, QWidget *parent = nullptr);

private:
    GitDialogManager() = default;
    static GitDialogManager* s_instance;
};

#endif // GITDIALOGS_H 