#include "gitmenuplugin.h"

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>
#include <dfm-extension/window/dfmextwindowplugin.h>

#include <QWidget>
#include <QApplication>
#include <QFileInfo>
#include <QDir>

#include "utils.h"
#include "gitdialogs.h"
#include "gitcommandexecutor.h"
#include <cache.h>

USING_DFMEXT_NAMESPACE

GitMenuPlugin::GitMenuPlugin()
    : DFMEXT::DFMExtMenuPlugin()
{
    registerInitialize([this](DFMEXT::DFMExtMenuProxy *proxy) {
        initialize(proxy);
    });
    registerBuildNormalMenu([this](DFMExtMenu *main, const std::string &currentPath,
                                   const std::string &focusPath, const std::list<std::string> &pathList,
                                   bool onDesktop) {
        return buildNormalMenu(main, currentPath, focusPath, pathList, onDesktop);
    });
    registerBuildEmptyAreaMenu([this](DFMExtMenu *main, const std::string &currentPath, bool onDesktop) {
        return buildEmptyAreaMenu(main, currentPath, onDesktop);
    });
}

void GitMenuPlugin::initialize(dfmext::DFMExtMenuProxy *proxy)
{
    m_proxy = proxy;
}

bool GitMenuPlugin::buildNormalMenu(dfmext::DFMExtMenu *main, const std::string &currentPath,
                                    const std::string &focusPath, const std::list<std::string> &pathList, bool onDesktop)
{
    if (onDesktop)
        return false;

    const QString &filePath = QString::fromStdString(focusPath);

    // 检查是否在Git仓库中
    if (!Utils::isInsideRepositoryFile(filePath))
        return false;

    auto rootAction { m_proxy->createAction() };
    rootAction->setText("Git");

    // 创建Git子菜单
    auto gitSubmenu = m_proxy->createMenu();
    rootAction->setMenu(gitSubmenu);

    bool hasValidAction = false;

    // 处理多文件选择
    if (pathList.size() > 1) {
        return buildMultiFileMenu(gitSubmenu, pathList, hasValidAction, main, rootAction);
    }

    // 单文件菜单处理
    const QString statusText = Utils::getFileStatusDescription(filePath);

    // === 文件操作组 ===

    // Git Add
    if (Utils::canAddFile(filePath)) {
        auto addAction = m_proxy->createAction();
        addAction->setText("Git Add");
        addAction->setIcon("vcs-added");
        addAction->setToolTip(QString("Add '%1' to staging area\nCurrent status: %2")
                                      .arg(QFileInfo(filePath).fileName(), statusText)
                                      .toStdString());
        addAction->registerTriggered([this, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleGitAdd(focusPath);
        });
        gitSubmenu->addAction(addAction);
        hasValidAction = true;
    }

    // Git Remove
    if (Utils::canRemoveFile(filePath)) {
        auto removeAction = m_proxy->createAction();
        removeAction->setText("Git Remove");
        removeAction->setIcon("vcs-removed");
        removeAction->setToolTip(QString("Remove '%1' from Git tracking\nCurrent status: %2")
                                         .arg(QFileInfo(filePath).fileName(), statusText)
                                         .toStdString());
        removeAction->registerTriggered([this, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleGitRemove(focusPath);
        });
        gitSubmenu->addAction(removeAction);
        hasValidAction = true;
    }

    // Git Revert
    if (Utils::canRevertFile(filePath)) {
        auto revertAction = m_proxy->createAction();
        revertAction->setText("Git Revert");
        revertAction->setIcon("vcs-update-required");
        revertAction->setToolTip(QString("Discard changes in '%1'\nCurrent status: %2\nWarning: This will permanently discard your changes!")
                                         .arg(QFileInfo(filePath).fileName(), statusText)
                                         .toStdString());
        revertAction->registerTriggered([this, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleGitRevert(focusPath);
        });
        gitSubmenu->addAction(revertAction);
        hasValidAction = true;
    }

    // 添加分隔符（如果有文件操作）
    if (hasValidAction) {
        auto separatorAction = m_proxy->createAction();
        separatorAction->setSeparator(true);
        gitSubmenu->addAction(separatorAction);
    }

    // === 查看操作组 ===

    // Git Diff
    if (Utils::canShowFileDiff(filePath)) {
        auto diffAction = m_proxy->createAction();
        diffAction->setText("Git Diff...");
        diffAction->setIcon("vcs-diff");
        diffAction->setToolTip(QString("View changes in '%1'\nCurrent status: %2")
                                       .arg(QFileInfo(filePath).fileName(), statusText)
                                       .toStdString());
        diffAction->registerTriggered([this, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleGitDiff(focusPath);
        });
        gitSubmenu->addAction(diffAction);
        hasValidAction = true;
    }

    // Git Log (for file)
    if (Utils::canShowFileLog(filePath)) {
        auto logAction = m_proxy->createAction();
        logAction->setText("Git Log...");
        logAction->setIcon("vcs-normal");
        logAction->setToolTip(QString("View commit history for '%1'\nCurrent status: %2")
                                      .arg(QFileInfo(filePath).fileName(), statusText)
                                      .toStdString());
        logAction->registerTriggered([this, currentPath, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            QString repoPath = Utils::repositoryBaseDir(QString::fromStdString(currentPath));
            handleGitLog(repoPath.toStdString(), focusPath);
        });
        gitSubmenu->addAction(logAction);
        hasValidAction = true;
    }

    // Git Blame
    if (Utils::canShowFileBlame(filePath)) {
        auto blameAction = m_proxy->createAction();
        blameAction->setText("Git Blame...");
        blameAction->setIcon("vcs-annotation");
        blameAction->setToolTip(QString("View line-by-line authorship for '%1'\nCurrent status: %2")
                                        .arg(QFileInfo(filePath).fileName(), statusText)
                                        .toStdString());
        blameAction->registerTriggered([this, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleGitBlame(focusPath);
        });
        gitSubmenu->addAction(blameAction);
        hasValidAction = true;
    }

    if (hasValidAction) {
        main->addAction(rootAction);
        return true;
    }

    return false;
}

