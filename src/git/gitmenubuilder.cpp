#include "gitmenubuilder.h"
#include "gitoperationservice.h"
#include "utils.h"

#include <QFileInfo>

GitMenuBuilder::GitMenuBuilder(DFMEXT::DFMExtMenuProxy *proxy,
                               GitOperationService *operationService,
                               QObject *parent)
    : QObject(parent), m_proxy(proxy), m_operationService(operationService)
{
}

GitMenuBuilder::~GitMenuBuilder() = default;

bool GitMenuBuilder::buildSingleFileMenu(DFMEXT::DFMExtMenu *gitSubmenu,
                                         const QString &currentPath,
                                         const QString &focusPath)
{
    bool hasValidAction = false;
    const QString statusText = Utils::getFileStatusDescription(focusPath);

    // === 文件操作组 ===
    addFileOperationMenuItems(gitSubmenu, focusPath);

    // 检查是否添加了文件操作菜单项
    if (Utils::canAddFile(focusPath) || Utils::canRemoveFile(focusPath) || Utils::canRevertFile(focusPath)) {
        hasValidAction = true;

        // 添加分隔符
        auto separator = createSeparator();
        gitSubmenu->addAction(separator);
    }

    // === 查看操作组 ===
    addViewOperationMenuItems(gitSubmenu, focusPath, currentPath);

    // 检查是否添加了查看操作菜单项
    if (Utils::canShowFileDiff(focusPath) || Utils::canShowFileLog(focusPath) || Utils::canShowFileBlame(focusPath)) {
        hasValidAction = true;
    }

    return hasValidAction;
}

bool GitMenuBuilder::buildMultiFileMenu(DFMEXT::DFMExtMenu *gitSubmenu,
                                        const std::list<std::string> &pathList)
{
    if (pathList.empty()) {
        return false;
    }

    // 检查所有文件是否都在Git仓库中
    bool allInRepo = true;
    for (const auto &path : pathList) {
        if (!Utils::isInsideRepositoryFile(QString::fromStdString(path))) {
            allInRepo = false;
            break;
        }
    }

    if (!allInRepo) {
        return false;
    }

    // 获取兼容的操作列表
    QStringList compatibleOps = getCompatibleOperationsForMultiSelection(pathList);

    if (compatibleOps.isEmpty()) {
        return false;
    }

    addMultiFileOperationMenuItems(gitSubmenu, pathList, compatibleOps);
    return true;
}

bool GitMenuBuilder::buildRepositoryMenuItems(DFMEXT::DFMExtMenu *main,
                                              const QString &repositoryPath,
                                              DFMEXT::DFMExtAction *beforeAction)
{
    // 获取当前分支信息
    const QString branchName = Utils::getBranchName(repositoryPath);

    // === Git More...二级菜单（包含分支操作和同步操作） ===
    auto gitMoreAction = m_proxy->createAction();
    gitMoreAction->setText("Git More...");
    gitMoreAction->setToolTip(QString("More Git operations\nCurrent branch: %1").arg(branchName).toStdString());

    auto gitMoreSubmenu = m_proxy->createMenu();
    gitMoreAction->setMenu(gitMoreSubmenu);

    // === 分支操作组（在二级菜单中） ===
    addBranchOperationMenuItems(gitMoreSubmenu, repositoryPath);

    auto subSeparator0 = createSeparator();
    gitMoreSubmenu->addAction(subSeparator0);

    // === 同步操作组（在二级菜单中） ===
    addSyncOperationMenuItems(gitMoreSubmenu, repositoryPath);

    // 将Git More...菜单添加到主菜单
    if (beforeAction) {
        main->insertAction(beforeAction, gitMoreAction);
    } else {
        main->addAction(gitMoreAction);
    }

    auto separator0 = createSeparator();
    if (beforeAction) {
        main->insertAction(beforeAction, separator0);
    } else {
        main->addAction(separator0);
    }

    // === 查看操作组（直接在主菜单中） ===
    addRepositoryOperationMenuItems(main, repositoryPath, beforeAction);

    auto separator1 = createSeparator();
    if (beforeAction) {
        main->insertAction(beforeAction, separator1);
    } else {
        main->addAction(separator1);
    }

    // Git Commit（直接在主菜单中）
    auto commitAction = m_proxy->createAction();
    commitAction->setText("Git Commit...");
    commitAction->setIcon("vcs-commit");
    commitAction->setToolTip(QString("Commit staged changes to repository\nCurrent branch: %1").arg(branchName).toStdString());
    commitAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->commitChanges(repositoryPath.toStdString());
    });

    if (beforeAction) {
        main->insertAction(beforeAction, commitAction);
    } else {
        main->addAction(commitAction);
    }

    auto separator2 = createSeparator();
    if (beforeAction) {
        main->insertAction(beforeAction, separator2);
    } else {
        main->addAction(separator2);
    }

    return true;
}

