#include "gitlogdatamanager.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

GitLogDataManager::GitLogDataManager(const QString &repositoryPath, QObject *parent)
    : QObject(parent)
    , m_repositoryPath(repositoryPath)
    , m_hasMoreCommits(true)
    , m_currentProcess(nullptr)
{
    qDebug() << "[GitLogDataManager] Initialized for repository:" << repositoryPath;
}

bool GitLogDataManager::loadCommitHistory(const QString &branch, int offset, int limit)
{
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        qWarning() << "WARNING: [GitLogDataManager] Previous git process still running, skipping load";
        return false;
    }

    QStringList args;
    args << "log"
         << "--oneline"
         << "--graph"
         << "--pretty=format:%h|%s|%an|%ad|%H"
         << "--date=short"
         << QString("--skip=%1").arg(offset)
         << QString("--max-count=%1").arg(limit);

    // 如果指定了文件路径，只显示该文件的历史
    if (!m_filePath.isEmpty()) {
        QDir repoDir(m_repositoryPath);
        QString relativePath = repoDir.relativeFilePath(m_filePath);
        args << "--" << relativePath;
    }

    // 如果选择了特定分支
    if (!branch.isEmpty() && branch != "HEAD" && branch != tr("All Branches")) {
        args.insert(1, branch);
    }

    QString output, error;
    if (!executeGitCommand(args, output, error)) {
        Q_EMIT dataLoadError("Load Commit History", error);
        return false;
    }

    QList<CommitInfo> commits = parseCommitHistory(output);
    
    // 如果这是追加加载，添加到现有列表
    bool append = (offset > 0);
    if (append) {
        m_commits.append(commits);
    } else {
        m_commits = commits;
    }

    // 检查是否还有更多提交
    m_hasMoreCommits = (commits.size() == limit);

    Q_EMIT commitHistoryLoaded(commits, append);

    qInfo() << QString("INFO: [GitLogDataManager] Loaded %1 commits (total: %2, append: %3)")
                       .arg(commits.size())
                       .arg(m_commits.size())
                       .arg(append);

    return true;
}

bool GitLogDataManager::loadBranches()
{
    QString currentBranch;
    QString branchOutput;
    QString tagOutput;
    QString error;

    // 获取当前分支
    QStringList currentBranchArgs = {"branch", "--show-current"};
    if (executeGitCommand(currentBranchArgs, currentBranch, error)) {
        currentBranch = currentBranch.trimmed();
    }

    // 获取所有分支
    QStringList branchArgs = {"branch", "-a", "--format=%(refname:short)"};
    if (!executeGitCommand(branchArgs, branchOutput, error)) {
        Q_EMIT dataLoadError("Load Branches", error);
        return false;
    }

    // 获取所有标签
    QStringList tagArgs = {"tag", "-l"};
    executeGitCommand(tagArgs, tagOutput, error);  // 标签加载失败不是致命错误

    BranchInfo branchInfo = parseBranchInfo(branchOutput, tagOutput, currentBranch);
    m_branchInfo = branchInfo;

    Q_EMIT branchesLoaded(branchInfo);

    qInfo() << QString("INFO: [GitLogDataManager] Loaded %1 local branches, %2 remote branches, %3 tags")
                       .arg(branchInfo.localBranches.size())
                       .arg(branchInfo.remoteBranches.size())
                       .arg(branchInfo.tags.size());

    return true;
}

bool GitLogDataManager::loadCommitDetails(const QString &commitHash)
{
    // 检查缓存
    if (m_commitDetailsCache.contains(commitHash)) {
        Q_EMIT commitDetailsLoaded(commitHash, m_commitDetailsCache[commitHash]);
        return true;
    }

    QStringList args = {"show", "--format=fuller", "--no-patch", commitHash};
    QString output, error;

    if (!executeGitCommand(args, output, error)) {
        Q_EMIT dataLoadError("Load Commit Details", error);
        return false;
    }

    m_commitDetailsCache[commitHash] = output;
    Q_EMIT commitDetailsLoaded(commitHash, output);

    qDebug() << "[GitLogDataManager] Loaded commit details for:" << commitHash.left(8);
    return true;
}

