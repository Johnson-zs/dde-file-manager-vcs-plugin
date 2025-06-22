#include "git-utils.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QIODevice>
#include <QObject>
#include <QDebug>

// 进程适配包含
#ifdef GIT_PLUGIN_PROCESS
// 插件进程使用本地缓存
class GitLocalCache;
extern GitLocalCache* g_localCache;
#elif defined(GIT_DAEMON_PROCESS)
// 服务进程直接访问内部缓存
class GitStatusCache;
extern GitStatusCache* g_statusCache;
#elif defined(GIT_DIALOG_PROCESS)
// 对话框进程通过DBus调用
class GitDBusClient;
extern GitDBusClient* g_dbusClient;
#endif

namespace GitUtils {

// 递归辅助函数
namespace {
bool isDirectoryEmptyRecursive(const QString &path, int remainingDepth)
{
    if (remainingDepth <= 0) {
        // 达到最大深度，为了性能考虑，假设不为空
        return false;
    }

    QDir directory(path);
    if (!directory.exists()) {
        return true;   // 不存在的目录视为空
    }

    // 快速路径：如果目录真的为空，直接返回true
    if (directory.isEmpty()) {
        return true;
    }

    // 检查是否有文件
    QStringList files = directory.entryList(QDir::Files | QDir::NoDotAndDotDot);
    if (!files.isEmpty()) {
        return false;   // 有文件，不为空
    }

    // 只检查目录
    QStringList dirs = directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    const int maxDirsToCheck = 5;   // 递归时减少检查数量

    if (dirs.size() > maxDirsToCheck) {
        // 子目录太多，为了性能考虑，假设不为空
        return false;
    }

    // 递归检查每个子目录
    for (const QString &subDir : dirs) {
        QString subDirPath = directory.absoluteFilePath(subDir);
        if (!isDirectoryEmptyRecursive(subDirPath, remainingDepth - 1)) {
            return false;
        }
    }

    return true;
}
}

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

bool isInsideRepositoryFile(const QString &filePath)
{
#ifdef GIT_PLUGIN_PROCESS
    // 插件进程：查询本地缓存
    if (g_localCache) {
        return g_localCache->isInsideRepository(filePath);
    }
#elif defined(GIT_DAEMON_PROCESS)
    // 服务进程：直接查询内部缓存
    if (g_statusCache) {
        return g_statusCache->isInsideRepository(filePath);
    }
#elif defined(GIT_DIALOG_PROCESS)
    // 对话框进程：通过DBus调用
    if (g_dbusClient) {
        return g_dbusClient->isInsideRepository(filePath);
    }
#else
    // 测试环境：检查文件是否在仓库中
    QFileInfo fileInfo(filePath);
    
    // 首先检查文件是否存在
    if (!fileInfo.exists()) {
        // 对于不存在的文件，先检查是否在Git仓库中
        QString directory = fileInfo.absolutePath();
        QString repositoryPath = repositoryBaseDir(directory);
        if (repositoryPath.isEmpty()) {
            return UnversionedVersion;
        }
        
        // 检查文件是否被Git跟踪
        QProcess process;
        process.setWorkingDirectory(repositoryPath);
        QString relativePath = filePath;
        if (filePath.startsWith(repositoryPath + "/")) {
            relativePath = filePath.mid(repositoryPath.length() + 1);
        }
        
        process.start("git", {"ls-files", relativePath});
        if (process.waitForFinished(3000) && process.exitCode() == 0) {
            QString lsOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            if (!lsOutput.isEmpty()) {
                return MissingVersion;  // 被跟踪但文件不存在
            }
        }
        return UnversionedVersion;  // 不存在且未被跟踪
    }
    
    QString directory = fileInfo.isDir() ? filePath : fileInfo.absolutePath();
    
    // 获取仓库根目录
    QString repositoryPath = repositoryBaseDir(directory);
    if (repositoryPath.isEmpty()) {
        return UnversionedVersion;
    }
    
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    
    // 使用相对路径查询文件状态
    QString relativePath = filePath;
    if (filePath.startsWith(repositoryPath + "/")) {
        relativePath = filePath.mid(repositoryPath.length() + 1);
    } else if (filePath == repositoryPath) {
        // 查询仓库根目录状态，使用 "." 
        relativePath = ".";
    }
    
    process.start("git", {"status", "--porcelain", relativePath});
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        
        if (output.isEmpty()) {
            // 如果是仓库根目录，检查是否有任何更改
            if (relativePath == ".") {
                process.start("git", {"status", "--porcelain"});
                if (process.waitForFinished(3000) && process.exitCode() == 0) {
                    QString fullOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
                    return fullOutput.isEmpty() ? NormalVersion : LocallyModifiedVersion;
                }
            }
            
            return NormalVersion;  // 文件已提交且无更改
        }
        
        // 解析状态输出
        if (output.length() >= 2) {
            char indexStatus = output[0].toLatin1();
            char workingStatus = output[1].toLatin1();
            return parseFileStatusFromChars(indexStatus, workingStatus);
        }
    }
#endif
    
