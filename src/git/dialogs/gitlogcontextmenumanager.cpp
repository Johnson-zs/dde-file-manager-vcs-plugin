#include "gitlogcontextmenumanager.h"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

GitLogContextMenuManager::GitLogContextMenuManager(const QString &repositoryPath, QObject *parent)
    : QObject(parent), m_repositoryPath(repositoryPath), m_commitContextMenu(nullptr), m_fileContextMenu(nullptr)
{
    setupCommitContextMenu();
    setupFileContextMenu();

    qDebug() << "[GitLogContextMenuManager] Initialized for repository:" << repositoryPath;
}

GitLogContextMenuManager::~GitLogContextMenuManager()
{
    delete m_commitContextMenu;
    delete m_fileContextMenu;
}

void GitLogContextMenuManager::setupCommitContextMenu()
{
    m_commitContextMenu = new QMenu;

    // === 基础操作 ===
    m_checkoutCommitAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("vcs-normal"), tr("Checkout Commit"));
    m_createBranchAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("vcs-branch"), tr("Create Branch Here"));
    m_createTagAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("vcs-tag"), tr("Create Tag Here"));

    m_commitContextMenu->addSeparator();

    // === Reset操作子菜单 ===
    m_resetMenu = m_commitContextMenu->addMenu(
            QIcon::fromTheme("edit-undo"), tr("Reset to Here"));
    m_softResetAction = m_resetMenu->addAction(tr("Soft Reset"));
    m_mixedResetAction = m_resetMenu->addAction(tr("Mixed Reset"));
    m_hardResetAction = m_resetMenu->addAction(tr("Hard Reset"));

    // 设置工具提示
    m_softResetAction->setToolTip(tr("Keep working directory and staging area"));
    m_mixedResetAction->setToolTip(tr("Keep working directory, reset staging area"));
    m_hardResetAction->setToolTip(tr("Reset working directory and staging area"));

    // === 其他操作 ===
    m_revertCommitAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("edit-undo"), tr("Revert Commit"));
    m_cherryPickAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("vcs-merge"), tr("Cherry-pick Commit"));

    m_commitContextMenu->addSeparator();

    // === 查看操作 ===
    m_compareWorkingTreeAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("document-compare"), tr("Compare with Working Tree"));

    m_commitContextMenu->addSeparator();

    // === 复制操作 ===
    m_copyHashAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("edit-copy"), tr("Copy Commit Hash"));
    m_copyShortHashAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("edit-copy"), tr("Copy Short Hash"));
    m_copyMessageAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("edit-copy"), tr("Copy Commit Message"));

    // === 新增：浏览器打开commit ===
    m_commitContextMenu->addSeparator();
    m_openInBrowserAction = m_commitContextMenu->addAction(
            QIcon::fromTheme("internet-web-browser"), tr("Open in Browser"));
    m_openInBrowserAction->setToolTip(tr("Open commit in web browser"));

    // === 连接信号 ===
    connect(m_checkoutCommitAction, &QAction::triggered, this, &GitLogContextMenuManager::onCheckoutCommit);
    connect(m_createBranchAction, &QAction::triggered, this, &GitLogContextMenuManager::onCreateBranchFromCommit);
    connect(m_createTagAction, &QAction::triggered, this, &GitLogContextMenuManager::onCreateTagFromCommit);
    connect(m_softResetAction, &QAction::triggered, this, &GitLogContextMenuManager::onSoftResetToCommit);
    connect(m_mixedResetAction, &QAction::triggered, this, &GitLogContextMenuManager::onMixedResetToCommit);
    connect(m_hardResetAction, &QAction::triggered, this, &GitLogContextMenuManager::onHardResetToCommit);
    connect(m_revertCommitAction, &QAction::triggered, this, &GitLogContextMenuManager::onRevertCommit);
    connect(m_cherryPickAction, &QAction::triggered, this, &GitLogContextMenuManager::onCherryPickCommit);
    connect(m_compareWorkingTreeAction, &QAction::triggered, this, &GitLogContextMenuManager::onCompareWithWorkingTree);
    connect(m_copyHashAction, &QAction::triggered, this, &GitLogContextMenuManager::onCopyCommitHash);
    connect(m_copyShortHashAction, &QAction::triggered, this, &GitLogContextMenuManager::onCopyShortHash);
    connect(m_copyMessageAction, &QAction::triggered, this, &GitLogContextMenuManager::onCopyCommitMessage);

    // === 新增：浏览器打开commit信号连接 ===
    connect(m_openInBrowserAction, &QAction::triggered, this, &GitLogContextMenuManager::onOpenCommitInBrowser);
}

