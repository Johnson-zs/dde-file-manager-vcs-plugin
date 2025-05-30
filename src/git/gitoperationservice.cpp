#include "gitoperationservice.h"
#include "dialogs/gitdialogs.h"
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

    // Use the enhanced showOperationDialog method that actually executes the command
    GitDialogManager::instance()->showOperationDialog(operation, workingDir, arguments, parentWidget);
    
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