// ============================================================================
// 私有方法实现
// ============================================================================

void GitMenuBuilder::addFileOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &filePath)
{
    const QString statusText = Utils::getFileStatusDescription(filePath);
    const QString fileName = QFileInfo(filePath).fileName();

    // Git Add
    if (Utils::canAddFile(filePath)) {
        auto addAction = m_proxy->createAction();
        addAction->setText("Git Add");
        addAction->setIcon("vcs-added");
        addAction->setToolTip(QString("Add '%1' to staging area\nCurrent status: %2")
                                      .arg(fileName, statusText)
                                      .toStdString());
        addAction->registerTriggered([this, filePath](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->addFile(filePath.toStdString());
        });
        menu->addAction(addAction);
    }

    // Git Remove
    if (Utils::canRemoveFile(filePath)) {
        auto removeAction = m_proxy->createAction();
        removeAction->setText("Git Remove");
        removeAction->setIcon("vcs-removed");
        removeAction->setToolTip(QString("Remove '%1' from Git tracking\nCurrent status: %2")
                                         .arg(fileName, statusText)
                                         .toStdString());
        removeAction->registerTriggered([this, filePath](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->removeFile(filePath.toStdString());
        });
        menu->addAction(removeAction);
    }

    // Git Revert
    if (Utils::canRevertFile(filePath)) {
        auto revertAction = m_proxy->createAction();
        revertAction->setText("Git Revert");
        revertAction->setIcon("vcs-update-required");
        revertAction->setToolTip(QString("Discard changes in '%1'\nCurrent status: %2\nWarning: This will permanently discard your changes!")
                                         .arg(fileName, statusText)
                                         .toStdString());
        revertAction->registerTriggered([this, filePath](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->revertFile(filePath.toStdString());
        });
        menu->addAction(revertAction);
    }
}

void GitMenuBuilder::addViewOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &filePath,
                                               const QString &currentPath)
{
    const QString statusText = Utils::getFileStatusDescription(filePath);
    const QString fileName = QFileInfo(filePath).fileName();

    // Git Diff
    if (Utils::canShowFileDiff(filePath)) {
        auto diffAction = m_proxy->createAction();
        diffAction->setText("Git Diff...");
        diffAction->setIcon("vcs-diff");
        diffAction->setToolTip(QString("View changes in '%1'\nCurrent status: %2")
                                       .arg(fileName, statusText)
                                       .toStdString());
        diffAction->registerTriggered([this, filePath](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->showFileDiff(filePath.toStdString());
        });
        menu->addAction(diffAction);
    }

    // Git Log (for file)
    if (Utils::canShowFileLog(filePath)) {
        auto logAction = m_proxy->createAction();
        logAction->setText("Git Log...");
        logAction->setIcon("vcs-normal");
        logAction->setToolTip(QString("View commit history for '%1'\nCurrent status: %2")
                                      .arg(fileName, statusText)
                                      .toStdString());
        logAction->registerTriggered([this, currentPath, filePath](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            QString repoPath = Utils::repositoryBaseDir(currentPath);
            m_operationService->showFileLog(repoPath.toStdString(), filePath.toStdString());
        });
        menu->addAction(logAction);
    }

    // Git Blame
    if (Utils::canShowFileBlame(filePath)) {
        auto blameAction = m_proxy->createAction();
        blameAction->setText("Git Blame...");
        blameAction->setIcon("vcs-annotation");
        blameAction->setToolTip(QString("View line-by-line authorship for '%1'\nCurrent status: %2")
                                        .arg(fileName, statusText)
                                        .toStdString());
        blameAction->registerTriggered([this, filePath](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->showFileBlame(filePath.toStdString());
        });
        menu->addAction(blameAction);
    }
}