void GitLogContextMenuManager::setupFileContextMenu()
{
    m_fileContextMenu = new QMenu;

    // === 文件查看操作 ===
    m_viewFileAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("document-open"), tr("View File at This Commit"));
    m_showFileDiffAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("document-properties"), tr("Show File Diff"));
    m_showFileHistoryAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("view-list-details"), tr("Show File History"));
    m_showFileBlameAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("view-list-tree"), tr("Show File Blame"));

    m_fileContextMenu->addSeparator();

    // === 文件管理操作 ===
    m_openFileAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("document-open"), tr("Open File"));
    m_showFolderAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("folder-open"), tr("Show in Folder"));

    m_fileContextMenu->addSeparator();

    // === 复制操作 ===
    m_copyFilePathAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("edit-copy"), tr("Copy File Path"));
    m_copyFileNameAction = m_fileContextMenu->addAction(
            QIcon::fromTheme("edit-copy"), tr("Copy File Name"));

    // === 连接信号 ===
    connect(m_viewFileAction, &QAction::triggered, this, &GitLogContextMenuManager::onViewFileAtCommit);
    connect(m_showFileDiffAction, &QAction::triggered, this, &GitLogContextMenuManager::onShowFileDiff);
    connect(m_showFileHistoryAction, &QAction::triggered, this, &GitLogContextMenuManager::onShowFileHistory);
    connect(m_showFileBlameAction, &QAction::triggered, this, &GitLogContextMenuManager::onShowFileBlame);
    connect(m_openFileAction, &QAction::triggered, this, &GitLogContextMenuManager::onOpenFile);
    connect(m_showFolderAction, &QAction::triggered, this, &GitLogContextMenuManager::onShowInFolder);
    connect(m_copyFilePathAction, &QAction::triggered, this, &GitLogContextMenuManager::onCopyFilePath);
    connect(m_copyFileNameAction, &QAction::triggered, this, &GitLogContextMenuManager::onCopyFileName);
}

void GitLogContextMenuManager::showCommitContextMenu(const QPoint &globalPos, const QString &commitHash, const QString &commitMessage)
{
    m_currentCommitHash = commitHash;
    m_currentCommitMessage = commitMessage;

    updateCommitMenuState(commitHash, commitMessage);
    m_commitContextMenu->exec(globalPos);
}

void GitLogContextMenuManager::showCommitContextMenu(const QPoint &globalPos, const QString &commitHash, const QString &commitMessage, 
                                                     bool isRemoteCommit, bool hasRemoteUrl)
{
    m_currentCommitHash = commitHash;
    m_currentCommitMessage = commitMessage;

    updateCommitMenuState(commitHash, commitMessage, isRemoteCommit, hasRemoteUrl);
    m_commitContextMenu->exec(globalPos);
}

void GitLogContextMenuManager::showFileContextMenu(const QPoint &globalPos, const QString &commitHash, const QString &filePath)
{
    m_currentCommitHash = commitHash;
    m_currentFilePath = filePath;

    updateFileMenuState(commitHash, filePath);
    m_fileContextMenu->exec(globalPos);
}

void GitLogContextMenuManager::updateCommitMenuState(const QString &commitHash, const QString &commitMessage)
{
    QString shortHash = commitHash.left(8);

    // 更新菜单项文本
    m_checkoutCommitAction->setText(tr("Checkout Commit (%1)").arg(shortHash));
    m_createBranchAction->setText(tr("Create Branch from %1").arg(shortHash));
    m_createTagAction->setText(tr("Create Tag at %1").arg(shortHash));
    m_revertCommitAction->setText(tr("Revert %1").arg(shortHash));
    m_cherryPickAction->setText(tr("Cherry-pick %1").arg(shortHash));

    // === 新增：更新浏览器打开菜单项文本 ===
    m_openInBrowserAction->setText(tr("Open %1 in Browser").arg(shortHash));
}

