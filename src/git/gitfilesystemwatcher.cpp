#include "gitfilesystemwatcher.h"
#include "utils.h"

#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>

GitFileSystemWatcher::GitFileSystemWatcher(QObject *parent)
    : QObject(parent),
      m_fileWatcher(new QFileSystemWatcher(this)),
      m_updateTimer(new QTimer(this)),
      m_cleanupTimer(new QTimer(this))
{
    qInfo() << "INFO: [GitFileSystemWatcher] Initializing real-time Git file system monitor";

    // 配置防抖更新定时器
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(UPDATE_DELAY_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &GitFileSystemWatcher::onDelayedUpdate);

    // 配置文件系统监控器信号
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &GitFileSystemWatcher::onFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &GitFileSystemWatcher::onDirectoryChanged);

    // 配置定期清理定时器
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL_MS);
    connect(m_cleanupTimer, &QTimer::timeout, this, &GitFileSystemWatcher::onCleanupPaths);
    m_cleanupTimer->start();

    qInfo() << "INFO: [GitFileSystemWatcher] File system monitor initialized successfully";
}

GitFileSystemWatcher::~GitFileSystemWatcher()
{
    qInfo() << "INFO: [GitFileSystemWatcher] Destroying file system monitor";

    // 停止定时器
    m_updateTimer->stop();
    m_cleanupTimer->stop();

    // 清理所有监控
    for (const QString &repo : m_repositories) {
        removeRepositoryWatching(repo);
    }

    // 清理缓存
    m_repositories.clear();
    m_pendingUpdates.clear();
    m_repoFiles.clear();
    m_repoDirs.clear();
}

void GitFileSystemWatcher::addRepository(const QString &repositoryPath)
{
    if (repositoryPath.isEmpty() || m_repositories.contains(repositoryPath)) {
        return;
    }

    // 验证是否为有效的Git仓库
    if (!Utils::isInsideRepositoryDir(repositoryPath)) {
        qWarning() << "WARNING: [GitFileSystemWatcher] Invalid Git repository path:" << repositoryPath;
        return;
    }

    qInfo() << "INFO: [GitFileSystemWatcher] Adding repository to monitor:" << repositoryPath;

    m_repositories.insert(repositoryPath);
    setupRepositoryWatching(repositoryPath);

    qInfo() << "INFO: [GitFileSystemWatcher] Successfully added repository:" << repositoryPath
            << "Total repositories:" << m_repositories.size();
}

void GitFileSystemWatcher::removeRepository(const QString &repositoryPath)
{
    if (!m_repositories.contains(repositoryPath)) {
        return;
    }

    qInfo() << "INFO: [GitFileSystemWatcher] Removing repository from monitor:" << repositoryPath;

    removeRepositoryWatching(repositoryPath);
    m_repositories.remove(repositoryPath);
    m_pendingUpdates.remove(repositoryPath);
    m_repoFiles.remove(repositoryPath);
    m_repoDirs.remove(repositoryPath);

    qInfo() << "INFO: [GitFileSystemWatcher] Successfully removed repository:" << repositoryPath
            << "Remaining repositories:" << m_repositories.size();
}

QStringList GitFileSystemWatcher::getWatchedRepositories() const
{
    return m_repositories.values();
}

bool GitFileSystemWatcher::isWatching(const QString &repositoryPath) const
{
    return m_repositories.contains(repositoryPath);
}

void GitFileSystemWatcher::onFileChanged(const QString &path)
{
    QString repositoryPath = getRepositoryFromPath(path);
    if (repositoryPath.isEmpty()) {
        return;
    }

    if (!shouldWatchFile(path)) {
        return;
    }

    qInfo() << "INFO: [GitFileSystemWatcher] File changed:" << path << "in repository:" << repositoryPath;
    scheduleUpdate(repositoryPath);
}

void GitFileSystemWatcher::onDirectoryChanged(const QString &path)
{
    QString repositoryPath = getRepositoryFromPath(path);
    if (repositoryPath.isEmpty()) {
        return;
    }

    qInfo() << "INFO: [GitFileSystemWatcher] Directory changed:" << path << "in repository:" << repositoryPath;

    // 目录变化可能意味着：
    // 1. 新建了文件（untracked状态）
    // 2. 删除了文件（deleted状态）
    // 3. 移动了文件
    // 4. 新建了目录

    // 关键修复：检测并添加新建的子目录到监控
    checkAndAddNewDirectories(path, repositoryPath);

    // 需要立即更新整个仓库状态
    scheduleUpdate(repositoryPath);
}

