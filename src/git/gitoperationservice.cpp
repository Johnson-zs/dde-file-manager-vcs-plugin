#include "gitoperationservice.h"
#include "dialogs/gitdialogs.h"
#include "dialogs/gitoperationdialog.h"
#include "utils.h"

#include <QApplication>
#include <QWidget>
#include <QFileInfo>
#include <QDir>
#include <QDialog>

GitOperationService::GitOperationService(QObject *parent)
    : QObject(parent)
    , m_executor(new GitCommandExecutor(this))
{
    // 连接GitCommandExecutor信号
    connect(m_executor, &GitCommandExecutor::commandFinished,
            this, &GitOperationService::onCommandFinished);
}

GitOperationService::~GitOperationService() = default;

// ============================================================================
// 文件操作实现
// ============================================================================

void GitOperationService::addFile(const std::string &filePath)
{
    const QString file = QString::fromStdString(filePath);
    const QString repoPath = resolveRepositoryPath(filePath);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::addFile] Repository path not found for file:" << file;
        return;
    }

    GitCommandExecutor executor;
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::addFile] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitOperationService::addFile] Adding file:" << file << "relative path:" << relativePath;

    QStringList args { "add", relativePath };
    executeSilentOperation("Add", repoPath, args);
}

void GitOperationService::removeFile(const std::string &filePath)
{
    const QString file = QString::fromStdString(filePath);
    const QString repoPath = resolveRepositoryPath(filePath);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::removeFile] Repository path not found for file:" << file;
        return;
    }

    GitCommandExecutor executor;
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::removeFile] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitOperationService::removeFile] Removing file:" << file << "relative path:" << relativePath;

    // 根据文件状态选择合适的删除命令
    auto status = Utils::getFileGitStatus(file);
    QStringList args;

    using Global::ItemVersion;
    if (status == ItemVersion::AddedVersion) {
        args << "reset" << "HEAD" << relativePath;
    } else {
        args << "rm" << "--cached" << relativePath;
    }

    executeSilentOperation("Remove", repoPath, args);
}

void GitOperationService::revertFile(const std::string &filePath)
{
    const QString file = QString::fromStdString(filePath);
    const QString repoPath = resolveRepositoryPath(filePath);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::revertFile] Repository path not found for file:" << file;
        return;
    }

    GitCommandExecutor executor;
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::revertFile] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitOperationService::revertFile] Reverting file:" << file << "relative path:" << relativePath;

    // 根据文件状态选择合适的还原命令
    auto status = Utils::getFileGitStatus(file);
    QStringList args;

    using Global::ItemVersion;
    if (status == ItemVersion::LocallyModifiedUnstagedVersion) {
        args << "restore" << relativePath;
    } else if (status == ItemVersion::LocallyModifiedVersion) {
        args << "restore" << "--staged" << "--worktree" << relativePath;
    } else if (status == ItemVersion::AddedVersion) {
        args << "reset" << "HEAD" << relativePath;
    } else {
        args << "checkout" << "HEAD" << "--" << relativePath;
    }

    executeSilentOperation("Revert", repoPath, args);
}

void GitOperationService::showFileDiff(const std::string &filePath)
{
    const QString file = QString::fromStdString(filePath);
    const QString repoPath = resolveRepositoryPath(filePath);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::showFileDiff] Repository path not found for file:" << file;
        return;
    }

    qInfo() << "INFO: [GitOperationService::showFileDiff] Opening diff dialog for file:" << file;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::showFileDiff] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showDiffDialog(repoPath, file, parentWidget);
}

void GitOperationService::showFileBlame(const std::string &filePath)
{
    const QString file = QString::fromStdString(filePath);
    const QString repoPath = resolveRepositoryPath(filePath);

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::showFileBlame] Repository path not found for file:" << file;
        return;
    }

    GitCommandExecutor executor;
    QString relativePath = executor.makeRelativePath(repoPath, file);
    if (relativePath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::showFileBlame] Failed to calculate relative path for:" << file;
        return;
    }

    qInfo() << "INFO: [GitOperationService::showFileBlame] Opening enhanced blame dialog for file:" << file;

    // Use GitDialogManager to show blame dialog
    GitDialogManager::instance()->showBlameDialog(repoPath, file, QApplication::activeWindow());
}

void GitOperationService::showFileLog(const std::string &repositoryPath, const std::string &filePath)
{
    const QString repoPath = QString::fromStdString(repositoryPath);
    const QString file = QString::fromStdString(filePath);

    qInfo() << "INFO: [GitOperationService::showFileLog] Opening log for repository:" << repoPath
            << "file:" << (file.isEmpty() ? "all" : file);

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::showFileLog] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    if (file.isEmpty()) {
        GitDialogManager::instance()->showLogDialog(repoPath, parentWidget);
    } else {
        GitDialogManager::instance()->showLogDialog(repoPath, file, parentWidget);
    }
}

