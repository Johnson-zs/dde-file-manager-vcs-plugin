#include "git-repository-watcher.h"
#include "git-utils.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QStandardPaths>
#include <algorithm>

GitRepositoryWatcher::GitRepositoryWatcher(QObject *parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_updateTimer(new QTimer(this))
    , m_cleanupTimer(new QTimer(this))
    , m_watchEvents(0)
    , m_updateEvents(0)
{
    // 配置防抖更新定时器
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(UPDATE_DELAY_MS);
    connect(m_updateTimer, &QTimer::timeout, this, &GitRepositoryWatcher::onDelayedUpdate);

    // 配置清理定时器
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL_MS);
    connect(m_cleanupTimer, &QTimer::timeout, this, &GitRepositoryWatcher::onCleanupPaths);
    m_cleanupTimer->start();

    // 连接文件系统监控信号
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &GitRepositoryWatcher::onFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged, this, &GitRepositoryWatcher::onDirectoryChanged);

    qDebug() << "[GitRepositoryWatcher] Initialized with update delay" << UPDATE_DELAY_MS << "ms";
}

GitRepositoryWatcher::~GitRepositoryWatcher()
{
    qDebug() << "[GitRepositoryWatcher] Destroyed, stats - Watch events:" << m_watchEvents 
             << "Update events:" << m_updateEvents;
}

void GitRepositoryWatcher::addRepository(const QString &repositoryPath)
{
    if (!isValidRepository(repositoryPath)) {
        qWarning() << "[GitRepositoryWatcher::addRepository] Invalid repository:" << repositoryPath;
        return;
    }

    if (m_repositories.contains(repositoryPath)) {
        qDebug() << "[GitRepositoryWatcher::addRepository] Repository already watched:" << repositoryPath;
        return;
    }

    m_repositories.insert(repositoryPath);
    setupRepositoryWatching(repositoryPath);
    
    qDebug() << "[GitRepositoryWatcher::addRepository] Added repository:" << repositoryPath;
    Q_EMIT repositoryDiscovered(repositoryPath);
}

void GitRepositoryWatcher::removeRepository(const QString &repositoryPath)
{
    if (!m_repositories.contains(repositoryPath)) {
        return;
    }

    removeRepositoryWatching(repositoryPath);
    m_repositories.remove(repositoryPath);
    m_repoFiles.remove(repositoryPath);
    m_repoDirs.remove(repositoryPath);
    
    qDebug() << "[GitRepositoryWatcher::removeRepository] Removed repository:" << repositoryPath;
}

QStringList GitRepositoryWatcher::getWatchedRepositories() const
{
    return m_repositories.values();
}

bool GitRepositoryWatcher::isWatching(const QString &repositoryPath) const
{
    return m_repositories.contains(repositoryPath);
}

QHash<QString, QVariant> GitRepositoryWatcher::getWatcherStats() const
{
    QHash<QString, QVariant> stats;
    stats["watchedRepositories"] = m_repositories.size();
    stats["watchedFiles"] = m_fileWatcher->files().size();
    stats["watchedDirectories"] = m_fileWatcher->directories().size();
    stats["watchEvents"] = m_watchEvents;
    stats["updateEvents"] = m_updateEvents;
    stats["pendingUpdates"] = m_pendingUpdates.size();
    
    return stats;
}

void GitRepositoryWatcher::onFileChanged(const QString &path)
{
    m_watchEvents++;
    
    QString repositoryPath = getRepositoryFromPath(path);
    if (repositoryPath.isEmpty()) {
        return;
    }

    qDebug() << "[GitRepositoryWatcher::onFileChanged] File changed:" << path 
             << "in repository:" << repositoryPath;
    
    scheduleUpdate(repositoryPath);
}

void GitRepositoryWatcher::onDirectoryChanged(const QString &path)
{
    m_watchEvents++;
    
    QString repositoryPath = getRepositoryFromPath(path);
    if (repositoryPath.isEmpty()) {
        return;
    }

    qDebug() << "[GitRepositoryWatcher::onDirectoryChanged] Directory changed:" << path 
             << "in repository:" << repositoryPath;

    // 检查是否有新的子目录需要监控
    checkAndAddNewDirectories(path, repositoryPath);
    
    scheduleUpdate(repositoryPath);
}

