#ifndef GITLOGCONTEXTMENUMANAGER_H
#define GITLOGCONTEXTMENUMANAGER_H

#include <QObject>
#include <QMenu>
#include <QAction>
#include <QPoint>
#include <QString>

// Forward declarations
class GitOperationDialog;

/**
 * @brief Git日志右键菜单管理器
 * 
 * 专门负责创建和管理Git日志界面中的右键菜单：
 * - 提交相关操作菜单
 * - 文件相关操作菜单
 * - 菜单项的启用/禁用控制
 * - Git操作的执行和回调
 */
class GitLogContextMenuManager : public QObject
{
    Q_OBJECT

public:
    explicit GitLogContextMenuManager(const QString &repositoryPath, QObject *parent = nullptr);
    ~GitLogContextMenuManager();

    // === 菜单创建和显示 ===
    void showCommitContextMenu(const QPoint &globalPos, const QString &commitHash, const QString &commitMessage);
    void showFileContextMenu(const QPoint &globalPos, const QString &commitHash, const QString &filePath);

    // === 配置 ===
    void setRepositoryPath(const QString &path) { m_repositoryPath = path; }
    QString repositoryPath() const { return m_repositoryPath; }

Q_SIGNALS:
    // Git操作信号
    void gitOperationRequested(const QString &operation, const QStringList &args, bool needsConfirmation = false);
    void refreshRequested();
    
    // 界面操作信号
    void showCommitDetailsRequested(const QString &commitHash);
    void showFileDiffRequested(const QString &commitHash, const QString &filePath);
    void showFileHistoryRequested(const QString &filePath);
    void showFileBlameRequested(const QString &filePath);
    void compareWithWorkingTreeRequested(const QString &commitHash);
    void viewFileAtCommitRequested(const QString &commitHash, const QString &filePath);
    
    // 文件操作信号
    void openFileRequested(const QString &filePath);
    void showFileInFolderRequested(const QString &filePath);

private Q_SLOTS:
    // === Commit操作槽函数 ===
    void onCheckoutCommit();
    void onCreateBranchFromCommit();
    void onCreateTagFromCommit();
    void onSoftResetToCommit();
    void onMixedResetToCommit();
    void onHardResetToCommit();
    void onRevertCommit();
    void onCherryPickCommit();
    void onShowCommitDetails();
    void onCompareWithWorkingTree();
    void onCopyCommitHash();
    void onCopyShortHash();
    void onCopyCommitMessage();

    // === 文件操作槽函数 ===
    void onViewFileAtCommit();
    void onShowFileDiff();
    void onShowFileHistory();
    void onShowFileBlame();
    void onOpenFile();
    void onShowInFolder();
    void onCopyFilePath();
    void onCopyFileName();

private:
    void setupCommitContextMenu();
    void setupFileContextMenu();
    void updateCommitMenuState(const QString &commitHash, const QString &commitMessage);
    void updateFileMenuState(const QString &commitHash, const QString &filePath);

    QString m_repositoryPath;
    
    // 当前上下文
    QString m_currentCommitHash;
    QString m_currentCommitMessage;
    QString m_currentFilePath;
    
    // === 提交右键菜单 ===
    QMenu *m_commitContextMenu;
    QAction *m_checkoutCommitAction;
    QAction *m_createBranchAction;
    QAction *m_createTagAction;
    QMenu *m_resetMenu;
    QAction *m_softResetAction;
    QAction *m_mixedResetAction;
    QAction *m_hardResetAction;
    QAction *m_revertCommitAction;
    QAction *m_cherryPickAction;
    QAction *m_showDetailsAction;
    QAction *m_compareWorkingTreeAction;
    QAction *m_copyHashAction;
    QAction *m_copyShortHashAction;
    QAction *m_copyMessageAction;

    // === 文件右键菜单 ===
    QMenu *m_fileContextMenu;
    QAction *m_viewFileAction;
    QAction *m_showFileDiffAction;
    QAction *m_showFileHistoryAction;
    QAction *m_showFileBlameAction;
    QAction *m_openFileAction;
    QAction *m_showFolderAction;
    QAction *m_copyFilePathAction;
    QAction *m_copyFileNameAction;
};

#endif // GITLOGCONTEXTMENUMANAGER_H 