void GitFileSystemWatcher::onDelayedUpdate()
{
    QSet<QString> repositoriesToUpdate = m_pendingUpdates;
    m_pendingUpdates.clear();

    for (const QString &repositoryPath : repositoriesToUpdate) {
        qInfo() << "INFO: [GitFileSystemWatcher] Emitting repository changed signal for:" << repositoryPath;
        emit repositoryChanged(repositoryPath);
    }
}

void GitFileSystemWatcher::onCleanupPaths()
{
    qDebug() << "[GitFileSystemWatcher] Running periodic cleanup of invalid paths";

    // 清理无效的文件路径
    QStringList currentFiles = m_fileWatcher->files();
    QStringList invalidFiles;

    for (const QString &filePath : currentFiles) {
        if (!QFileInfo::exists(filePath)) {
            invalidFiles.append(filePath);
        }
    }

    if (!invalidFiles.isEmpty()) {
        m_fileWatcher->removePaths(invalidFiles);
        qDebug() << "[GitFileSystemWatcher] Cleaned up" << invalidFiles.size() << "invalid file paths";
    }

    // 清理无效的目录路径
    QStringList currentDirs = m_fileWatcher->directories();
    QStringList invalidDirs;

    for (const QString &dirPath : currentDirs) {
        if (!QDir(dirPath).exists()) {
            invalidDirs.append(dirPath);
        }
    }

    if (!invalidDirs.isEmpty()) {
        m_fileWatcher->removePaths(invalidDirs);
        qDebug() << "[GitFileSystemWatcher] Cleaned up" << invalidDirs.size() << "invalid directory paths";
    }

    // 更新缓存
    for (auto it = m_repoFiles.begin(); it != m_repoFiles.end(); ++it) {
        QStringList &files = it.value();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        files.removeIf([](const QString &path) {
            return !QFileInfo::exists(path);
        });
#else
        for (int i = files.size() - 1; i >= 0; --i) {
            if (!QFileInfo::exists(files.at(i))) {
                files.removeAt(i);
            }
        }
#endif
    }

    for (auto it = m_repoDirs.begin(); it != m_repoDirs.end(); ++it) {
        QStringList &dirs = it.value();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        dirs.removeIf([](const QString &path) {
            return !QDir(path).exists();
        });
#else
        for (int i = dirs.size() - 1; i >= 0; --i) {
            if (!QDir(dirs.at(i)).exists()) {
                dirs.removeAt(i);
            }
        }
#endif
    }
}

void GitFileSystemWatcher::setupRepositoryWatching(const QString &repositoryPath)
{
    qInfo() << "INFO: [GitFileSystemWatcher] Setting up monitoring for repository:" << repositoryPath;

    QStringList allPaths;

    // 1. 添加Git元数据文件监控
    QStringList gitFiles = getGitMetadataFiles(repositoryPath);
    allPaths.append(gitFiles);
    qDebug() << "[GitFileSystemWatcher] Found" << gitFiles.size() << "Git metadata files";

    // 2. 添加重要目录监控
    QStringList importantDirs = getImportantDirectories(repositoryPath);
    allPaths.append(importantDirs);
    qDebug() << "[GitFileSystemWatcher] Found" << importantDirs.size() << "important directories";

    // 3. 添加被跟踪文件监控（关键！）
    QStringList trackedFiles = getTrackedFiles(repositoryPath);
    allPaths.append(trackedFiles);
    qInfo() << "INFO: [GitFileSystemWatcher] Found" << trackedFiles.size()
            << "tracked files to monitor";

    // 立即同步添加所有监控路径（关键修复！）
    if (!allPaths.isEmpty()) {
        // 过滤出有效路径
        QStringList validPaths;
        for (const QString &path : allPaths) {
            if (QFileInfo::exists(path)) {
                validPaths.append(path);
            }
        }

        if (!validPaths.isEmpty()) {
            // 同步添加所有路径 - 不使用异步以确保立即生效
            m_fileWatcher->addPaths(validPaths);
            qInfo() << "INFO: [GitFileSystemWatcher] Immediately added" << validPaths.size() << "paths to file watcher";
        }

        // 分别缓存文件和目录
        QStringList files, dirs;
        for (const QString &path : allPaths) {
            if (QFileInfo(path).isFile()) {
                files.append(path);
            } else if (QFileInfo(path).isDir()) {
                dirs.append(path);
            }
        }

        m_repoFiles[repositoryPath] = files;
        m_repoDirs[repositoryPath] = dirs;

        qInfo() << "INFO: [GitFileSystemWatcher] Successfully setup monitoring for repository:" << repositoryPath
                << "Files:" << files.size() << "Directories:" << dirs.size();

        // 输出当前监控的路径数量用于调试
        qInfo() << "INFO: [GitFileSystemWatcher] Total paths being watched:"
                << "Files:" << m_fileWatcher->files().size()
                << "Directories:" << m_fileWatcher->directories().size();
    }
}