void GitLogContextMenuManager::updateCommitMenuState(const QString &commitHash, const QString &commitMessage, 
                                                     bool isRemoteCommit, bool hasRemoteUrl)
{
    QString shortHash = commitHash.left(8);

    // 更新菜单项文本
    m_checkoutCommitAction->setText(tr("Checkout Commit (%1)").arg(shortHash));
    m_createBranchAction->setText(tr("Create Branch from %1").arg(shortHash));
    m_createTagAction->setText(tr("Create Tag at %1").arg(shortHash));
    m_revertCommitAction->setText(tr("Revert %1").arg(shortHash));
    m_cherryPickAction->setText(tr("Cherry-pick %1").arg(shortHash));

    // === 关键修复：只对远程commit或已同步的commit显示浏览器打开选项 ===
    // 只有当满足以下条件时才显示浏览器打开选项：
    // 1. 有远程URL配置
    // 2. 且commit存在于远程（isRemoteCommit为true表示Remote或Both类型）
    bool shouldShowBrowserAction = hasRemoteUrl && isRemoteCommit;
    
    m_openInBrowserAction->setVisible(shouldShowBrowserAction);
    m_openInBrowserAction->setEnabled(shouldShowBrowserAction);
    
    if (shouldShowBrowserAction) {
        m_openInBrowserAction->setText(tr("Open %1 in Browser").arg(shortHash));
        m_openInBrowserAction->setToolTip(tr("Open commit in web browser"));
    }
    
    qDebug() << QString("[GitLogContextMenuManager] Commit %1: hasRemoteUrl=%2, isRemoteCommit=%3, showBrowser=%4")
                        .arg(shortHash)
                        .arg(hasRemoteUrl)
                        .arg(isRemoteCommit)
                        .arg(shouldShowBrowserAction);
}

void GitLogContextMenuManager::updateFileMenuState(const QString &commitHash, const QString &filePath)
{
    QString fileName = QFileInfo(filePath).fileName();

    // 更新菜单项文本
    m_viewFileAction->setText(tr("View %1 at This Commit").arg(fileName));
    m_showFileDiffAction->setText(tr("Show Diff for %1").arg(fileName));
    m_showFileHistoryAction->setText(tr("Show History of %1").arg(fileName));
    m_showFileBlameAction->setText(tr("Show Blame for %1").arg(fileName));
}

// === Commit操作实现 ===

void GitLogContextMenuManager::onCheckoutCommit()
{
    if (m_currentCommitHash.isEmpty()) return;

    int ret = QMessageBox::warning(nullptr, tr("Checkout Commit"),
                                   tr("This will checkout commit %1 and put you in 'detached HEAD' state.\n\n"
                                      "Do you want to continue?")
                                           .arg(m_currentCommitHash.left(8)),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        Q_EMIT gitOperationRequested(tr("Checkout Commit"), { "checkout", m_currentCommitHash });
    }
}

void GitLogContextMenuManager::onCreateBranchFromCommit()
{
    if (m_currentCommitHash.isEmpty()) return;

    bool ok;
    QString branchName = QInputDialog::getText(nullptr, tr("Create Branch"),
                                               tr("Enter branch name:"), QLineEdit::Normal, "", &ok);

    if (ok && !branchName.isEmpty()) {
        Q_EMIT gitOperationRequested(tr("Create Branch"), { "checkout", "-b", branchName, m_currentCommitHash });
    }
}

void GitLogContextMenuManager::onCreateTagFromCommit()
{
    if (m_currentCommitHash.isEmpty()) return;

    bool ok;
    QString tagName = QInputDialog::getText(nullptr, tr("Create Tag"),
                                            tr("Enter tag name:"), QLineEdit::Normal, "", &ok);

    if (ok && !tagName.isEmpty()) {
        Q_EMIT gitOperationRequested(tr("Create Tag"), { "tag", tagName, m_currentCommitHash });
    }
}

void GitLogContextMenuManager::onSoftResetToCommit()
{
    if (m_currentCommitHash.isEmpty()) return;
    Q_EMIT gitOperationRequested(tr("Soft Reset"), { "reset", "--soft", m_currentCommitHash }, true);
}

void GitLogContextMenuManager::onMixedResetToCommit()
{
    if (m_currentCommitHash.isEmpty()) return;
    Q_EMIT gitOperationRequested(tr("Mixed Reset"), { "reset", "--mixed", m_currentCommitHash }, true);
}