// ============================================================================
// 批量文件操作实现
// ============================================================================

void GitOperationService::addMultipleFiles(const std::list<std::string> &pathList)
{
    if (pathList.empty()) return;

    const QString firstFile = QString::fromStdString(*pathList.begin());
    const QString repoPath = resolveRepositoryPath(*pathList.begin());

    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::addMultipleFiles] Repository path not found";
        return;
    }

    QStringList args = buildFileArguments("add", repoPath, pathList);
    if (args.size() > 1) {
        qInfo() << "INFO: [GitOperationService::addMultipleFiles] Adding" << (args.size() - 1) << "files";
        executeSilentOperation("Add", repoPath, args);
    }
}

void GitOperationService::removeMultipleFiles(const std::list<std::string> &pathList)
{
    if (pathList.empty()) return;

    const QString repoPath = resolveRepositoryPath(*pathList.begin());
    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::removeMultipleFiles] Repository path not found";
        return;
    }

    QStringList args { "rm", "--cached" };
    GitCommandExecutor executor;
    
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);
        QString relativePath = executor.makeRelativePath(repoPath, filePath);
        if (!relativePath.isEmpty()) {
            args << relativePath;
        }
    }

    if (args.size() > 2) {
        qInfo() << "INFO: [GitOperationService::removeMultipleFiles] Removing" << (args.size() - 2) << "files";
        executeSilentOperation("Remove", repoPath, args);
    }
}

void GitOperationService::revertMultipleFiles(const std::list<std::string> &pathList)
{
    if (pathList.empty()) return;

    const QString repoPath = resolveRepositoryPath(*pathList.begin());
    if (repoPath.isEmpty()) {
        qWarning() << "ERROR: [GitOperationService::revertMultipleFiles] Repository path not found";
        return;
    }

    QStringList args { "restore", "--staged", "--worktree" };
    GitCommandExecutor executor;
    
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);
        QString relativePath = executor.makeRelativePath(repoPath, filePath);
        if (!relativePath.isEmpty()) {
            args << relativePath;
        }
    }

    if (args.size() > 3) {
        qInfo() << "INFO: [GitOperationService::revertMultipleFiles] Reverting" << (args.size() - 3) << "files";
        executeSilentOperation("Revert", repoPath, args);
    }
}

// ============================================================================
// 仓库操作实现
// ============================================================================

void GitOperationService::showRepositoryStatus(const std::string &repositoryPath)
{
    const QString repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitOperationService::showRepositoryStatus] Opening status dialog for repository:" << repoPath;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::showRepositoryStatus] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showStatusDialog(repoPath, parentWidget);
}

void GitOperationService::checkoutBranch(const std::string &repositoryPath)
{
    const QString repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitOperationService::checkoutBranch] Opening checkout dialog for:" << repoPath;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::checkoutBranch] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showCheckoutDialog(repoPath, parentWidget);
    
    // Note: The refresh signal should be emitted when the checkout operation actually completes
    // This will need to be connected through the dialog's success signal in the future
}

void GitOperationService::pushRepository(const std::string &repositoryPath)
{
    const QString repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitOperationService::pushRepository] Pushing repository:" << repoPath;

    QStringList args { "push" };
    executeInteractiveOperation("Push", repoPath, args);
}

void GitOperationService::pullRepository(const std::string &repositoryPath)
{
    const QString repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitOperationService::pullRepository] Pulling repository:" << repoPath;

    QStringList args { "pull" };
    executeInteractiveOperation("Pull", repoPath, args);
}

void GitOperationService::commitChanges(const std::string &repositoryPath)
{
    const QString repoPath = QString::fromStdString(repositoryPath);

    qInfo() << "INFO: [GitOperationService::commitChanges] Opening commit dialog for:" << repoPath;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::commitChanges] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showCommitDialog(repoPath, parentWidget);
    
    // Note: The refresh signal should be emitted when the commit operation actually completes
    // This will need to be connected through the dialog's success signal in the future
}

// ============================================================================
// 高级Push/Pull操作实现
// ============================================================================