    // 降级处理：直接检查目录
    QFileInfo fallbackFileInfo(filePath);
    QString fallbackDirectory = fallbackFileInfo.isDir() ? filePath : fallbackFileInfo.absolutePath();
    return isInsideRepositoryDir(fallbackDirectory);
}

bool isGitRepositoryRoot(const QString &directoryPath)
{
    // 轻量级检测：检查 .git 目录是否存在
    QDir dir(directoryPath);
    if (!dir.exists()) {
        return false;
    }

    // 检查 .git 目录或 .git 文件（用于 git worktree）
    QString gitPath = dir.absoluteFilePath(".git");
    QFileInfo gitInfo(gitPath);

    return gitInfo.exists() && (gitInfo.isDir() || gitInfo.isFile());
}

ItemVersion getFileGitStatus(const QString &filePath)
{
#ifdef GIT_PLUGIN_PROCESS
    // 插件进程：查询本地缓存
    if (g_localCache) {
        return g_localCache->getFileStatus(filePath);
    }
#elif defined(GIT_DAEMON_PROCESS)
    // 服务进程：直接查询内部缓存
    if (g_statusCache) {
        return g_statusCache->version(filePath);
    }
#elif defined(GIT_DIALOG_PROCESS)
    // 对话框进程：通过DBus调用
    if (g_dbusClient) {
        return g_dbusClient->getFileStatus(filePath);
    }
#else
    // 测试环境：直接查询Git状态
    QFileInfo fileInfo(filePath);
    
    // 首先检查文件是否存在
    if (!fileInfo.exists()) {
        // 对于不存在的文件，先检查是否在Git仓库中
        QString directory = fileInfo.absolutePath();
        QString repositoryPath = repositoryBaseDir(directory);
        if (repositoryPath.isEmpty()) {
            return UnversionedVersion;
        }
        
        // 检查文件是否被Git跟踪
        QProcess process;
        process.setWorkingDirectory(repositoryPath);
        QString relativePath = filePath;
        if (filePath.startsWith(repositoryPath + "/")) {
            relativePath = filePath.mid(repositoryPath.length() + 1);
        }
        
        process.start("git", {"ls-files", relativePath});
        if (process.waitForFinished(3000) && process.exitCode() == 0) {
            QString lsOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            if (!lsOutput.isEmpty()) {
                return MissingVersion;  // 被跟踪但文件不存在
            }
        }
        return UnversionedVersion;  // 不存在且未被跟踪
    }
    
    QString directory = fileInfo.isDir() ? filePath : fileInfo.absolutePath();
    
    // 获取仓库根目录
    QString repositoryPath = repositoryBaseDir(directory);
    if (repositoryPath.isEmpty()) {
        return UnversionedVersion;
    }
    
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    
    // 使用相对路径查询文件状态
    QString relativePath = filePath;
    if (filePath.startsWith(repositoryPath + "/")) {
        relativePath = filePath.mid(repositoryPath.length() + 1);
    } else if (filePath == repositoryPath) {
        // 查询仓库根目录状态，使用 "." 
        relativePath = ".";
    }
    
    process.start("git", {"status", "--porcelain", relativePath});
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        
        if (output.isEmpty()) {
            // 如果是仓库根目录，检查是否有任何更改
            if (relativePath == ".") {
                process.start("git", {"status", "--porcelain"});
                if (process.waitForFinished(3000) && process.exitCode() == 0) {
                    QString fullOutput = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
                    return fullOutput.isEmpty() ? NormalVersion : LocallyModifiedVersion;
                }
            }
            
            return NormalVersion;  // 文件已提交且无更改
        }
        
        // 解析状态输出
        if (output.length() >= 2) {
            char indexStatus = output[0].toLatin1();
            char workingStatus = output[1].toLatin1();
            return parseFileStatusFromChars(indexStatus, workingStatus);
        }
    }