bool GitMenuPlugin::buildEmptyAreaMenu(dfmext::DFMExtMenu *main, const std::string &currentPath, bool onDesktop)
{
    if (onDesktop)
        return false;

    const QString &dirPath = QString::fromStdString(currentPath);
    if (!Utils::isInsideRepositoryDir(dirPath))
        return false;

    const QString &repositoryPath = Utils::repositoryBaseDir(dirPath);
    if (repositoryPath.isEmpty())
        return false;

    // 获取当前分支信息
    const QString branchName = Utils::getBranchName(repositoryPath);

    // === 查看操作组 ===

    // Git Log (for repository)
    auto repoLogAction = m_proxy->createAction();
    repoLogAction->setText("Git Log...");
    repoLogAction->setIcon("vcs-normal");
    repoLogAction->setToolTip(QString("View repository commit history\nCurrent branch: %1").arg(branchName).toStdString());
    repoLogAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitLog(repositoryPath.toStdString());
    });
    main->addAction(repoLogAction);

    // Git Status
    auto statusAction = m_proxy->createAction();
    statusAction->setText("Git Status...");
    statusAction->setIcon("vcs-status");
    statusAction->setToolTip(QString("View repository status and pending changes\nCurrent branch: %1").arg(branchName).toStdString());
    statusAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitStatus(repositoryPath.toStdString());
    });
    main->addAction(statusAction);

    auto separator1 = m_proxy->createAction();
    separator1->setSeparator(true);
    main->addAction(separator1);

    // === 分支操作组 ===

    // Git Checkout
    auto checkoutAction = m_proxy->createAction();
    checkoutAction->setText("Git Checkout...");
    checkoutAction->setIcon("vcs-branch");
    checkoutAction->setToolTip(QString("Switch branches or create new branch\nCurrent branch: %1").arg(branchName).toStdString());
    checkoutAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitCheckout(repositoryPath.toStdString());
    });
    main->addAction(checkoutAction);

    auto separator2 = m_proxy->createAction();
    separator2->setSeparator(true);
    main->addAction(separator2);

    // === 同步操作组 ===

    // Git Pull
    auto pullAction = m_proxy->createAction();
    pullAction->setText("Git Pull...");
    pullAction->setIcon("vcs-pull");
    pullAction->setToolTip(QString("Pull latest changes from remote repository\nCurrent branch: %1").arg(branchName).toStdString());
    pullAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitPull(repositoryPath.toStdString());
    });
    main->addAction(pullAction);

    // Git Push
    auto pushAction = m_proxy->createAction();
    pushAction->setText("Git Push...");
    pushAction->setIcon("vcs-push");
    pushAction->setToolTip(QString("Push local commits to remote repository\nCurrent branch: %1").arg(branchName).toStdString());
    pushAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitPush(repositoryPath.toStdString());
    });
    main->addAction(pushAction);

    auto separator3 = m_proxy->createAction();
    separator3->setSeparator(true);
    main->addAction(separator3);

    // === 提交操作组 ===

    // Git Commit
    auto commitAction = m_proxy->createAction();
    commitAction->setText("Git Commit...");
    commitAction->setIcon("vcs-commit");
    commitAction->setToolTip(QString("Commit staged changes to repository\nCurrent branch: %1").arg(branchName).toStdString());
    commitAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitCommit(repositoryPath.toStdString());
    });
    main->addAction(commitAction);

    return true;
}