void GitMenuBuilder::addRepositoryOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath,
                                                     DFMEXT::DFMExtAction *beforeAction)
{
    const QString branchName = Utils::getBranchName(repositoryPath);

    // Git Log (for repository)
    auto repoLogAction = m_proxy->createAction();
    repoLogAction->setText("Git Log...");
    repoLogAction->setIcon("vcs-normal");
    repoLogAction->setToolTip(QString("View repository commit history\nCurrent branch: %1").arg(branchName).toStdString());
    repoLogAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->showFileLog(repositoryPath.toStdString());
    });

    if (beforeAction) {
        menu->insertAction(beforeAction, repoLogAction);
    } else {
        menu->addAction(repoLogAction);
    }

    // Git Status
    auto statusAction = m_proxy->createAction();
    statusAction->setText("Git Status...");
    statusAction->setIcon("vcs-status");
    statusAction->setToolTip(QString("View repository status and pending changes\nCurrent branch: %1").arg(branchName).toStdString());
    statusAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->showRepositoryStatus(repositoryPath.toStdString());
    });

    if (beforeAction) {
        menu->insertAction(beforeAction, statusAction);
    } else {
        menu->addAction(statusAction);
    }
}

void GitMenuBuilder::addBranchOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath)
{
    const QString branchName = Utils::getBranchName(repositoryPath);

    // Git Checkout
    auto checkoutAction = m_proxy->createAction();
    checkoutAction->setText("Git Checkout...");
    checkoutAction->setIcon("vcs-branch");
    checkoutAction->setToolTip(QString("Switch branches or create new branch\nCurrent branch: %1").arg(branchName).toStdString());
    checkoutAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->checkoutBranch(repositoryPath.toStdString());
    });

    menu->addAction(checkoutAction);
}

void GitMenuBuilder::addSyncOperationMenuItems(DFMEXT::DFMExtMenu *menu, const QString &repositoryPath)
{
    const QString branchName = Utils::getBranchName(repositoryPath);

    // Git Pull - 使用高级对话框
    auto pullAction = m_proxy->createAction();
    pullAction->setText("Git Pull...");
    pullAction->setIcon("vcs-pull");
    pullAction->setToolTip(QString("Pull latest changes from remote repository with advanced options\nCurrent branch: %1").arg(branchName).toStdString());
    pullAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        // 使用GitOperationService显示高级Pull对话框
        m_operationService->showAdvancedPullDialog(repositoryPath);
    });

    menu->addAction(pullAction);

    // Git Push - 使用高级对话框
    auto pushAction = m_proxy->createAction();
    pushAction->setText("Git Push...");
    pushAction->setIcon("vcs-push");
    pushAction->setToolTip(QString("Push local commits to remote repository with advanced options\nCurrent branch: %1").arg(branchName).toStdString());
    pushAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        // 使用GitOperationService显示高级Push对话框
        m_operationService->showAdvancedPushDialog(repositoryPath);
    });

    menu->addAction(pushAction);

    // 添加分隔符
    auto separator = createSeparator();
    menu->addAction(separator);

    // 快速Pull（保留简单操作）
    auto quickPullAction = m_proxy->createAction();
    quickPullAction->setText("Quick Pull");
    quickPullAction->setIcon("vcs-pull");
    quickPullAction->setToolTip(QString("Quick pull from default remote\nCurrent branch: %1").arg(branchName).toStdString());
    quickPullAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->pullRepository(repositoryPath.toStdString());
    });

    menu->addAction(quickPullAction);

    // 快速Push（保留简单操作）
    auto quickPushAction = m_proxy->createAction();
    quickPushAction->setText("Quick Push");
    quickPushAction->setIcon("vcs-push");
    quickPushAction->setToolTip(QString("Quick push to default remote\nCurrent branch: %1").arg(branchName).toStdString());
    quickPushAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->pushRepository(repositoryPath.toStdString());
    });

    menu->addAction(quickPushAction);

    // 添加分隔符
    auto separator2 = createSeparator();
    menu->addAction(separator2);

    // Git Remote Manager
    auto remoteManagerAction = m_proxy->createAction();
    remoteManagerAction->setText("Git Remote Manager...");
    remoteManagerAction->setIcon("vcs-branch");
    remoteManagerAction->setToolTip(QString("Manage remote repositories\nCurrent branch: %1").arg(branchName).toStdString());
    remoteManagerAction->registerTriggered([this, repositoryPath](DFMEXT::DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        m_operationService->showRemoteManager(repositoryPath);
    });

    menu->addAction(remoteManagerAction);
}