void GitFileSystemWatcher::removeRepositoryWatching(const QString &repositoryPath)
{
    qDebug() << "[GitFileSystemWatcher] Removing monitoring for repository:" << repositoryPath;

    // 移除文件监控
    if (m_repoFiles.contains(repositoryPath)) {
        QStringList files = m_repoFiles[repositoryPath];
        if (!files.isEmpty()) {
            m_fileWatcher->removePaths(files);
        }
    }

    // 移除目录监控
    if (m_repoDirs.contains(repositoryPath)) {
        QStringList dirs = m_repoDirs[repositoryPath];
        if (!dirs.isEmpty()) {
            m_fileWatcher->removePaths(dirs);
        }
    }
}

QStringList GitFileSystemWatcher::getGitMetadataFiles(const QString &repositoryPath) const
{
    QStringList files;
    QString gitDir = repositoryPath + "/.git";

    // Git关键元数据文件
    QStringList gitFiles = {
        gitDir + "/index",   // 暂存区索引
        gitDir + "/HEAD",   // 当前分支指针
        gitDir + "/config",   // 仓库配置
        gitDir + "/FETCH_HEAD",   // fetch操作记录
        gitDir + "/ORIG_HEAD",   // 操作前HEAD
        gitDir + "/MERGE_HEAD",   // 合并状态
        gitDir + "/COMMIT_EDITMSG",   // 提交消息
        gitDir + "/MERGE_MSG"   // 合并消息
    };

    for (const QString &file : gitFiles) {
        if (QFileInfo::exists(file)) {
            files.append(file);
        }
    }

    return files;
}