void GitRepositoryWatcher::onDelayedUpdate()
{
    QSet<QString> repositoriesToUpdate = m_pendingUpdates;
    m_pendingUpdates.clear();
    
    for (const QString &repositoryPath : repositoriesToUpdate) {
        m_updateEvents++;
        qDebug() << "[GitRepositoryWatcher::onDelayedUpdate] Triggering update for repository:" << repositoryPath;
        Q_EMIT repositoryChanged(repositoryPath);
    }
}

void GitRepositoryWatcher::onCleanupPaths()
{
    // 清理无效的监控路径
    QStringList invalidFiles;
    QStringList invalidDirs;
    
    for (const QString &file : m_fileWatcher->files()) {
        if (!QFileInfo::exists(file)) {
            invalidFiles.append(file);
        }
    }
    
    for (const QString &dir : m_fileWatcher->directories()) {
        if (!QFileInfo::exists(dir)) {
            invalidDirs.append(dir);
        }
    }
    
    if (!invalidFiles.isEmpty()) {
        m_fileWatcher->removePaths(invalidFiles);
        qDebug() << "[GitRepositoryWatcher::onCleanupPaths] Removed" << invalidFiles.size() << "invalid files";
    }
    
    if (!invalidDirs.isEmpty()) {
        m_fileWatcher->removePaths(invalidDirs);
        qDebug() << "[GitRepositoryWatcher::onCleanupPaths] Removed" << invalidDirs.size() << "invalid directories";
    }
}

void GitRepositoryWatcher::setupRepositoryWatching(const QString &repositoryPath)
{
    // 监控Git元数据文件
    QStringList gitFiles = getGitMetadataFiles(repositoryPath);
    addWatchPaths(gitFiles, true);
    m_repoFiles[repositoryPath] = gitFiles;
    
    // 监控重要目录
    QStringList importantDirs = getImportantDirectories(repositoryPath);
    addWatchPaths(importantDirs, false);
    m_repoDirs[repositoryPath] = importantDirs;
    
    qDebug() << "[GitRepositoryWatcher::setupRepositoryWatching] Set up watching for repository:" 
             << repositoryPath << "with" << gitFiles.size() << "files and" 
             << importantDirs.size() << "directories";
}

void GitRepositoryWatcher::removeRepositoryWatching(const QString &repositoryPath)
{
    // 移除文件监控
    if (m_repoFiles.contains(repositoryPath)) {
        const QStringList &files = m_repoFiles[repositoryPath];
        QStringList validFiles;
        for (const QString &file : files) {
            if (m_fileWatcher->files().contains(file)) {
                validFiles.append(file);
            }
        }
        if (!validFiles.isEmpty()) {
            m_fileWatcher->removePaths(validFiles);
        }
    }
    
    // 移除目录监控
    if (m_repoDirs.contains(repositoryPath)) {
        const QStringList &dirs = m_repoDirs[repositoryPath];
        QStringList validDirs;
        for (const QString &dir : dirs) {
            if (m_fileWatcher->directories().contains(dir)) {
                validDirs.append(dir);
            }
        }
        if (!validDirs.isEmpty()) {
            m_fileWatcher->removePaths(validDirs);
        }
    }
}

QStringList GitRepositoryWatcher::getGitMetadataFiles(const QString &repositoryPath) const
{
    QStringList gitFiles;
    QDir gitDir(repositoryPath + "/.git");
    
    if (!gitDir.exists()) {
        return gitFiles;
    }
    
    // 关键的Git元数据文件
    const QStringList criticalFiles = {
        "index",        // 暂存区
        "HEAD",         // 当前分支指针
        "ORIG_HEAD",    // 原始HEAD
        "FETCH_HEAD",   // 拉取的HEAD
        "MERGE_HEAD",   // 合并的HEAD
        "config"        // 仓库配置
    };
    
    for (const QString &fileName : criticalFiles) {
        QString filePath = gitDir.absoluteFilePath(fileName);
        if (QFileInfo::exists(filePath)) {
            gitFiles.append(filePath);
        }
    }
    
    // refs目录下的引用文件
    QDir refsDir(gitDir.absoluteFilePath("refs"));
    if (refsDir.exists()) {
        QDirIterator it(refsDir.absolutePath(), QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QFileInfo fileInfo(filePath);
            if (fileInfo.isFile()) {
                gitFiles.append(filePath);
            }
        }
    }
    
    return gitFiles;
}