QStringList GitMenuBuilder::getCompatibleOperationsForMultiSelection(const std::list<std::string> &pathList)
{
    QStringList operations;

    bool canAdd = true;
    bool canRemove = true;
    bool canRevert = true;

    // 检查所有文件的状态兼容性
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);

        if (!Utils::canAddFile(filePath)) {
            canAdd = false;
        }
        if (!Utils::canRemoveFile(filePath)) {
            canRemove = false;
        }
        if (!Utils::canRevertFile(filePath)) {
            canRevert = false;
        }
    }

    if (canAdd) operations << "add";
    if (canRemove) operations << "remove";
    if (canRevert) operations << "revert";

    return operations;
}

void GitMenuBuilder::addMultiFileOperationMenuItems(DFMEXT::DFMExtMenu *menu,
                                                    const std::list<std::string> &pathList,
                                                    const QStringList &compatibleOps)
{
    const int fileCount = static_cast<int>(pathList.size());
    const QString fileCountText = getFileCountText(fileCount);

    // 批量 Git Add
    if (compatibleOps.contains("add")) {
        auto addAction = m_proxy->createAction();
        addAction->setText(tr("Git Add Selected").toStdString());
        addAction->setIcon("vcs-added");
        addAction->setToolTip(tr("Add all selected files to staging area\n%1").arg(fileCountText).toStdString());
        addAction->registerTriggered([this, pathList](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->addMultipleFiles(pathList);
        });
        menu->addAction(addAction);
    }

    // 批量 Git Remove
    if (compatibleOps.contains("remove")) {
        auto removeAction = m_proxy->createAction();
        removeAction->setText(tr("Git Remove Selected").toStdString());
        removeAction->setIcon("vcs-removed");
        removeAction->setToolTip(tr("Remove all selected files from Git tracking\n%1").arg(fileCountText).toStdString());
        removeAction->registerTriggered([this, pathList](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->removeMultipleFiles(pathList);
        });
        menu->addAction(removeAction);
    }

    // 批量 Git Revert
    if (compatibleOps.contains("revert")) {
        auto revertAction = m_proxy->createAction();
        revertAction->setText(tr("Git Revert Selected").toStdString());
        revertAction->setIcon("vcs-update-required");
        revertAction->setToolTip(tr("Discard changes in all selected files\n%1\nWarning: This will permanently discard your changes!")
                                         .arg(fileCountText)
                                         .toStdString());
        revertAction->registerTriggered([this, pathList](DFMEXT::DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            m_operationService->revertMultipleFiles(pathList);
        });
        menu->addAction(revertAction);
    }
}

DFMEXT::DFMExtAction *GitMenuBuilder::createSeparator()
{
    auto separatorAction = m_proxy->createAction();
    separatorAction->setSeparator(true);
    return separatorAction;
}

QString GitMenuBuilder::getFileCountText(int count) const
{
    return tr("%1 files selected").arg(count);
}