QStringList GitFileSystemWatcher::getTrackedFiles(const QString &repositoryPath) const
{
    qInfo() << "INFO: [GitFileSystemWatcher] Getting tracked files for repository:" << repositoryPath;

    QStringList trackedFiles;

    // 使用git ls-files获取所有被跟踪的文件
    QProcess process;
    process.setWorkingDirectory(repositoryPath);
    process.start("git", { "ls-files", "-z" });

    if (!process.waitForFinished(5000)) {
        qWarning() << "WARNING: [GitFileSystemWatcher] Failed to get tracked files for repository:" << repositoryPath;
        return trackedFiles;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList relativePaths = output.split('\0', Qt::SkipEmptyParts);
#else
    QStringList relativePaths = output.split('\0', QString::SkipEmptyParts);
#endif

    qInfo() << "INFO: [GitFileSystemWatcher] git ls-files returned" << relativePaths.size() << "files";

    int fileCount = 0;
    int skippedCount = 0;
    for (const QString &relativePath : relativePaths) {
        if (fileCount >= MAX_FILES_PER_REPO) {
            qWarning() << "WARNING: [GitFileSystemWatcher] Reached maximum file limit ("
                       << MAX_FILES_PER_REPO << ") for repository:" << repositoryPath;
            break;
        }

        QString absolutePath = repositoryPath + "/" + relativePath;
        QFileInfo fileInfo(absolutePath);

        if (fileInfo.exists() && fileInfo.isFile()) {
            if (shouldWatchFile(absolutePath)) {
                trackedFiles.append(absolutePath);
                fileCount++;
            } else {
                skippedCount++;
                // 输出一些被跳过的文件用于调试
                if (skippedCount <= 5) {
                    qDebug() << "[GitFileSystemWatcher] Skipped file:" << relativePath << "reason: filter";
                }
            }
        } else {
            skippedCount++;
            if (skippedCount <= 5) {
                qDebug() << "[GitFileSystemWatcher] Skipped file:" << relativePath << "reason: not exists or not file";
            }
        }
    }

    if (skippedCount > 5) {
        qDebug() << "[GitFileSystemWatcher] ... and" << (skippedCount - 5) << "more files were skipped";
    }

    qInfo() << "INFO: [GitFileSystemWatcher] Selected" << trackedFiles.size()
            << "valid tracked files out of" << relativePaths.size() << "total files";

    return trackedFiles;
}

QStringList GitFileSystemWatcher::getImportantDirectories(const QString &repositoryPath) const
{
    QStringList dirs;

    // 仓库根目录
    dirs.append(repositoryPath);

    // .git目录及其重要子目录
    QString gitDir = repositoryPath + "/.git";
    if (QDir(gitDir).exists()) {
        dirs.append(gitDir);

        QStringList gitSubDirs = {
            gitDir + "/refs",
            gitDir + "/refs/heads",
            gitDir + "/refs/remotes",
            gitDir + "/logs"
        };

        for (const QString &subDir : gitSubDirs) {
            if (QDir(subDir).exists()) {
                dirs.append(subDir);
            }
        }
    }

    // 添加工作目录的子目录监控（关键修复！）
    // 这样可以检测到新建文件和删除文件
    QDir repoDir(repositoryPath);
    QStringList subDirs = repoDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    int dirCount = 0;
    const int maxDirs = 5000;   // 限制监控的子目录数量

    for (const QString &subDirName : subDirs) {
        if (dirCount >= maxDirs) break;

        QString subDirPath = repositoryPath + "/" + subDirName;

        if (shouldWatchDirectory(subDirPath, repositoryPath)) {
            dirs.append(subDirPath);
            dirCount++;

            // 递归添加一层子目录
            QDir subDir(subDirPath);
            QStringList subSubDirs = subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

            for (const QString &subSubDirName : subSubDirs) {
                if (dirCount >= maxDirs) break;

                QString subSubDirPath = subDirPath + "/" + subSubDirName;
                if (shouldWatchDirectory(subSubDirPath, repositoryPath)) {
                    dirs.append(subSubDirPath);
                    dirCount++;
                }
            }
        }
    }

    qDebug() << "[GitFileSystemWatcher] Found" << dirs.size() << "directories to monitor (including"
             << dirCount << "working directories)";

    return dirs;
}

bool GitFileSystemWatcher::shouldWatchFile(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);

    // 检查文件是否存在
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    QString fileName = fileInfo.fileName();
    QString suffix = fileInfo.suffix().toLower();
    QString absolutePath = fileInfo.absoluteFilePath();

    // Git元数据文件始终监控
    if (absolutePath.contains("/.git/")) {
        return true;
    }

    // 忽略的文件后缀（扩展列表，忽略更多构建和临时文件）
    static const QStringList ignoredSuffixes = {
        "tmp", "temp", "bak", "swp", "swo", "~",
        "o", "obj", "exe", "dll", "so", "a", "lib",
        "pyc", "pyo", "class", "d", "ts", "qm", "moc",
        "cache", "log", "out", "debug", "cmake", "make",
        "json", "txt", "internal", "depends"
    };

    if (ignoredSuffixes.contains(suffix)) {
        return false;
    }

    // 忽略隐藏文件（除了Git相关，但允许更多常见文件）
    if (fileName.startsWith(".") && fileName != ".gitignore" && fileName != ".gitmodules" && fileName != ".gitattributes" && !fileName.endsWith(".md")) {
        return false;
    }

    // 忽略在构建/缓存目录中的文件（更精确的路径匹配）
    static const QStringList ignoredPaths = {
        "/node_modules/", "/build/", "/dist/", "/.vscode/", "/.idea/", "/__pycache__/"
    };

    for (const QString &ignoredPath : ignoredPaths) {
        if (absolutePath.contains(ignoredPath)) {
            return false;
        }
    }

    // 默认允许监控（更宽松的策略）
    return true;
}

