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

    // 如果选择了多个文件，暂时不处理
    if (pathList.size() > 1)
        return false;

    auto rootAction { m_proxy->createAction() };
    rootAction->setText("Git");

    // 创建Git子菜单
    auto gitSubmenu = m_proxy->createMenu();
    rootAction->setMenu(gitSubmenu);

    bool hasValidAction = false;

    // Git Add
    if (Utils::canAddFile(filePath)) {
        auto addAction = m_proxy->createAction();
        addAction->setText("Git Add");
        addAction->setIcon("vcs-added");
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
        revertAction->registerTriggered([this, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            handleGitRevert(focusPath);
        });
        gitSubmenu->addAction(revertAction);
        hasValidAction = true;
    }

    // Git Log (for file)
    if (Utils::canShowFileLog(filePath)) {
        if (hasValidAction) {
            auto separatorAction = m_proxy->createAction();
            separatorAction->setSeparator(true);
            gitSubmenu->addAction(separatorAction);
        }

        auto logAction = m_proxy->createAction();
        logAction->setText("Git Log...");
        logAction->setIcon("vcs-normal");
        logAction->registerTriggered([this, currentPath, focusPath](DFMExtAction *action, bool checked) {
            Q_UNUSED(action)
            Q_UNUSED(checked)
            QString repoPath = Utils::repositoryBaseDir(QString::fromStdString(currentPath));
            handleGitLog(repoPath.toStdString(), focusPath);
        });
        gitSubmenu->addAction(logAction);
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

    // Git Log (for repository)
    auto repoLogAction = m_proxy->createAction();
    repoLogAction->setText("Git Log...");
    repoLogAction->setIcon("vcs-normal");
    repoLogAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitLog(repositoryPath.toStdString());
    });
    main->addAction(repoLogAction);

    auto separator1 = m_proxy->createAction();
    separator1->setSeparator(true);
    main->addAction(separator1);

    // Git Checkout
    auto checkoutAction = m_proxy->createAction();
    checkoutAction->setText("Git Checkout...");
    checkoutAction->setIcon("vcs-update-required");
    checkoutAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitCheckout(repositoryPath.toStdString());
    });
    main->addAction(checkoutAction);

    auto separator2 = m_proxy->createAction();
    separator2->setSeparator(true);
    main->addAction(separator2);

    // Git Pull
    auto pullAction = m_proxy->createAction();
    pullAction->setText("Git Pull...");
    pullAction->setIcon("vcs-update-required");
    pullAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitPull(repositoryPath.toStdString());
    });
    main->addAction(pullAction);

    // Git Push
    auto pushAction = m_proxy->createAction();
    pushAction->setText("Git Push...");
    pushAction->setIcon("vcs-normal");
    pushAction->registerTriggered([this, repositoryPath](DFMExtAction *action, bool checked) {
        Q_UNUSED(action)
        Q_UNUSED(checked)
        handleGitPush(repositoryPath.toStdString());
    });
    main->addAction(pushAction);

    auto separator3 = m_proxy->createAction();
    separator3->setSeparator(true);
    main->addAction(separator3);

    // Git Commit
    auto commitAction = m_proxy->createAction();
    commitAction->setText("Git Commit...");
    commitAction->setIcon("vcs-added");
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
    const QString &repoPath = Utils::repositoryBaseDir(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitAdd] Repository path not found for file:" << file;
        return;
    }

    // 计算相对于仓库根目录的路径
    QString relativePath = QDir(repoPath).relativeFilePath(file);
    
    qInfo() << "INFO: [GitMenuPlugin::handleGitAdd] Adding file:" << file 
            << "relative path:" << relativePath;

    QStringList args { "add", relativePath };
    executeGitOperation("Add", repoPath, args);
}

void GitMenuPlugin::handleGitRemove(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);
    const QString &repoPath = Utils::repositoryBaseDir(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitRemove] Repository path not found for file:" << file;
        return;
    }

    // 计算相对于仓库根目录的路径
    QString relativePath = QDir(repoPath).relativeFilePath(file);
    
    qInfo() << "INFO: [GitMenuPlugin::handleGitRemove] Removing file:" << file
            << "relative path:" << relativePath;

    // 根据文件状态选择合适的删除命令
    auto status = Utils::getFileGitStatus(file);
    QStringList args;
    
    using Global::ItemVersion;
    if (status == ItemVersion::AddedVersion) {
        // 如果是新添加的文件，使用 reset 来取消暂存
        args << "reset" << "HEAD" << relativePath;
    } else if (status == ItemVersion::NormalVersion || 
               status == ItemVersion::LocallyModifiedVersion ||
               status == ItemVersion::LocallyModifiedUnstagedVersion) {
        // 对于已跟踪的文件，使用 rm --cached 从暂存区删除但保留工作目录文件
        args << "rm" << "--cached" << relativePath;
    } else {
        // 其他情况使用默认的 rm --cached
        args << "rm" << "--cached" << relativePath;
    }
    
    executeGitOperation("Remove", repoPath, args);
}

void GitMenuPlugin::handleGitRevert(const std::string &filePath)
{
    const QString &file = QString::fromStdString(filePath);
    const QString &repoPath = Utils::repositoryBaseDir(file);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitMenuPlugin::handleGitRevert] Repository path not found for file:" << file;
        return;
    }

    // 计算相对于仓库根目录的路径
    QString relativePath = QDir(repoPath).relativeFilePath(file);
    
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
        args << "restore" << "--staged" << "--worktree" << relativePath;
    } else if (status == ItemVersion::AddedVersion) {
        // 对于新添加的文件，使用 reset 取消暂存
        args << "reset" << "HEAD" << relativePath;
    } else {
        // 默认使用 checkout 命令（兼容旧版本Git）
        args << "checkout" << "HEAD" << "--" << relativePath;
    }
    
    executeGitOperation("Revert", repoPath, args);
}

void GitMenuPlugin::handleGitLog(const std::string &repositoryPath, const std::string &filePath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);
    const QString &file = QString::fromStdString(filePath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitLog] Opening log for repository:" << repoPath
            << "file:" << (file.isEmpty() ? "all" : file);

    auto *logDialog = new GitLogDialog(repoPath, file);
    logDialog->setAttribute(Qt::WA_DeleteOnClose);
    logDialog->show();
}

void GitMenuPlugin::handleGitCheckout(const std::string &repositoryPath)
{
    const QString &repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitMenuPlugin::handleGitCheckout] Opening checkout dialog for:" << repoPath;

    auto *checkoutDialog = new GitCheckoutDialog(repoPath);
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

    auto *commitDialog = new GitCommitDialog(repoPath, QStringList());
    commitDialog->setAttribute(Qt::WA_DeleteOnClose);

    if (commitDialog->exec() == QDialog::Accepted) {
        refreshFileManager();
    }
}

void GitMenuPlugin::executeGitOperation(const QString &operation, const QString &workingDir, const QStringList &arguments)
{
    qInfo() << "INFO: [GitMenuPlugin::executeGitOperation] Executing git" << arguments.join(' ')
            << "in directory:" << workingDir;

    auto *opDialog = new GitOperationDialog(operation);
    opDialog->setAttribute(Qt::WA_DeleteOnClose);
    opDialog->executeCommand(workingDir, arguments);

    if (opDialog->exec() == QDialog::Accepted) {
        refreshFileManager();
    }
}

void GitMenuPlugin::refreshFileManager()
{
    // 触发文件管理器刷新以更新Git状态图标
    // 这里可以发送信号通知文件管理器刷新
    qInfo() << "INFO: [GitMenuPlugin::refreshFileManager] Requesting file manager refresh";
}