void GitOperationService::pushWithOptions(const QString &repositoryPath, const QString &remoteName, 
                                         const QString &localBranch, const QString &remoteBranch,
                                         bool forceWithLease, bool pushTags, 
                                         bool setUpstream, bool dryRun)
{
    qInfo() << "INFO: [GitOperationService::pushWithOptions] Pushing with options:"
            << "remote:" << remoteName << "local:" << localBranch << "remote branch:" << remoteBranch
            << "force:" << forceWithLease << "tags:" << pushTags << "upstream:" << setUpstream << "dry-run:" << dryRun;

    QStringList args;
    args << "push";
    
    if (dryRun) {
        args << "--dry-run";
    }
    
    if (forceWithLease) {
        args << "--force-with-lease";
    }
    
    if (pushTags) {
        args << "--tags";
    }
    
    if (setUpstream) {
        args << "-u";
    }
    
    // 添加远程和分支
    args << remoteName;
    if (!remoteBranch.isEmpty()) {
        args << QString("%1:%2").arg(localBranch, remoteBranch);
    } else {
        args << localBranch;
    }
    
    executeInteractiveOperation("Push", repositoryPath, args);
}

void GitOperationService::pullWithOptions(const QString &repositoryPath, const QString &remoteName,
                                         const QString &remoteBranch, const QString &strategy,
                                         bool prune, bool autoStash, bool dryRun)
{
    qInfo() << "INFO: [GitOperationService::pullWithOptions] Pulling with options:"
            << "remote:" << remoteName << "branch:" << remoteBranch << "strategy:" << strategy
            << "prune:" << prune << "auto-stash:" << autoStash << "dry-run:" << dryRun;

    QStringList args;
    args << "pull";
    
    if (dryRun) {
        args << "--dry-run";
    }
    
    if (prune) {
        args << "--prune";
    }
    
    if (autoStash) {
        args << "--autostash";
    }
    
    // 合并策略
    if (strategy == "rebase") {
        args << "--rebase";
    } else if (strategy == "ff-only") {
        args << "--ff-only";
    }
    
    // 添加远程和分支
    args << remoteName;
    if (!remoteBranch.isEmpty()) {
        args << remoteBranch;
    }
    
    executeInteractiveOperation("Pull", repositoryPath, args);
}

void GitOperationService::showAdvancedPushDialog(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitOperationService::showAdvancedPushDialog] Opening advanced push dialog for:" << repositoryPath;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::showAdvancedPushDialog] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showPushDialog(repositoryPath, parentWidget);
}

void GitOperationService::showAdvancedPullDialog(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitOperationService::showAdvancedPullDialog] Opening advanced pull dialog for:" << repositoryPath;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::showAdvancedPullDialog] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showPullDialog(repositoryPath, parentWidget);
}

void GitOperationService::showRemoteManager(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitOperationService::showRemoteManager] Opening remote manager for:" << repositoryPath;

    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::showRemoteManager] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    GitDialogManager::instance()->showRemoteManager(repositoryPath, parentWidget);
}

// ============================================================================
// 远程仓库管理实现
// ============================================================================

void GitOperationService::addRemote(const QString &repositoryPath, const QString &name, const QString &url)
{
    qInfo() << "INFO: [GitOperationService::addRemote] Adding remote:" << name << "url:" << url;

    QStringList args;
    args << "remote" << "add" << name << url;
    
    executeInteractiveOperation("Add Remote", repositoryPath, args);
}

void GitOperationService::removeRemote(const QString &repositoryPath, const QString &name)
{
    qInfo() << "INFO: [GitOperationService::removeRemote] Removing remote:" << name;

    QStringList args;
    args << "remote" << "remove" << name;
    
    executeInteractiveOperation("Remove Remote", repositoryPath, args);
}

void GitOperationService::renameRemote(const QString &repositoryPath, const QString &oldName, const QString &newName)
{
    qInfo() << "INFO: [GitOperationService::renameRemote] Renaming remote from:" << oldName << "to:" << newName;

    QStringList args;
    args << "remote" << "rename" << oldName << newName;
    
    executeInteractiveOperation("Rename Remote", repositoryPath, args);
}

void GitOperationService::setRemoteUrl(const QString &repositoryPath, const QString &name, const QString &url)
{
    qInfo() << "INFO: [GitOperationService::setRemoteUrl] Setting remote URL for:" << name << "to:" << url;

    QStringList args;
    args << "remote" << "set-url" << name << url;
    
    executeInteractiveOperation("Set Remote URL", repositoryPath, args);
}