// ============================================================================
// Git操作处理函数实现
// ============================================================================

void GitMenuPlugin::handleGitAdd(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);

    // 使用GitCommandExecutor解析仓库路径
    GitCommandExecutor executor;
    const QString &repoPath = executor.resolveRepositoryPath(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitAdd] Repository path not found for file:" << file;
        return;
    }

    // 计算相对路径
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitAdd] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitMenuPlugin::handleGitAdd] Adding file:" << file
            << "relative path:" << relativePath;

    QStringList args { "add", relativePath };
    executeSilentGitOperation("Add", repoPath, args);
}

void GitMenuPlugin::handleGitRemove(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);

    // 使用GitCommandExecutor解析仓库路径
    GitCommandExecutor executor;
    const QString &repoPath = executor.resolveRepositoryPath(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitRemove] Repository path not found for file:" << file;
        return;
    }

    // 计算相对路径
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitRemove] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitMenuPlugin::handleGitRemove] Removing file:" << file
            << "relative path:" << relativePath;

    // 根据文件状态选择合适的删除命令
    auto status = Utils::getFileGitStatus(file);
    QStringList args;

    using Global::ItemVersion;
    if (status == ItemVersion::AddedVersion) {
        // 如果是新添加的文件，使用 reset 来取消暂存
        args << "reset"
             << "HEAD" << relativePath;
    } else if (status == ItemVersion::NormalVersion || status == ItemVersion::LocallyModifiedVersion || status == ItemVersion::LocallyModifiedUnstagedVersion) {
        // 对于已跟踪的文件，使用 rm --cached 从暂存区删除但保留工作目录文件
        args << "rm"
             << "--cached" << relativePath;
    } else {
        // 其他情况使用默认的 rm --cached
        args << "rm"
             << "--cached" << relativePath;
    }

    executeSilentGitOperation("Remove", repoPath, args);
}

void GitMenuPlugin::handleGitRevert(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);

    // 使用GitCommandExecutor解析仓库路径
    GitCommandExecutor executor;
    const QString &repoPath = executor.resolveRepositoryPath(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitRevert] Repository path not found for file:" << file;
        return;
    }

    // 计算相对路径
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitRevert] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitMenuPlugin::handleGitRevert] Reverting file:" << file
            << "relative path:" << relativePath;

    // 根据文件状态选择合适的还原命令
    auto status = Utils::getFileGitStatus(file);
    QStringList args;

    using Global::ItemVersion;
    if (status == ItemVersion::LocallyModifiedUnstagedVersion) {
        // 对于工作目录中的修改，使用 restore 命令（Git 2.23+）或 checkout
        args << "restore" << relativePath;
    } else if (status == ItemVersion::LocallyModifiedVersion) {
        // 对于暂存区的修改，先取消暂存再还原
        args << "restore"
             << "--staged"
             << "--worktree" << relativePath;
    } else if (status == ItemVersion::AddedVersion) {
        // 对于新添加的文件，使用 reset 取消暂存
        args << "reset"
             << "HEAD" << relativePath;
    } else {
        // 默认使用 checkout 命令（兼容旧版本Git）
        args << "checkout"
             << "HEAD"
             << "--" << relativePath;
    }

    executeSilentGitOperation("Revert", repoPath, args);
}

void GitMenuPlugin::handleGitLog(const std::string &repositoryPath, const std::string &filePath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);
    const QString &file = QString::fromStdString(filePath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitLog] Opening log for repository:" << repoPath
            << "file:" << (file.isEmpty() ? "all" : file);

    // 检查应用程序状态
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitMenuPlugin::handleGitLog] No QApplication instance found";
        return;
    }

    // 获取合适的父窗口
    QWidget *parentWidget = QApplication::activeWindow();

    auto *logDialog = new GitLogDialog(repoPath, file, parentWidget);
    logDialog->setAttribute(Qt::WA_DeleteOnClose);
    logDialog->show();
}