bool GitLogDataManager::loadCommitFiles(const QString &commitHash)
{
    // 检查缓存
    if (m_commitFilesCache.contains(commitHash)) {
        Q_EMIT commitFilesLoaded(commitHash, m_commitFilesCache[commitHash]);
        return true;
    }

    QStringList args = {"show", "--name-status", "--format=", commitHash};
    QString output, error;

    if (!executeGitCommand(args, output, error)) {
        Q_EMIT dataLoadError("Load Commit Files", error);
        return false;
    }

    QList<FileChangeInfo> files = parseCommitFiles(output);
    m_commitFilesCache[commitHash] = files;
    Q_EMIT commitFilesLoaded(commitHash, files);

    qDebug() << "[GitLogDataManager] Loaded" << files.size() << "files for commit:" << commitHash.left(8);
    return true;
}

bool GitLogDataManager::loadFileChangeStats(const QString &commitHash)
{
    QStringList args = {"show", "--numstat", "--format=", commitHash};
    QString output, error;

    if (!executeGitCommand(args, output, error)) {
        Q_EMIT dataLoadError("Load File Stats", error);
        return false;
    }

    // 获取现有的文件列表（如果已缓存）
    QList<FileChangeInfo> existingFiles = m_commitFilesCache.value(commitHash);
    QList<FileChangeInfo> updatedFiles = parseFileStats(output, existingFiles);

    // 更新缓存
    m_commitFilesCache[commitHash] = updatedFiles;
    Q_EMIT fileStatsLoaded(commitHash, updatedFiles);

    qDebug() << "[GitLogDataManager] Loaded file stats for commit:" << commitHash.left(8);
    return true;
}

bool GitLogDataManager::loadFileDiff(const QString &commitHash, const QString &filePath)
{
    QString cacheKey = QString("%1:%2").arg(commitHash, filePath);

    // 检查缓存
    if (m_fileDiffCache.contains(cacheKey)) {
        Q_EMIT fileDiffLoaded(commitHash, filePath, m_fileDiffCache[cacheKey]);
        return true;
    }

    QStringList args = {"show", commitHash, "--", filePath};
    QString output, error;

    if (!executeGitCommand(args, output, error)) {
        Q_EMIT dataLoadError("Load File Diff", error);
        return false;
    }

    m_fileDiffCache[cacheKey] = output;
    Q_EMIT fileDiffLoaded(commitHash, filePath, output);

    qDebug() << "[GitLogDataManager] Loaded diff for file:" << filePath << "at commit:" << commitHash.left(8);
    return true;
}

QString GitLogDataManager::getCommitDetails(const QString &commitHash) const
{
    return m_commitDetailsCache.value(commitHash);
}

QList<GitLogDataManager::FileChangeInfo> GitLogDataManager::getCommitFiles(const QString &commitHash) const
{
    return m_commitFilesCache.value(commitHash);
}

QString GitLogDataManager::getFileDiff(const QString &commitHash, const QString &filePath) const
{
    QString cacheKey = QString("%1:%2").arg(commitHash, filePath);
    return m_fileDiffCache.value(cacheKey);
}

void GitLogDataManager::clearCache()
{
    m_commitDetailsCache.clear();
    m_commitFilesCache.clear();
    m_fileDiffCache.clear();
    qInfo() << "INFO: [GitLogDataManager] All caches cleared";
}

void GitLogDataManager::clearCommitCache()
{
    m_commits.clear();
    m_commitDetailsCache.clear();
    qInfo() << "INFO: [GitLogDataManager] Commit cache cleared";
}

void GitLogDataManager::clearFileCache()
{
    m_commitFilesCache.clear();
    m_fileDiffCache.clear();
    qInfo() << "INFO: [GitLogDataManager] File cache cleared";
}

int GitLogDataManager::getCacheSize() const
{
    return m_commitDetailsCache.size() + m_commitFilesCache.size() + m_fileDiffCache.size();
}

