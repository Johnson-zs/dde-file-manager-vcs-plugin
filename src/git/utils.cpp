#include "utils.h"

#include <QProcess>
#include <QUrl>
#include <QDir>

#include <cache.h>

namespace Utils {

QString repositoryBaseDir(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--show-toplevel" });
    if (process.waitForFinished(1000) && process.exitCode() == 0)
        return QString::fromUtf8(process.readAll().chopped(1));
    return QString();
}

QString findPathBelowGitBaseDir(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--show-prefix" });
    QString dirBelowBaseDir;
    while (process.waitForReadyRead()) {
        char buffer[512];
        while (process.readLine(buffer, sizeof(buffer)) > 0) {
            dirBelowBaseDir = QString(buffer).trimmed();   // ends in "/" or is empty
        }
    }
    return dirBelowBaseDir;
}

bool isInsideRepositoryDir(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--is-inside-work-tree" });
    if (process.waitForFinished(1000) && process.exitCode() == 0)
        return true;
    return false;
}

bool isInsideRepositoryFile(const QString &path)
{
    const auto &repositoryPaths { Global::Cache::instance().allRepositoryPaths() };
    return std::any_of(repositoryPaths.begin(), repositoryPaths.end(), [&path](const auto &repositoryPath) {
        return path.startsWith(repositoryPath + "/") && path != repositoryPath;
    });
}

int readUntilZeroChar(QIODevice *device, char *buffer, const int maxChars)
{
    if (buffer == nullptr) {   // discard until next \0
        char c;
        while (device->getChar(&c) && c != '\0')
            ;
        return 0;
    }
    int index = -1;
    while (++index < maxChars) {
        if (!device->getChar(&buffer[index])) {
            if (device->waitForReadyRead(30000)) {   // 30 seconds to be consistent with QProcess::waitForReadyRead default
                --index;
                continue;
            } else {
                buffer[index] = '\0';
                return index <= 0 ? 0 : index + 1;
            }
        }
        if (buffer[index] == '\0') {   // line end or we put it there (see above)
            return index + 1;
        }
    }
    return maxChars;
}

std::tuple<char, char, QString> parseLineGitStatus(const QString &line)
{
    Q_ASSERT(line.size() >= 3);
    return { line[0].toLatin1(), line[1].toLatin1(), line.mid(3) };
}

Global::ItemVersion parseXYState(Global::ItemVersion state, char X, char Y)
{
    using Global::ItemVersion;
    switch (X) {
    case '!':
        state = ItemVersion::IgnoredVersion;
        break;
    case '?':
        state = ItemVersion::UnversionedVersion;
        break;
    case 'C':   // handle copied as added version
    case 'A':
        state = ItemVersion::AddedVersion;
        break;
    case 'D':
        state = ItemVersion::RemovedVersion;
        break;
    case 'M':
        state = ItemVersion::LocallyModifiedVersion;
        break;
    }

    // overwrite status depending on the working tree
    switch (Y) {
    case 'D':   // handle "deleted in working tree" as "modified in working tree"
    case 'M':
        state = ItemVersion::LocallyModifiedUnstagedVersion;
        break;
    }

    return state;
}

QStringList makeDirGroup(const QString &directory, const QString &relativeFileName)
{
    Q_ASSERT(relativeFileName.contains("/"));
    Q_ASSERT(QUrl::fromLocalFile(directory).isValid());

    QStringList group;
    int index = relativeFileName.indexOf('/');
    while (index != -1) {
        group.append(directory + "/" + relativeFileName.left(index));
        index = relativeFileName.indexOf('/', index + 1);
    }
    return group;
}

bool isDirectoryEmpty(const QString &path)
{
    QDir directory(path);
    return directory.exists() && directory.isEmpty();
}

bool isIgnoredDirectory(const QString &directory, const QString &path)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "check-ignore", "-v", path });
    if (process.waitForFinished(1000) && process.exitCode() == 0) {
        const QString &result { QString::fromUtf8(process.readAll().chopped(1)) };
        return result.startsWith(".ignore");
    }

    return false;
}

Global::ItemVersion getFileGitStatus(const QString &filePath)
{
    return Global::Cache::instance().version(filePath);
}

bool canAddFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return false;
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    // 可以添加的文件状态：未版本控制、本地修改(未暂存)、忽略的文件可以强制添加
    return status == ItemVersion::UnversionedVersion 
           || status == ItemVersion::LocallyModifiedUnstagedVersion
           || status == ItemVersion::IgnoredVersion;
}

bool canRemoveFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return false;
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    // 可以删除的文件状态：正常版本、本地修改、已添加
    return status == ItemVersion::NormalVersion 
           || status == ItemVersion::LocallyModifiedVersion
           || status == ItemVersion::LocallyModifiedUnstagedVersion
           || status == ItemVersion::AddedVersion;
}

bool canRevertFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return false;
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    // 可以还原的文件状态：本地修改、冲突、已删除
    return status == ItemVersion::LocallyModifiedVersion
           || status == ItemVersion::LocallyModifiedUnstagedVersion
           || status == ItemVersion::ConflictingVersion
           || status == ItemVersion::RemovedVersion;
}

bool canShowFileLog(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return false;
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    // 只要是在版本控制中的文件都可以查看日志（除了未版本控制和忽略的文件）
    return status != ItemVersion::UnversionedVersion 
           && status != ItemVersion::IgnoredVersion;
}

bool canShowFileDiff(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return false;
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    // 可以查看差异的文件状态：有修改的文件
    return status == ItemVersion::LocallyModifiedVersion
           || status == ItemVersion::LocallyModifiedUnstagedVersion
           || status == ItemVersion::ConflictingVersion;
}

bool canShowFileBlame(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return false;
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    // 可以查看blame的文件状态：已在版本控制中的文件（除了新添加的文件）
    return status == ItemVersion::NormalVersion 
           || status == ItemVersion::LocallyModifiedVersion
           || status == ItemVersion::LocallyModifiedUnstagedVersion
           || status == ItemVersion::ConflictingVersion
           || status == ItemVersion::UpdateRequiredVersion;
}

QString getFileStatusDescription(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath))
        return QObject::tr("Not in Git repository");
    
    auto status = getFileGitStatus(filePath);
    using Global::ItemVersion;
    
    switch (status) {
    case ItemVersion::UnversionedVersion:
        return QObject::tr("Untracked file");
    case ItemVersion::NormalVersion:
        return QObject::tr("No changes");
    case ItemVersion::UpdateRequiredVersion:
        return QObject::tr("Update required");
    case ItemVersion::LocallyModifiedVersion:
        return QObject::tr("Modified (staged)");
    case ItemVersion::LocallyModifiedUnstagedVersion:
        return QObject::tr("Modified (unstaged)");
    case ItemVersion::AddedVersion:
        return QObject::tr("Added");
    case ItemVersion::RemovedVersion:
        return QObject::tr("Removed");
    case ItemVersion::ConflictingVersion:
        return QObject::tr("Conflicted");
    case ItemVersion::IgnoredVersion:
        return QObject::tr("Ignored");
    case ItemVersion::MissingVersion:
        return QObject::tr("Missing");
    default:
        return QObject::tr("Unknown status");
    }
}

QString getBranchName(const QString &repositoryPath)
{
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    process.start("git", { "branch", "--show-current" });
    
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        QString branchName = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!branchName.isEmpty()) {
            return branchName;
        }
    }
    
    // 如果上面的命令失败，尝试使用rev-parse
    process.start("git", { "rev-parse", "--abbrev-ref", "HEAD" });
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    
    return QObject::tr("Unknown branch");
}

}   // namespace Utils