void GitMenuPlugin::handleGitCheckout(const std::string &repositoryPath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitCheckout] Opening checkout dialog for:" << repoPath;

    // 检查应用程序状态
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitMenuPlugin::handleGitCheckout] No QApplication instance found";
        return;
    }

    // 获取合适的父窗口
    QWidget *parentWidget = QApplication::activeWindow();

    auto *checkoutDialog = new GitCheckoutDialog(repoPath, parentWidget);
    checkoutDialog->setAttribute(Qt::WA_DeleteOnClose);

    if (checkoutDialog->exec() == QDialog::Accepted) {
        refreshFileManager();
    }
}

void GitMenuPlugin::handleGitPush(const std::string &repositoryPath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitPush] Pushing repository:" << repoPath;

    QStringList args { "push" };
    executeGitOperation("Push", repoPath, args);
}

void GitMenuPlugin::handleGitPull(const std::string &repositoryPath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitPull] Pulling repository:" << repoPath;

    QStringList args { "pull" };
    executeGitOperation("Pull", repoPath, args);
}

void GitMenuPlugin::handleGitCommit(const std::string &repositoryPath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitCommit] Opening commit dialog for:" << repoPath;

    // 检查应用程序状态
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitMenuPlugin::handleGitCommit] No QApplication instance found";
        return;
    }

    // 获取合适的父窗口
    QWidget *parentWidget = QApplication::activeWindow();

    auto *commitDialog = new GitCommitDialog(repoPath, QStringList(), parentWidget);
    commitDialog->setAttribute(Qt::WA_DeleteOnClose);

    if (commitDialog->exec() == QDialog::Accepted) {
        refreshFileManager();
    }
}

void GitMenuPlugin::executeGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments)
{
    qInfo() << "INFO: [GitMenuPlugin::executeGitOperation] Executing git" << arguments.join(' ')
            << "in directory:" << workingDir;

    // 检查应用程序状态
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitMenuPlugin::executeGitOperation] No QApplication instance found";
        return;
    }

    // 判断是否为静默操作（基础文件操作成功时不弹窗）
    bool isSilentOperation = (operation == "Add" || operation == "Remove" || operation == "Revert");
    
    if (isSilentOperation) {
        // 对于基础操作，使用同步执行，成功时静默，失败时弹窗
        executeSilentGitOperation(operation, workingDir, arguments);
    } else {
        // 对于复杂操作（Push/Pull/Commit等），显示进度对话框
        executeInteractiveGitOperation(operation, workingDir, arguments);
    }
}

void GitMenuPlugin::executeSilentGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments)
{
    // 显示简短的状态提示（可选：可以通过系统通知或状态栏显示）
    qInfo() << "INFO: [GitMenuPlugin::executeSilentGitOperation] Starting" << operation << "operation...";
    
    GitCommandExecutor executor;
    GitCommandExecutor::GitCommand cmd;
    cmd.command = operation;
    cmd.arguments = arguments;
    cmd.workingDirectory = workingDir;
    cmd.timeout = 10000; // 10秒超时

    QString output, error;
    GitCommandExecutor::Result result = executor.executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        // 成功时静默执行，只记录日志和刷新文件管理器
        qInfo() << "INFO: [GitMenuPlugin::executeSilentGitOperation] Operation" << operation << "completed successfully";
        refreshFileManager();
        
        // 可选：显示简短的成功提示（通过系统通知，不阻塞用户）
        // 这里可以集成系统通知API，比如libnotify（Linux）
        showSuccessNotification(operation);
    } else {
        // 失败时使用交互式对话框显示错误信息（复用现有代码）
        qWarning() << "WARNING: [GitMenuPlugin::executeSilentGitOperation] Operation" << operation << "failed, showing interactive dialog";
        executeInteractiveGitOperation(operation, workingDir, arguments);
    }
}

