#include "gitoperationutils.h"

#include <QProcess>
#include <QDebug>
#include <QFileInfo>

GitOperationUtils::GitOperationUtils(QObject *parent)
    : QObject(parent)
{
}

GitOperationResult GitOperationUtils::stageFile(const QString &repositoryPath, const QString &filePath)
{
    return executeSingleFileOperation(repositoryPath, "stage", { "add" }, filePath);
}

GitOperationResult GitOperationUtils::unstageFile(const QString &repositoryPath, const QString &filePath)
{
    return executeSingleFileOperation(repositoryPath, "unstage", { "reset", "HEAD" }, filePath);
}

GitOperationResult GitOperationUtils::discardFile(const QString &repositoryPath, const QString &filePath)
{
    return executeSingleFileOperation(repositoryPath, "discard", { "checkout", "HEAD", "--" }, filePath);
}

GitOperationResult GitOperationUtils::addFile(const QString &repositoryPath, const QString &filePath)
{
    return executeSingleFileOperation(repositoryPath, "add", { "add" }, filePath);
}

GitOperationResult GitOperationUtils::resetFile(const QString &repositoryPath, const QString &filePath)
{
    return executeSingleFileOperation(repositoryPath, "reset", { "checkout", "HEAD", "--" }, filePath);
}

GitOperationResult GitOperationUtils::stageFiles(const QString &repositoryPath, const QStringList &filePaths)
{
    return executeBatchFileOperation(repositoryPath, "stage", { "add" }, filePaths);
}

GitOperationResult GitOperationUtils::unstageFiles(const QString &repositoryPath, const QStringList &filePaths)
{
    return executeBatchFileOperation(repositoryPath, "unstage", { "reset", "HEAD" }, filePaths);
}

GitOperationResult GitOperationUtils::addFiles(const QString &repositoryPath, const QStringList &filePaths)
{
    return executeBatchFileOperation(repositoryPath, "add", { "add" }, filePaths);
}

GitOperationResult GitOperationUtils::resetFiles(const QString &repositoryPath, const QStringList &filePaths)
{
    return executeBatchFileOperation(repositoryPath, "reset", { "checkout", "HEAD", "--" }, filePaths);
}

QString GitOperationUtils::getCurrentBranch(const QString &repositoryPath)
{
    auto result = executeGitCommand(repositoryPath, { "symbolic-ref", "--short", "HEAD" }, 3000);
    if (result.success) {
        QString branch = result.output.trimmed();
        if (!branch.isEmpty()) {
            return branch;
        }
    }

    // Fallback: try rev-parse
    result = executeGitCommand(repositoryPath, { "rev-parse", "--abbrev-ref", "HEAD" }, 3000);
    if (result.success) {
        return result.output.trimmed();
    }

    return QObject::tr("Unknown branch");
}

bool GitOperationUtils::isRepositoryClean(const QString &repositoryPath)
{
    auto result = executeGitCommand(repositoryPath, { "status", "--porcelain" }, 3000);
    return result.success && result.output.trimmed().isEmpty();
}

GitOperationResult GitOperationUtils::executeGitCommand(const QString &repositoryPath,
                                                        const QStringList &arguments,
                                                        int timeoutMs)
{
    QProcess process;
    process.setWorkingDirectory(repositoryPath);

    qDebug() << "[GitOperationUtils] Executing git command:" << arguments << "in" << repositoryPath;

    process.start("git", arguments);

    if (!process.waitForFinished(timeoutMs)) {
        QString error = QObject::tr("Git command timed out: %1").arg(arguments.join(" "));
        qWarning() << "[GitOperationUtils]" << error;
        return GitOperationResult(false, QString(), error, -1);
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QString error = QString::fromUtf8(process.readAllStandardError());
    const int exitCode = process.exitCode();
    const bool success = (exitCode == 0);

    if (success) {
        qDebug() << "[GitOperationUtils] Git command succeeded:" << arguments.join(" ");
    } else {
        qWarning() << "[GitOperationUtils] Git command failed:" << arguments.join(" ")
                   << "Exit code:" << exitCode << "Error:" << error;
    }

    return GitOperationResult(success, output, error, exitCode);
}

GitOperationResult GitOperationUtils::executeSingleFileOperation(const QString &repositoryPath,
                                                                 const QString &operation,
                                                                 const QStringList &arguments,
                                                                 const QString &filePath)
{
    QStringList fullArgs = arguments;
    fullArgs.append(filePath);

    auto result = executeGitCommand(repositoryPath, fullArgs);

    if (result.success) {
        qDebug() << "[GitOperationUtils] Successfully" << operation << "file:" << filePath;
    } else {
        qWarning() << "[GitOperationUtils] Failed to" << operation << "file:" << filePath
                   << "Error:" << result.error;
    }

    return result;
}

GitOperationResult GitOperationUtils::executeBatchFileOperation(const QString &repositoryPath,
                                                                const QString &operation,
                                                                const QStringList &baseArguments,
                                                                const QStringList &filePaths)
{
    if (filePaths.isEmpty()) {
        return GitOperationResult(true, QObject::tr("No files to process"), QString(), 0);
    }

    // For batch operations, we can pass all files at once to git
    QStringList fullArgs = baseArguments;
    fullArgs.append(filePaths);

    auto result = executeGitCommand(repositoryPath, fullArgs);

    if (result.success) {
        qDebug() << "[GitOperationUtils] Successfully" << operation << filePaths.size() << "files";
    } else {
        qWarning() << "[GitOperationUtils] Failed to" << operation << filePaths.size() << "files"
                   << "Error:" << result.error;
    }

    return result;
}