#endif
    
    // 降级处理：返回未版本控制状态
    return UnversionedVersion;
}

// 添加用于解析Git状态字符的辅助函数
ItemVersion parseFileStatusFromChars(char indexStatus, char workingStatus)
{
    // 处理冲突状态
    if ((indexStatus == 'U' && workingStatus == 'U') ||  // 两边都修改的冲突
        (indexStatus == 'A' && workingStatus == 'A') ||  // 两边都添加的冲突
        (indexStatus == 'D' && workingStatus == 'D') ||  // 两边都删除的冲突
        (indexStatus == 'U' && workingStatus != 'U') ||  // 一边冲突
        (indexStatus != 'U' && workingStatus == 'U')) {  // 另一边冲突
        return ConflictingVersion;
    }
    
    // Parse git status codes to our enum
    if (indexStatus != ' ' && indexStatus != '?') {
        // File is staged
        if (indexStatus == 'A') {
            return AddedVersion;
        } else if (indexStatus == 'M') {
            return LocallyModifiedVersion;
        } else if (indexStatus == 'D') {
            return RemovedVersion;
        } else if (indexStatus == 'R') {
            return LocallyModifiedVersion;  // Renamed files treated as modified
        } else if (indexStatus == 'C') {
            return AddedVersion;  // Copied files treated as added
        } else {
            return LocallyModifiedVersion;  // Other staged changes
        }
    } else if (workingStatus == '?') {
        return UnversionedVersion;
    } else if (workingStatus == '!') {
        return IgnoredVersion;
    } else {
        // File is modified but not staged
        if (workingStatus == 'M') {
            return LocallyModifiedUnstagedVersion;
        } else if (workingStatus == 'D') {
            return MissingVersion;  // Deleted in working tree
        } else {
            return LocallyModifiedUnstagedVersion;
        }
    }
}

QString getFileStatusDescription(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return QObject::tr("Not in Git repository");

    auto status = getFileGitStatus(filePath);

    switch (status) {
    case UnversionedVersion:
        return QObject::tr("Untracked file");
    case NormalVersion:
        return QObject::tr("No changes");
    case UpdateRequiredVersion:
        return QObject::tr("Update required");
    case LocallyModifiedVersion:
        return QObject::tr("Modified (staged)");
    case LocallyModifiedUnstagedVersion:
        return QObject::tr("Modified (unstaged)");
    case AddedVersion:
        return QObject::tr("Added");
    case RemovedVersion:
        return QObject::tr("Removed");
    case ConflictingVersion:
        return QObject::tr("Conflicted");
    case IgnoredVersion:
        return QObject::tr("Ignored");
    case MissingVersion:
        return QObject::tr("Missing");
    default:
        return QObject::tr("Unknown status");
    }
}

bool canAddFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return false;

    auto status = getFileGitStatus(filePath);

    // 可以添加的文件状态：未版本控制、本地修改(未暂存)、忽略的文件可以强制添加
    return status == UnversionedVersion
            || status == LocallyModifiedUnstagedVersion
            || status == IgnoredVersion;
}

bool canRemoveFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return false;

    auto status = getFileGitStatus(filePath);

    // 可以删除的文件状态：正常版本、本地修改、已添加
    return status == NormalVersion
            || status == LocallyModifiedVersion
            || status == LocallyModifiedUnstagedVersion
            || status == AddedVersion;
}

bool canRevertFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return false;

    auto status = getFileGitStatus(filePath);

    // 可以还原的文件状态：本地修改、冲突、已删除
    return status == LocallyModifiedVersion
            || status == LocallyModifiedUnstagedVersion
            || status == ConflictingVersion
            || status == RemovedVersion;
}

bool canShowFileLog(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return false;

    auto status = getFileGitStatus(filePath);

    // 只要是在版本控制中的文件都可以查看日志（除了未版本控制和忽略的文件）
    return status != UnversionedVersion
            && status != IgnoredVersion;
}