bool GitLogDataManager::executeGitCommand(const QStringList &args, QString &output, QString &error)
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", args);

    if (!process.waitForFinished(10000)) {  // 10秒超时
        error = process.errorString();
        return false;
    }

    if (process.exitCode() != 0) {
        error = QString::fromUtf8(process.readAllStandardError());
        return false;
    }

    output = QString::fromUtf8(process.readAllStandardOutput());
    return true;
}

QList<GitLogDataManager::CommitInfo> GitLogDataManager::parseCommitHistory(const QString &output)
{
    QList<CommitInfo> commits;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) continue;

        // 使用正则表达式解析commit信息
        QRegularExpression commitRegex(R"(([a-f0-9]{7,})\|(.+)\|(.+)\|(.+)\|([a-f0-9]{40})$)");
        QRegularExpressionMatch match = commitRegex.match(line);

        if (match.hasMatch()) {
            CommitInfo commit;
            commit.shortHash = match.captured(1);
            commit.message = match.captured(2).trimmed();
            commit.author = match.captured(3).trimmed();
            commit.date = match.captured(4).trimmed();
            commit.fullHash = match.captured(5);

            // 提取graph部分
            int commitDataStart = match.capturedStart();
            QString graphPart = line.left(commitDataStart).trimmed();
            if (graphPart.isEmpty()) {
                commit.graphInfo = "●";
            } else {
                QString cleanGraph = graphPart;
                cleanGraph.replace("*", "●");
                if (cleanGraph.length() > 10) {
                    cleanGraph = cleanGraph.left(8) + "…";
                }
                commit.graphInfo = cleanGraph;
            }

            commits.append(commit);
        }
    }

    return commits;
}

GitLogDataManager::BranchInfo GitLogDataManager::parseBranchInfo(const QString &branchOutput, const QString &tagOutput, const QString &currentBranch)
{
    BranchInfo info;
    info.currentBranch = currentBranch;

    // 解析分支
    QStringList allBranches = branchOutput.split('\n', Qt::SkipEmptyParts);
    for (const QString &branch : allBranches) {
        QString cleanBranch = branch.trimmed();
        if (cleanBranch.isEmpty() || cleanBranch.startsWith("origin/HEAD")) {
            continue;
        }

        if (cleanBranch.startsWith("origin/") || cleanBranch.contains("/")) {
            info.remoteBranches << cleanBranch;
        } else {
            info.localBranches << cleanBranch;
        }
    }

    // 解析标签
    if (!tagOutput.isEmpty()) {
        info.tags = tagOutput.split('\n', Qt::SkipEmptyParts);
    }

    return info;
}

QList<GitLogDataManager::FileChangeInfo> GitLogDataManager::parseCommitFiles(const QString &output)
{
    QList<FileChangeInfo> files;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) continue;

        QStringList parts = line.split('\t');
        if (parts.size() >= 2) {
            FileChangeInfo fileInfo;
            fileInfo.status = parts[0];
            fileInfo.filePath = parts[1];
            fileInfo.statsLoaded = false;  // 统计信息需要单独加载
            files.append(fileInfo);
        }
    }

    return files;
}

QList<GitLogDataManager::FileChangeInfo> GitLogDataManager::parseFileStats(const QString &output, const QList<FileChangeInfo> &existingFiles)
{
    QList<FileChangeInfo> result = existingFiles;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    // 创建文件路径到统计信息的映射
    QHash<QString, QPair<int, int>> fileStats;
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) continue;

        QStringList parts = line.split('\t');
        if (parts.size() >= 3) {
            QString additionsStr = parts[0];
            QString deletionsStr = parts[1];
            QString filePath = parts[2];

            // 处理二进制文件（显示为"-"）
            int additions = (additionsStr == "-") ? 0 : additionsStr.toInt();
            int deletions = (deletionsStr == "-") ? 0 : deletionsStr.toInt();

            fileStats[filePath] = qMakePair(additions, deletions);
        }
    }

    // 更新现有文件的统计信息
    for (int i = 0; i < result.size(); ++i) {
        if (fileStats.contains(result[i].filePath)) {
            QPair<int, int> stats = fileStats[result[i].filePath];
            result[i].additions = stats.first;
            result[i].deletions = stats.second;
            result[i].statsLoaded = true;
        }
    }

    return result;
} 