bool GitFileSystemWatcher::shouldWatchDirectory(const QString &dirPath, const QString &repositoryPath) const
{
    // 确保目录在仓库中
    if (!dirPath.startsWith(repositoryPath)) {
        return false;
    }

    QFileInfo dirInfo(dirPath);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        return false;
    }

    QString dirName = dirInfo.fileName();

    // 忽略的目录名（扩展列表，包含更多构建目录）
    static const QStringList ignoredDirs = {
        "node_modules", "build", "dist", "target", "bin", "obj",
        "__pycache__", ".vscode", ".idea", ".vs", "CMakeFiles",
        "tmp", "temp", "cache", ".cache", "debian", ".debhelper",
        ".clangd", "autogen", "_autogen"
    };

    if (ignoredDirs.contains(dirName)) {
        return false;
    }

    // .git目录及其子目录特殊处理
    if (dirName == ".git" || dirPath.contains("/.git/")) {
        return true;
    }

    // 忽略以.开头的隐藏目录
    if (dirName.startsWith(".")) {
        return false;
    }

    return true;
}

QString GitFileSystemWatcher::getRepositoryFromPath(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString absolutePath = fileInfo.absoluteFilePath();

    // 查找匹配的仓库（从最长路径开始匹配）
    QString bestMatch;
    for (const QString &repoPath : m_repositories) {
        if ((absolutePath.startsWith(repoPath + "/") || absolutePath == repoPath) && repoPath.length() > bestMatch.length()) {
            bestMatch = repoPath;
        }
    }

    return bestMatch;
}

void GitFileSystemWatcher::scheduleUpdate(const QString &repositoryPath)
{
    if (repositoryPath.isEmpty() || !m_repositories.contains(repositoryPath)) {
        return;
    }

    m_pendingUpdates.insert(repositoryPath);

    // 启动或重启防抖定时器
    m_updateTimer->start();
}

void GitFileSystemWatcher::addWatchPaths(const QStringList &paths, bool isFile)
{
    Q_UNUSED(isFile)   // 当前版本不需要区分文件和目录类型

    if (paths.isEmpty()) {
        return;
    }

    // 过滤出有效路径
    QStringList validPaths;
    for (const QString &path : paths) {
        if (QFileInfo::exists(path)) {
            validPaths.append(path);
        }
    }

    if (!validPaths.isEmpty()) {
        // 直接添加所有路径（Qt会自动处理文件和目录的区别）
        m_fileWatcher->addPaths(validPaths);
        qInfo() << "INFO: [GitFileSystemWatcher] Added" << validPaths.size() << "paths to file watcher";
    }
}

void GitFileSystemWatcher::checkAndAddNewDirectories(const QString &changedDirPath, const QString &repositoryPath)
{
    QDir changedDir(changedDirPath);
    if (!changedDir.exists()) {
        return;
    }

    // 获取当前正在监控的目录列表
    QStringList currentlyWatched;
    if (m_repoDirs.contains(repositoryPath)) {
        currentlyWatched = m_repoDirs[repositoryPath];
    }

    // 获取变化目录下的所有子目录
    QStringList subDirs = changedDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList newDirsToWatch;

    for (const QString &subDirName : subDirs) {
        QString subDirPath = changedDirPath + "/" + subDirName;

        // 检查是否应该监控这个目录，且当前未被监控
        if (shouldWatchDirectory(subDirPath, repositoryPath) && !currentlyWatched.contains(subDirPath)) {

            newDirsToWatch.append(subDirPath);

            // 递归检查子目录（只检查一层，避免性能问题）
            QDir subDir(subDirPath);
            QStringList subSubDirs = subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

            for (const QString &subSubDirName : subSubDirs) {
                QString subSubDirPath = subDirPath + "/" + subSubDirName;
                if (shouldWatchDirectory(subSubDirPath, repositoryPath) && !currentlyWatched.contains(subSubDirPath)) {
                    newDirsToWatch.append(subSubDirPath);
                }
            }
        }
    }

    // 添加新发现的目录到监控
    if (!newDirsToWatch.isEmpty()) {
        m_fileWatcher->addPaths(newDirsToWatch);

        // 更新缓存
        if (m_repoDirs.contains(repositoryPath)) {
            m_repoDirs[repositoryPath].append(newDirsToWatch);
        } else {
            m_repoDirs[repositoryPath] = newDirsToWatch;
        }

        qInfo() << "INFO: [GitFileSystemWatcher] Dynamically added" << newDirsToWatch.size()
                << "new directories to monitoring:" << newDirsToWatch;
    }
}