bool GitOperationService::testRemoteConnection(const QString &repositoryPath, const QString &remoteName)
{
    qInfo() << "INFO: [GitOperationService::testRemoteConnection] Testing connection to remote:" << remoteName;

    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "ls-remote";
    cmd.arguments = QStringList() << "ls-remote" << "--heads" << remoteName;
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 10000; // 10秒超时
    
    auto result = executor.executeCommand(cmd, output, error);
    
    bool success = (result == GitCommandExecutor::Result::Success);
    
    if (success) {
        qInfo() << "INFO: [GitOperationService::testRemoteConnection] Remote connection successful";
    } else {
        qWarning() << "WARNING: [GitOperationService::testRemoteConnection] Remote connection failed:" << error;
    }
    
    return success;
}

// ============================================================================
// 分支和状态查询实现
// ============================================================================

QStringList GitOperationService::getRemotes(const QString &repositoryPath)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "remote";
    cmd.arguments = QStringList() << "remote";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        return output.split('\n', Qt::SkipEmptyParts);
    }
    
    qWarning() << "WARNING: [GitOperationService::getRemotes] Failed to get remotes:" << error;
    return QStringList();
}

QStringList GitOperationService::getLocalBranches(const QString &repositoryPath)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "branch";
    cmd.arguments = QStringList() << "branch";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        QStringList branches;
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            QString branchName = line.trimmed();
            if (branchName.startsWith("* ")) {
                branchName = branchName.mid(2);
            }
            branches.append(branchName);
        }
        
        return branches;
    }
    
    qWarning() << "WARNING: [GitOperationService::getLocalBranches] Failed to get local branches:" << error;
    return QStringList();
}

QStringList GitOperationService::getRemoteBranches(const QString &repositoryPath, const QString &remoteName)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "branch";
    cmd.arguments = QStringList() << "branch" << "-r";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        QStringList branches;
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            QString branchName = line.trimmed();
            if (remoteName.isEmpty() || branchName.startsWith(remoteName + "/")) {
                if (!remoteName.isEmpty()) {
                    branchName = branchName.mid(remoteName.length() + 1);
                }
                if (branchName != "HEAD") {
                    branches.append(branchName);
                }
            }
        }
        
        return branches;
    }
    
    qWarning() << "WARNING: [GitOperationService::getRemoteBranches] Failed to get remote branches:" << error;
    return QStringList();
}

QString GitOperationService::getCurrentBranch(const QString &repositoryPath)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "branch";
    cmd.arguments = QStringList() << "branch" << "--show-current";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        return output.trimmed();
    }
    
    qWarning() << "WARNING: [GitOperationService::getCurrentBranch] Failed to get current branch:" << error;
    return QString();
}

QStringList GitOperationService::getUnpushedCommits(const QString &repositoryPath, const QString &remoteBranch)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "log";
    cmd.arguments = QStringList() << "log" << "--oneline" << "--no-merges" << (remoteBranch + "..HEAD");
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        return output.split('\n', Qt::SkipEmptyParts);
    }
    
    // 如果没有找到远程分支，可能是新分支，返回所有本地提交
    if (error.contains("unknown revision")) {
        cmd.arguments = QStringList() << "log" << "--oneline" << "--no-merges";
        result = executor.executeCommand(cmd, output, error);
        if (result == GitCommandExecutor::Result::Success) {
            return output.split('\n', Qt::SkipEmptyParts);
        }
    }
    
    qWarning() << "WARNING: [GitOperationService::getUnpushedCommits] Failed to get unpushed commits:" << error;
    return QStringList();
}

QStringList GitOperationService::getRemoteUpdates(const QString &repositoryPath, const QString &remoteBranch)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "log";
    cmd.arguments = QStringList() << "log" << "--oneline" << "--no-merges" << ("HEAD.." + remoteBranch);
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 10000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        return output.split('\n', Qt::SkipEmptyParts);
    }
    
    qWarning() << "WARNING: [GitOperationService::getRemoteUpdates] Failed to get remote updates:" << error;
    return QStringList();
}

bool GitOperationService::hasLocalChanges(const QString &repositoryPath)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "status";
    cmd.arguments = QStringList() << "status" << "--porcelain";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        return !output.trimmed().isEmpty();
    }
    
    qWarning() << "WARNING: [GitOperationService::hasLocalChanges] Failed to check local changes:" << error;
    return false;
}

bool GitOperationService::hasUncommittedChanges(const QString &repositoryPath)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "status";
    cmd.arguments = QStringList() << "status" << "--porcelain";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;

    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            if (line.length() >= 2) {
                QChar worktree = line[1];
                if (worktree != ' ' && worktree != '?') {
                    return true; // 有工作区修改
                }
            }
        }
        return false;
    }
    
    qWarning() << "WARNING: [GitOperationService::hasUncommittedChanges] Failed to check uncommitted changes:" << error;
    return false;
}