void GitLogContextMenuManager::onHardResetToCommit()
{
    if (m_currentCommitHash.isEmpty()) return;

    int ret = QMessageBox::warning(nullptr, tr("Hard Reset"),
                                   tr("This will permanently discard all local changes and reset to commit %1.\n\n"
                                      "This action cannot be undone. Are you sure?")
                                           .arg(m_currentCommitHash.left(8)),
                                   QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        Q_EMIT gitOperationRequested(tr("Hard Reset"), { "reset", "--hard", m_currentCommitHash });
    }
}

void GitLogContextMenuManager::onRevertCommit()
{
    if (m_currentCommitHash.isEmpty()) return;
    Q_EMIT gitOperationRequested(tr("Revert Commit"), { "revert", "--no-edit", m_currentCommitHash });
}

void GitLogContextMenuManager::onCherryPickCommit()
{
    if (m_currentCommitHash.isEmpty()) return;
    Q_EMIT gitOperationRequested(tr("Cherry-pick Commit"), { "cherry-pick", m_currentCommitHash });
}

void GitLogContextMenuManager::onCompareWithWorkingTree()
{
    if (m_currentCommitHash.isEmpty()) return;
    Q_EMIT compareWithWorkingTreeRequested(m_currentCommitHash);
}

void GitLogContextMenuManager::onCopyCommitHash()
{
    if (!m_currentCommitHash.isEmpty()) {
        QApplication::clipboard()->setText(m_currentCommitHash);
        qDebug() << "[GitLogContextMenuManager] Copied full commit hash to clipboard:" << m_currentCommitHash;
    }
}

void GitLogContextMenuManager::onCopyShortHash()
{
    if (!m_currentCommitHash.isEmpty()) {
        QString shortHash = m_currentCommitHash.left(8);
        QApplication::clipboard()->setText(shortHash);
        qDebug() << "[GitLogContextMenuManager] Copied short commit hash to clipboard:" << shortHash;
    }
}

void GitLogContextMenuManager::onCopyCommitMessage()
{
    if (!m_currentCommitMessage.isEmpty()) {
        QApplication::clipboard()->setText(m_currentCommitMessage);
        qDebug() << "[GitLogContextMenuManager] Copied commit message to clipboard:" << m_currentCommitMessage;
    }
}

// === 文件操作实现 ===

void GitLogContextMenuManager::onViewFileAtCommit()
{
    if (m_currentCommitHash.isEmpty() || m_currentFilePath.isEmpty()) return;
    Q_EMIT viewFileAtCommitRequested(m_currentCommitHash, m_currentFilePath);
}

void GitLogContextMenuManager::onShowFileDiff()
{
    if (m_currentCommitHash.isEmpty() || m_currentFilePath.isEmpty()) return;
    Q_EMIT showFileDiffRequested(m_currentCommitHash, m_currentFilePath);
}

void GitLogContextMenuManager::onShowFileHistory()
{
    if (m_currentFilePath.isEmpty()) return;
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_currentFilePath);
    Q_EMIT showFileHistoryRequested(absolutePath);
}

void GitLogContextMenuManager::onShowFileBlame()
{
    if (m_currentFilePath.isEmpty()) return;
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_currentFilePath);
    Q_EMIT showFileBlameRequested(absolutePath);
}

void GitLogContextMenuManager::onOpenFile()
{
    if (m_currentFilePath.isEmpty()) return;
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_currentFilePath);
    Q_EMIT openFileRequested(absolutePath);
}

void GitLogContextMenuManager::onShowInFolder()
{
    if (m_currentFilePath.isEmpty()) return;
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_currentFilePath);
    Q_EMIT showFileInFolderRequested(absolutePath);
}

void GitLogContextMenuManager::onCopyFilePath()
{
    if (!m_currentFilePath.isEmpty()) {
        QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(m_currentFilePath);
        QApplication::clipboard()->setText(absolutePath);
        qDebug() << "[GitLogContextMenuManager] Copied file path to clipboard:" << absolutePath;
    }
}

void GitLogContextMenuManager::onCopyFileName()
{
    if (!m_currentFilePath.isEmpty()) {
        QString fileName = QFileInfo(m_currentFilePath).fileName();
        QApplication::clipboard()->setText(fileName);
        qDebug() << "[GitLogContextMenuManager] Copied file name to clipboard:" << fileName;
    }
}

void GitLogContextMenuManager::onOpenCommitInBrowser()
{
    if (m_currentCommitHash.isEmpty()) return;
    Q_EMIT openCommitInBrowserRequested(m_currentCommitHash);
}