bool canShowFileDiff(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return false;

    auto status = getFileGitStatus(filePath);

    // 可以查看差异的文件状态：有修改的文件
    return status == LocallyModifiedVersion
            || status == LocallyModifiedUnstagedVersion
            || status == ConflictingVersion;
}

bool canShowFileBlame(const QString &filePath)
{
    if (QFileInfo(filePath).isDir())
        return false;

    if (!isInsideRepositoryFile(filePath) && !isGitRepositoryRoot(filePath))
        return false;

    auto status = getFileGitStatus(filePath);

    // 可以查看blame的文件状态：已在版本控制中的文件（除了新添加的文件）
    return status == NormalVersion
            || status == LocallyModifiedVersion
            || status == LocallyModifiedUnstagedVersion
            || status == ConflictingVersion
            || status == UpdateRequiredVersion;
}

bool canStashFile(const QString &filePath)
{
    if (!isInsideRepositoryFile(filePath)) {
        return false;
    }

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        return false;
    }

    // 检查文件的Git状态
    ItemVersion status = getFileGitStatus(filePath);
    
    // 只有已修改、已添加或已删除的文件可以被stash
    return status == LocallyModifiedVersion ||
           status == LocallyModifiedUnstagedVersion ||
           status == AddedVersion ||
           status == RemovedVersion;
}

bool hasUncommittedChanges(const QString &repositoryPath)
{
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    process.start("git", { "status", "--porcelain" });
    
    if (process.waitForFinished(5000) && process.exitCode() == 0) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        return !output.trimmed().isEmpty();
    }
    
    return false;
}

bool hasStashes(const QString &repositoryPath)
{
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    process.start("git", { "stash", "list" });
    
    if (process.waitForFinished(5000) && process.exitCode() == 0) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        return !output.trimmed().isEmpty();
    }
    
    return false;
}

bool isWorkingDirectoryClean(const QString &repositoryPath)
{
    return !hasUncommittedChanges(repositoryPath);
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

    return QString("main");  // 默认分支名
}

GitRepositoryInfo getRepositoryInfo(const QString &repositoryPath)
{
    GitRepositoryInfo info;
    info.path = repositoryPath;
    info.branch = getBranchName(repositoryPath);
    info.isDirty = hasUncommittedChanges(repositoryPath);
    info.ahead = 0;    // TODO: 实现ahead/behind计算
    info.behind = 0;
    return info;
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

ItemVersion parseXYState(ItemVersion state, char X, char Y)
{
    switch (X) {
    case '!':
        state = IgnoredVersion;
        break;
    case '?':
        state = UnversionedVersion;
        break;
    case 'C':   // handle copied as added version
    case 'A':
        state = AddedVersion;
        break;
    case 'D':
        state = RemovedVersion;
        break;
    case 'M':
        state = LocallyModifiedVersion;
        break;
    }

    // overwrite status depending on the working tree
    switch (Y) {
    case 'D':   // handle "deleted in working tree" as "modified in working tree"
    case 'M':
        state = LocallyModifiedUnstagedVersion;
        break;
    }

    return state;
}

QStringList makeDirGroup(const QString &directory, const QString &relativeFileName)
{
    Q_ASSERT(relativeFileName.contains("/"));

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
    if (!directory.exists()) {
        return false;
    }

    // 快速路径：如果目录真的为空，直接返回true
    if (directory.isEmpty()) {
        return true;
    }

    // 检查是否只包含空目录（Git意义上的空）
    // 设置过滤器：只显示目录，不显示文件，不显示隐藏文件
    QStringList entries = directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // 如果有文件（通过检查总条目数与目录数的差异），则不为空
    QStringList allEntries = directory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    if (allEntries.size() > entries.size()) {
        // 存在文件，目录不为空
        return false;
    }

    // 只有目录，需要检查这些目录是否都为空
    // 为了性能考虑，限制递归深度和检查数量
    const int maxDirsToCheck = 10;   // 最多检查10个目录
    const int maxDepth = 3;   // 最大递归深度为3层

    if (entries.size() > maxDirsToCheck) {
        // 如果子目录太多，为了性能考虑，假设不为空
        return false;
    }

    // 递归检查每个子目录是否为空
    for (const QString &subDir : entries) {
        QString subDirPath = directory.absoluteFilePath(subDir);
        if (!isDirectoryEmptyRecursive(subDirPath, maxDepth - 1)) {
            return false;
        }
    }

    return true;
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

} 