void GitMenuPlugin::showSuccessNotification(const QString &operation)
{
    // 这里可以实现系统通知，目前只记录日志
    // 在实际部署时，可以集成libnotify或其他系统通知API
    qInfo() << "INFO: [GitMenuPlugin::showSuccessNotification] Git" << operation << "operation completed successfully";
    
    // TODO: 集成系统通知
    // 示例代码（需要libnotify支持）：
    // notify_notification_new("Git Operation", 
    //                        QString("Git %1 completed successfully").arg(operation).toUtf8().data(),
    //                        "vcs-normal");
}

void GitMenuPlugin::executeInteractiveGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments)
{
    // 获取当前活动窗口作为父窗口，避免崩溃
    QWidget *parentWidget = QApplication::activeWindow();
    if (!parentWidget) {
        // 如果没有活动窗口，尝试获取主窗口
        auto widgets = QApplication::topLevelWidgets();
        for (auto *widget : widgets) {
            if (widget->isVisible() && widget->windowType() == Qt::Window) {
                parentWidget = widget;
                break;
            }
        }
    }

    auto *opDialog = new GitOperationDialog(operation, parentWidget);
    opDialog->setAttribute(Qt::WA_DeleteOnClose);
    
    // 设置操作描述
    QString description = QObject::tr("Preparing to execute %1 operation in repository").arg(operation);
    opDialog->setOperationDescription(description);
    
    // 异步执行命令
    opDialog->executeCommand(workingDir, arguments);
    
    // 显示对话框
    opDialog->show();
    
    // 连接完成信号以便在成功时刷新
    QObject::connect(opDialog, &QDialog::accepted, [this, opDialog]() {
        if (opDialog->getExecutionResult() == GitCommandExecutor::Result::Success) {
            refreshFileManager();
        }
    });
}

void GitMenuPlugin::refreshFileManager()
{
    // 触发文件管理器刷新以更新Git状态图标
    // 这里可以发送信号通知文件管理器刷新
    qInfo() << "INFO: [GitMenuPlugin::refreshFileManager] Requesting file manager refresh";
}

// ============================================================================
// 多文件菜单处理函数实现
// ============================================================================

bool GitMenuPlugin::buildMultiFileMenu(dfmext::DFMExtMenu *gitSubmenu, const std::list<std::string> &pathList,
                                       bool &hasValidAction, dfmext::DFMExtMenu *main, dfmext::DFMExtAction *rootAction)
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

    // 显示文件数量信息
    const int fileCount = static_cast<int>(pathList.size());
    const QString fileCountText = QObject::tr("%1 files selected").arg(fileCount);

    // === 批量文件操作 ===

    // 批量 Git Add
    if (compatibleOps.contains("add")) {
        auto addAction = m_proxy->createAction();
        addAction->setText(QObject::tr("Git Add Selected").toStdString());
        addAction->setIcon("vcs-added");
        addAction->setToolTip(QObject::tr("Add all selected files to staging area\n%1").arg(fileCountText).toStdString());
        addAction->registerTriggered([this, pathList](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleMultiFileAdd(pathList);
        });
        gitSubmenu->addAction(addAction);
        hasValidAction = true;
    }

    // 批量 Git Remove
    if (compatibleOps.contains("remove")) {
        auto removeAction = m_proxy->createAction();
        removeAction->setText(QObject::tr("Git Remove Selected").toStdString());
        removeAction->setIcon("vcs-removed");
        removeAction->setToolTip(QObject::tr("Remove all selected files from Git tracking\n%1").arg(fileCountText).toStdString());
        removeAction->registerTriggered([this, pathList](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleMultiFileRemove(pathList);
        });
        gitSubmenu->addAction(removeAction);
        hasValidAction = true;
    }

    // 批量 Git Revert
    if (compatibleOps.contains("revert")) {
        auto revertAction = m_proxy->createAction();
        revertAction->setText(QObject::tr("Git Revert Selected").toStdString());
        revertAction->setIcon("vcs-update-required");
        revertAction->setToolTip(QObject::tr("Discard changes in all selected files\n%1\nWarning: This will permanently discard your changes!")
                                         .arg(fileCountText)
                                         .toStdString());
        revertAction->registerTriggered([this, pathList](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleMultiFileRevert(pathList);
        });
        gitSubmenu->addAction(revertAction);
        hasValidAction = true;
    }

    if (hasValidAction) {
        main->addAction(rootAction);
        return true;
    }

    return false;
}

QStringList GitMenuPlugin::getCompatibleOperationsForMultiSelection(const std::list<std::string> &pathList)
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