QStringList GitRepositoryWatcher::getImportantDirectories(const QString &repositoryPath) const
{
    QStringList directories;
    
    // 仓库根目录
    directories.append(repositoryPath);
    
    // .git目录
    QString gitDirPath = repositoryPath + "/.git";
    if (QFileInfo::exists(gitDirPath)) {
        directories.append(gitDirPath);
        directories.append(gitDirPath + "/refs");
        directories.append(gitDirPath + "/refs/heads");
        directories.append(gitDirPath + "/refs/remotes");
    }
    
    return directories;
}

QString GitRepositoryWatcher::getRepositoryFromPath(const QString &filePath) const
{
    for (const QString &repoPath : m_repositories) {
        if (filePath.startsWith(repoPath + "/") || filePath == repoPath) {
            return repoPath;
        }
    }
    return QString();
}

void GitRepositoryWatcher::scheduleUpdate(const QString &repositoryPath)
{
    m_pendingUpdates.insert(repositoryPath);
    
    if (!m_updateTimer->isActive()) {
        m_updateTimer->start();
    }
}

void GitRepositoryWatcher::addWatchPaths(const QStringList &paths, bool isFile)
{
    QStringList validPaths;
    
    for (const QString &path : paths) {
        if (QFileInfo::exists(path)) {
            if (isFile && shouldWatchFile(path)) {
                validPaths.append(path);
            } else if (!isFile && QFileInfo(path).isDir()) {
                validPaths.append(path);
            }
        }
    }
    
    if (!validPaths.isEmpty()) {
        m_fileWatcher->addPaths(validPaths);
    }
}

bool GitRepositoryWatcher::shouldWatchFile(const QString &filePath) const
{
    // 检查文件是否应该被监控
    QFileInfo fileInfo(filePath);
    
    // 忽略临时文件和锁文件
    QString fileName = fileInfo.fileName();
    if (fileName.endsWith(".tmp") || fileName.endsWith(".lock") || 
        fileName.startsWith("~") || fileName.startsWith(".#")) {
        return false;
    }
    
    return true;
}

void GitRepositoryWatcher::checkAndAddNewDirectories(const QString &changedDirPath, const QString &repositoryPath)
{
    QDir dir(changedDirPath);
    if (!dir.exists()) {
        return;
    }
    
    // 检查是否有新的子目录
    QStringList entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString subDirPath = dir.absoluteFilePath(entry);
        
        if (shouldWatchDirectory(subDirPath, repositoryPath) && 
            !m_fileWatcher->directories().contains(subDirPath)) {
            m_fileWatcher->addPath(subDirPath);
            qDebug() << "[GitRepositoryWatcher::checkAndAddNewDirectories] Added new directory:" << subDirPath;
        }
    }
}

bool GitRepositoryWatcher::shouldWatchDirectory(const QString &dirPath, const QString &repositoryPath) const
{
    Q_UNUSED(repositoryPath)
    
    // 忽略隐藏目录（除了.git）
    QFileInfo dirInfo(dirPath);
    QString dirName = dirInfo.fileName();
    
    if (dirName.startsWith(".") && dirName != ".git") {
        return false;
    }
    
    // 忽略常见的构建和临时目录
    const QStringList ignoreDirs = {
        "build", "dist", "node_modules", ".vscode", ".idea",
        "target", "bin", "obj", ".vs", "__pycache__"
    };
    
    if (ignoreDirs.contains(dirName)) {
        return false;
    }
    
    return true;
}

bool GitRepositoryWatcher::isValidRepository(const QString &repositoryPath) const
{
    if (repositoryPath.isEmpty()) {
        return false;
    }
    
    QFileInfo repoInfo(repositoryPath);
    if (!repoInfo.exists() || !repoInfo.isDir()) {
        return false;
    }
    
    // 检查是否为Git仓库
    QDir repoDir(repositoryPath);
    return repoDir.exists(".git");
}