// ============================================================================
// 私有辅助方法实现
// ============================================================================

void GitOperationService::executeSilentOperation(const QString &operation, const QString &workingDir, 
                                                 const QStringList &arguments)
{
    qInfo() << "INFO: [GitOperationService::executeSilentOperation] Starting" << operation << "operation...";
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = operation;
    cmd.arguments = arguments;
    cmd.workingDirectory = workingDir;
    cmd.timeout = 10000; // 10秒超时

    QString output, error;
    GitCommandExecutor::Result result = m_executor->executeCommand(cmd, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        qInfo() << "INFO: [GitOperationService::executeSilentOperation] Operation" << operation << "completed successfully";
        emit fileManagerRefreshRequested();
        showSuccessNotification(operation);
        emit operationCompleted(operation, true);
    } else {
        qWarning() << "WARNING: [GitOperationService::executeSilentOperation] Operation" << operation << "failed, showing interactive dialog";
        executeInteractiveOperation(operation, workingDir, arguments);
    }
}

void GitOperationService::executeInteractiveOperation(const QString &operation, const QString &workingDir, 
                                                     const QStringList &arguments)
{
    if (!QApplication::instance()) {
        qCritical() << "ERROR: [GitOperationService::executeInteractiveOperation] No QApplication instance found";
        return;
    }

    QWidget *parentWidget = QApplication::activeWindow();
    if (!parentWidget) {
        auto widgets = QApplication::topLevelWidgets();
        for (auto *widget : widgets) {
            if (widget->isVisible() && widget->windowType() == Qt::Window) {
                parentWidget = widget;
                break;
            }
        }
    }

    // 创建操作对话框
    auto *dialog = new GitOperationDialog(operation, parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QString description = QObject::tr("Preparing to execute %1 operation in repository").arg(operation);
    dialog->setOperationDescription(description);

    // 连接完成信号，确保能够正确传递操作结果
    connect(dialog, &QDialog::finished, this, [this, operation, dialog](int result) {
        bool success = (result == QDialog::Accepted);
        QString message;
        
        if (success) {
            message = QObject::tr("%1 operation completed successfully").arg(operation);
            qInfo() << "INFO: [GitOperationService::executeInteractiveOperation] Operation completed successfully:" << operation;
            emit fileManagerRefreshRequested();
        } else {
            auto executionResult = dialog->getExecutionResult();
            if (result == QDialog::Rejected) {
                message = QObject::tr("%1 operation was cancelled").arg(operation);
            } else {
                message = QObject::tr("%1 operation failed").arg(operation);
            }
            qWarning() << "WARNING: [GitOperationService::executeInteractiveOperation] Operation failed or cancelled:" << operation;
        }
        
        // 发射操作完成信号
        emit operationCompleted(operation, success, message);
    });

    // 执行命令并显示对话框
    dialog->executeCommand(workingDir, arguments);
    dialog->show();
    
    qInfo() << "INFO: [GitOperationService::executeInteractiveOperation] Started interactive operation:" 
            << operation << "with arguments:" << arguments;
}

void GitOperationService::showSuccessNotification(const QString &operation)
{
    qInfo() << "INFO: [GitOperationService::showSuccessNotification] Git" << operation << "operation completed successfully";
    
    // TODO: 集成系统通知
    // 示例代码（需要libnotify支持）：
    // notify_notification_new("Git Operation", 
    //                        QString("Git %1 completed successfully").arg(operation).toUtf8().data(),
    //                        "vcs-normal");
}

QString GitOperationService::resolveRepositoryPath(const std::string &filePath)
{
    GitCommandExecutor executor;
    return executor.resolveRepositoryPath(QString::fromStdString(filePath));
}

QStringList GitOperationService::buildFileArguments(const QString &operation, const QString &repoPath, 
                                                   const std::list<std::string> &pathList)
{
    QStringList args { operation };
    GitCommandExecutor executor;
    
    for (const auto &pathStr : pathList) {
        const QString filePath = QString::fromStdString(pathStr);
        QString relativePath = executor.makeRelativePath(repoPath, filePath);
        if (!relativePath.isEmpty()) {
            args << relativePath;
        }
    }
    
    return args;
}

void GitOperationService::onCommandFinished(const QString &command, GitCommandExecutor::Result result,
                                           const QString &output, const QString &error)
{
    Q_UNUSED(output)
    Q_UNUSED(error)
    
    bool success = (result == GitCommandExecutor::Result::Success);
    emit operationCompleted(command, success);
    
    if (success) {
        emit fileManagerRefreshRequested();
    }
} 