// ============================================================================
// 新的Git操作处理函数实现
// ============================================================================

void GitMenuPlugin::handleGitDiff(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);

    GitCommandExecutor executor;
    const QString &repoPath = executor.resolveRepositoryPath(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitDiff] Repository path not found for file:" << file;
        return;
    }

    qInfo() << "INFO: [GitMenuPlugin::handleGitDiff] Opening diff dialog for file:" << file;

    // 检查应用程序状态
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitMenuPlugin::handleGitDiff] No QApplication instance found";
        return;
    }

    // 获取合适的父窗口
    QWidget *parentWidget = QApplication::activeWindow();

    auto *diffDialog = new GitDiffDialog(repoPath, file, parentWidget);
    diffDialog->setAttribute(Qt::WA_DeleteOnClose);
    diffDialog->show();
}

void GitMenuPlugin::handleGitBlame(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);

    GitCommandExecutor executor;
    const QString &repoPath = executor.resolveRepositoryPath(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitBlame] Repository path not found for file:" << file;
        return;
    }

    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitBlame] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitMenuPlugin::handleGitBlame] Showing blame for file:" << file;

    QStringList args { "blame", "--line-porcelain", relativePath };
    executeGitOperation("Blame", repoPath, args);
}

void GitMenuPlugin::handleGitStatus(const std::string &repositoryPath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitStatus] Opening status dialog for repository:" << repoPath;

    // 检查应用程序状态
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitMenuPlugin::handleGitStatus] No QApplication instance found";
        return;
    }

    // 获取合适的父窗口
    QWidget *parentWidget = QApplication::activeWindow();

    auto *statusDialog = new GitStatusDialog(repoPath, parentWidget);
    statusDialog->setAttribute(Qt::WA_DeleteOnClose);
    statusDialog->show();
}

void GitMenuPlugin::handleMultiFileAdd(const std::list<std::string> &pathList)
{
    if (pathList.empty()) return;

    // 使用第一个文件确定仓库路径
    const QString firstFile = QString::fromStdString(*pathList.begin());
    GitCommandExecutor executor;
    const QString repoPath = executor.resolveRepositoryPath(firstFile);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleMultiFileAdd] Repository path not found";
        return;
    }

    QStringList args { "add" };
    
    // 添加所有文件的相对路径
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);
        QString relativePath = executor.makeRelativePath(repoPath, filePath);
        if (!relativePath.isEmpty()) {
            args << relativePath;
        }
    }

    if (args.size() > 1) {   // 确保有文件被添加
        qInfo() << "INFO: [GitMenuPlugin::handleMultiFileAdd] Adding" << (args.size() - 1) << "files";
        executeSilentGitOperation("Add", repoPath, args);
    }
}

void GitMenuPlugin::handleMultiFileRemove(const std::list<std::string> &pathList)
{
    if (pathList.empty()) return;

    const QString firstFile = QString::fromStdString(*pathList.begin());
    GitCommandExecutor executor;
    const QString repoPath = executor.resolveRepositoryPath(firstFile);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleMultiFileRemove] Repository path not found";
        return;
    }

    QStringList args { "rm", "--cached" };
    
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);
        QString relativePath = executor.makeRelativePath(repoPath, filePath);
        if (!relativePath.isEmpty()) {
            args << relativePath;
        }
    }

    if (args.size() > 2) {
        qInfo() << "INFO: [GitMenuPlugin::handleMultiFileRemove] Removing" << (args.size() - 2) << "files";
        executeSilentGitOperation("Remove", repoPath, args);
    }
}

void GitMenuPlugin::handleMultiFileRevert(const std::list<std::string> &pathList)
{
    if (pathList.empty()) return;

    const QString firstFile = QString::fromStdString(*pathList.begin());
    GitCommandExecutor executor;
    const QString repoPath = executor.resolveRepositoryPath(firstFile);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleMultiFileRevert] Repository path not found";
        return;
    }

    QStringList args { "restore", "--staged", "--worktree" };
    
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);
        QString relativePath = executor.makeRelativePath(repoPath, filePath);
        if (!relativePath.isEmpty()) {
            args << relativePath;
        }
    }

    if (args.size() > 3) {
        qInfo() << "INFO: [GitMenuPlugin::handleMultiFileRevert] Reverting" << (args.size() - 3) << "files";
        executeSilentGitOperation("Revert", repoPath, args);
    }
}
