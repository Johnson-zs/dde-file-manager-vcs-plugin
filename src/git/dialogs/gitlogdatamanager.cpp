#include "gitlogdatamanager.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QProcess>

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

    // 检查是否需要加载远程commits
    if (shouldLoadRemoteCommits(branch)) {
        qInfo() << "INFO: [GitLogDataManager] Branch might be behind remote, loading with remote commits";
        return loadCommitHistoryWithRemote(branch, offset, limit);
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
    if (!branch.isEmpty() && branch != "HEAD") {
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

    // 重要修复：对于初始加载和追加加载都要更新远程状态
    if (!branch.isEmpty() && branch != "HEAD") {
        qInfo() << "INFO: [GitLogDataManager] Loading commit history completed, updating remote status for branch:" << branch << "(append:" << append << ")";
        updateCommitRemoteStatus(branch);
    }

    qInfo() << QString("INFO: [GitLogDataManager] Loaded %1 commits (total: %2, append: %3)")
                       .arg(commits.size())
                       .arg(m_commits.size())
                       .arg(append);

    return true;
}

bool GitLogDataManager::loadCommitHistoryWithRemote(const QString &branch, int offset, int limit)
{
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        qWarning() << "WARNING: [GitLogDataManager] Previous git process still running, skipping load";
        return false;
    }

    // 先加载跟踪信息以获取远程分支
    if (!loadAllRemoteTrackingInfo(branch)) {
        qWarning() << "WARNING: [GitLogDataManager] Failed to load tracking info, falling back to local only";
        return loadCommitHistory(branch, offset, limit);
    }

    if (!m_trackingInfo.hasRemote) {
        qInfo() << "INFO: [GitLogDataManager] No remote tracking for branch, loading local only";
        return loadCommitHistory(branch, offset, limit);
    }

    // === 修复：使用原来的混合加载保持时间排序，但改进标记逻辑 ===
    QString localBranch = branch.isEmpty() ? "HEAD" : branch;
    QString remoteBranch = m_trackingInfo.remoteBranch;
    
    qInfo() << "INFO: [GitLogDataManager] Loading commits with local:" << localBranch << "and remote:" << remoteBranch;

    // === 使用原来的git log命令保持正确的时间排序 ===
    QStringList args;
    args << "log"
         << "--oneline"
         << "--graph"
         << "--pretty=format:%h|%s|%an|%ad|%H"
         << "--date=short"
         << QString("--skip=%1").arg(offset)
         << QString("--max-count=%1").arg(limit)
         << localBranch << remoteBranch;  // 恢复原来的混合方式

    // 如果指定了文件路径，只显示该文件的历史
    if (!m_filePath.isEmpty()) {
        QDir repoDir(m_repositoryPath);
        QString relativePath = repoDir.relativeFilePath(m_filePath);
        args << "--" << relativePath;
    }

    QString output, error;
    if (!executeGitCommand(args, output, error)) {
        qWarning() << "WARNING: [GitLogDataManager] Failed to load with remote, falling back to local only";
        return loadCommitHistory(branch, offset, limit);
    }

    QList<CommitInfo> commits = parseCommitHistory(output);
    
    // === 关键修复：先获取本地HEAD的hash，用于后续选中 ===
    QString localHeadHash;
    QStringList headArgs = {"rev-parse", "HEAD"};
    QString headOutput, headError;
    if (executeGitCommand(headArgs, headOutput, headError)) {
        localHeadHash = headOutput.trimmed();
        qInfo() << "INFO: [GitLogDataManager] Local HEAD hash:" << localHeadHash.left(8);
    }

    // === 增强：标记每个commit的来源和是否为本地HEAD ===
    markCommitSources(commits, localBranch, remoteBranch);
    
    // 标记本地HEAD commit
    for (auto &commit : commits) {
        if (commit.fullHash == localHeadHash) {
            commit.isLocalHead = true;  // 需要在CommitInfo中添加这个字段
            qInfo() << "INFO: [GitLogDataManager] Marked local HEAD commit:" << commit.shortHash;
            break;
        }
    }
    
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

    // 更新远程状态
    if (!branch.isEmpty() && branch != "HEAD") {
        qInfo() << "INFO: [GitLogDataManager] Loading commit history with remote completed, updating remote status for branch:" << branch << "(append:" << append << ")";
        updateCommitRemoteStatus(branch);
    }

    qInfo() << QString("INFO: [GitLogDataManager] Loaded %1 commits with remote (total: %2, append: %3)")
                       .arg(commits.size())
                       .arg(m_commits.size())
                       .arg(append);

    return true;
}

bool GitLogDataManager::shouldLoadRemoteCommits(const QString &branch)
{
    if (branch.isEmpty() || branch == "HEAD") {
        return false;
    }

    // 快速检查是否有远程跟踪分支
    if (!loadAllRemoteTrackingInfo(branch)) {
        return false;
    }

    if (!m_trackingInfo.hasRemote) {
        return false;
    }

    qInfo() << "INFO: [GitLogDataManager] Branch" << branch << "has remote tracking:" << m_trackingInfo.remoteBranch;

    // 修复：更激进的策略 - 只要有远程跟踪分支就加载远程commits
    // 这样用户总能看到完整的本地+远程历史，类似GitKraken/VSCode
    qInfo() << "INFO: [GitLogDialog] Branch" << branch << "has remote tracking, will load complete local+remote history";
    return true;

    // 注释掉原来的复杂检测逻辑，采用更简单直接的方案
    /*
    // 检查本地是否落后于远程
    QStringList checkArgs = {"rev-list", "--count", 
                            QString("%1..%2").arg(branch, m_trackingInfo.remoteBranch)};
    QString output, error;
    int behindCount = 0;
    
    if (executeGitCommand(checkArgs, output, error)) {
        behindCount = output.trimmed().toInt();
        if (behindCount > 0) {
            qInfo() << "INFO: [GitLogDataManager] Branch" << branch << "is" << behindCount << "commits behind remote, will load remote commits";
            return true;
        }
    }

    // 即使不落后，如果用户reset hard了，也应该显示远程commits
    // 检查是否有远程领先的commits（这意味着可能用户做了reset）
    QStringList aheadCheckArgs = {"rev-list", "--count", 
                                 QString("%1..%2").arg(m_trackingInfo.remoteBranch, branch)};
    if (executeGitCommand(aheadCheckArgs, output, error)) {
        int aheadCount = output.trimmed().toInt();
        if (aheadCount == 0 && behindCount == 0) {
            // 如果既不领先也不落后，但有远程分支，仍然应该显示远程commits
            // 这样可以让用户看到完整的历史
            qInfo() << "INFO: [GitLogDataManager] Branch" << branch << "is synchronized with remote, will still load remote commits for complete view";
            return true;
        }
    }

    return false;
    */
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
    m_trackingInfoCache.clear();
    m_remoteRefTimestampCache.clear();
    qInfo() << "INFO: [GitLogDataManager] All caches cleared";
}

void GitLogDataManager::clearCommitCache()
{
    m_commits.clear();
    m_commitDetailsCache.clear();
    // 清除跟踪信息，因为commit状态会改变
    m_trackingInfoCache.clear();
    m_remoteRefTimestampCache.clear();  // 新增：清除远程引用时间戳缓存
    qInfo() << "INFO: [GitLogDataManager] Commit cache cleared";
}

void GitLogDataManager::clearFileCache()
{
    m_commitFilesCache.clear();
    m_fileDiffCache.clear();
    qInfo() << "INFO: [GitLogDataManager] File cache cleared";
}

void GitLogDataManager::clearRemoteRefTimestampCache()
{
    m_remoteRefTimestampCache.clear();
    qInfo() << "INFO: [GitLogDataManager] Remote reference timestamp cache cleared";
}

int GitLogDataManager::getCacheSize() const
{
    return m_commitDetailsCache.size() + m_commitFilesCache.size() + m_fileDiffCache.size() + m_trackingInfoCache.size();
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

bool GitLogDataManager::loadBranchTrackingInfo(const QString &branch)
{
    if (branch.isEmpty()) {
        qWarning() << "WARNING: [GitLogDataManager] Empty branch name for tracking info";
        return false;
    }

    // 检查缓存
    if (m_trackingInfoCache.contains(branch)) {
        m_trackingInfo = m_trackingInfoCache[branch];
        qDebug() << "[GitLogDataManager] Using cached tracking info for branch:" << branch;
        return true;
    }

    // 获取分支跟踪信息
    QStringList args = {"for-each-ref", "--format=%(refname:short) %(upstream:short) %(upstream:track)", "refs/heads/" + branch};
    QString output, error;

    if (!executeGitCommand(args, output, error)) {
        // 如果没有跟踪分支，这不是错误
        if (error.contains("no upstream") || output.trimmed().isEmpty()) {
            m_trackingInfo = BranchTrackingInfo();
            m_trackingInfo.localBranch = branch;
            m_trackingInfo.hasRemote = false;
            m_trackingInfoCache[branch] = m_trackingInfo;
            
            qInfo() << "INFO: [GitLogDataManager] Branch" << branch << "has no tracking branch";
            return true;
        }
        
        Q_EMIT dataLoadError("Load Branch Tracking Info", error);
        return false;
    }

    m_trackingInfo = parseBranchTrackingInfo(output, branch);
    m_trackingInfoCache[branch] = m_trackingInfo;

    qInfo() << QString("INFO: [GitLogDataManager] Loaded tracking info for branch %1 -> %2 (ahead: %3, behind: %4)")
                       .arg(m_trackingInfo.localBranch)
                       .arg(m_trackingInfo.remoteBranch)
                       .arg(m_trackingInfo.aheadCount)
                       .arg(m_trackingInfo.behindCount);

    return true;
}

void GitLogDataManager::updateCommitRemoteStatus(const QString &branch)
{
    if (branch.isEmpty()) {
        qWarning() << "WARNING: [GitLogDataManager::updateCommitRemoteStatus] Empty branch name";
        return;
    }
    
    qInfo() << "INFO: [GitLogDataManager::updateCommitRemoteStatus] Starting remote status update for branch:" << branch;
    
    // 先尝试加载所有remote信息
    if (!loadAllRemoteTrackingInfo(branch)) {
        // 如果失败，回退到常规tracking info
        if (!loadBranchTrackingInfo(branch)) {
            qWarning() << "WARNING: [GitLogDataManager] Failed to load tracking info for branch:" << branch;
            return;
        }
    }

    if (!m_trackingInfo.hasRemote) {
        qInfo() << "INFO: [GitLogDataManager] Branch" << branch << "has no remote tracking, marking all commits as NotTracked";
        // 没有远程跟踪分支，设置所有commit为NotTracked
        for (auto &commit : m_commits) {
            commit.remoteStatus = RemoteStatus::NotTracked;
            commit.remoteRef.clear();
        }
        Q_EMIT remoteStatusUpdated(branch);
        return;
    }

    qInfo() << "INFO: [GitLogDataManager] Branch" << branch << "tracks remote:" << m_trackingInfo.remoteBranch;
    
    if (m_trackingInfo.allUpstreams.size() > 1) {
        qInfo() << "INFO: [GitLogDataManager] Multiple upstreams detected:" << m_trackingInfo.allUpstreams;
    }

    // 获取本地领先的提交
    QStringList aheadArgs = {"log", "--oneline", "--format=%H", 
                            QString("%1..%2").arg(m_trackingInfo.remoteBranch, m_trackingInfo.localBranch)};
    QString aheadOutput, error;
    QStringList aheadCommits;
    
    if (executeGitCommand(aheadArgs, aheadOutput, error)) {
        aheadCommits = aheadOutput.split('\n', Qt::SkipEmptyParts);
        qInfo() << "INFO: [GitLogDataManager] Found" << aheadCommits.size() << "ahead commits";
    } else {
        qWarning() << "WARNING: [GitLogDataManager] Failed to get ahead commits:" << error;
    }

    // 获取本地落后的提交  
    QStringList behindArgs = {"log", "--oneline", "--format=%H",
                             QString("%1..%2").arg(m_trackingInfo.localBranch, m_trackingInfo.remoteBranch)};
    QString behindOutput;
    QStringList behindCommits;
    
    if (executeGitCommand(behindArgs, behindOutput, error)) {
        behindCommits = behindOutput.split('\n', Qt::SkipEmptyParts);
        qInfo() << "INFO: [GitLogDataManager] Found" << behindCommits.size() << "behind commits";
    } else {
        qWarning() << "WARNING: [GitLogDataManager] Failed to get behind commits:" << error;
    }

    // 分配远程状态到提交
    if (m_trackingInfo.allUpstreams.size() > 1) {
        assignMultiRemoteStatusToCommits(aheadCommits, behindCommits, m_trackingInfo.remoteBranch);
    } else {
        assignRemoteStatusToCommits(aheadCommits, behindCommits);
    }

    Q_EMIT remoteStatusUpdated(branch);

    qInfo() << QString("INFO: [GitLogDataManager] Updated remote status for %1 commits (ahead: %2, behind: %3)")
                       .arg(m_commits.size())
                       .arg(aheadCommits.size())
                       .arg(behindCommits.size());
}

GitLogDataManager::BranchTrackingInfo GitLogDataManager::parseBranchTrackingInfo(const QString &output, const QString &branch)
{
    BranchTrackingInfo info;
    info.localBranch = branch;
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        info.hasRemote = false;
        return info;
    }

    QString line = lines.first().trimmed();
    QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    
    if (parts.size() >= 2) {
        info.remoteBranch = parts[1];
        info.hasRemote = !info.remoteBranch.isEmpty();
        
        // 解析ahead/behind信息：[ahead 2, behind 1] 或 [ahead 2] 或 [behind 1]
        if (parts.size() >= 3) {
            QString trackInfo = parts[2];
            
            QRegularExpression aheadRegex(R"(\[ahead (\d+))");
            QRegularExpressionMatch aheadMatch = aheadRegex.match(trackInfo);
            if (aheadMatch.hasMatch()) {
                info.aheadCount = aheadMatch.captured(1).toInt();
            }
            
            QRegularExpression behindRegex(R"(behind (\d+)\])");
            QRegularExpressionMatch behindMatch = behindRegex.match(trackInfo);
            if (behindMatch.hasMatch()) {
                info.behindCount = behindMatch.captured(1).toInt();
            }
        }
    } else {
        info.hasRemote = false;
    }

    return info;
}

void GitLogDataManager::assignRemoteStatusToCommits(const QStringList &aheadCommits, const QStringList &behindCommits)
{
    QSet<QString> aheadSet(aheadCommits.begin(), aheadCommits.end());
    QSet<QString> behindSet(behindCommits.begin(), behindCommits.end());
    
    for (auto &commit : m_commits) {
        commit.remoteRef = m_trackingInfo.remoteBranch;
        
        bool isAhead = aheadSet.contains(commit.fullHash);
        bool isBehind = behindSet.contains(commit.fullHash);
        
        if (isAhead && isBehind) {
            // 不应该发生，但以防万一
            commit.remoteStatus = RemoteStatus::Diverged;
        } else if (isAhead) {
            commit.remoteStatus = RemoteStatus::Ahead;
        } else if (isBehind) {
            commit.remoteStatus = RemoteStatus::Behind;
        } else {
            // 既不在ahead也不在behind列表中，说明已同步
            commit.remoteStatus = RemoteStatus::Synchronized;
        }
    }
    
    // 如果有分叉情况（同时有ahead和behind），标记为Diverged
    if (!aheadCommits.isEmpty() && !behindCommits.isEmpty()) {
        qInfo() << "INFO: [GitLogDataManager] Branch has diverged - ahead:" << aheadCommits.size() 
                << "behind:" << behindCommits.size();
    }
}

bool GitLogDataManager::loadAllRemoteTrackingInfo(const QString &branch)
{
    if (branch.isEmpty()) {
        qWarning() << "WARNING: [GitLogDataManager] Empty branch name for all remote tracking info";
        return false;
    }

    // === 新增：智能更新远程引用 ===
    if (shouldUpdateRemoteReferences(branch)) {
        qInfo() << "INFO: [GitLogDataManager] Attempting to update remote references for branch:" << branch;
        bool updateSuccess = updateRemoteReferences(branch);
        if (updateSuccess) {
            qInfo() << "INFO: [GitLogDataManager] Remote references updated successfully";
        } else {
            qWarning() << "WARNING: [GitLogDataManager] Failed to update remote references, continuing with existing data";
        }
    }

    // 获取所有remotes
    QStringList remotesArgs = {"remote"};
    QString remotesOutput, error;
    
    if (!executeGitCommand(remotesArgs, remotesOutput, error)) {
        qWarning() << "WARNING: [GitLogDataManager] Failed to get remotes:" << error;
        return false;
    }

    QStringList remotes = remotesOutput.split('\n', Qt::SkipEmptyParts);
    m_trackingInfo.allRemotes = remotes;
    m_trackingInfo.allUpstreams.clear();
    
    qInfo() << "INFO: [GitLogDataManager] Found" << remotes.size() << "remotes:" << remotes;

    // 检查每个remote上是否有对应的分支
    for (const QString &remote : remotes) {
        QString remoteBranch = remote + "/" + branch;
        
        // 检查远程分支是否存在
        QStringList checkArgs = {"show-ref", "--verify", "--quiet", "refs/remotes/" + remoteBranch};
        QString checkOutput;
        
        if (executeGitCommand(checkArgs, checkOutput, error)) {
            m_trackingInfo.allUpstreams.append(remoteBranch);
            qInfo() << "INFO: [GitLogDataManager] Found upstream branch:" << remoteBranch;
        }
    }

    // 如果只有一个upstream，使用常规逻辑
    if (m_trackingInfo.allUpstreams.size() == 1) {
        m_trackingInfo.remoteBranch = m_trackingInfo.allUpstreams.first();
        m_trackingInfo.hasRemote = true;
    } else if (m_trackingInfo.allUpstreams.size() > 1) {
        // 多个upstream，优先使用origin
        QString originBranch = "origin/" + branch;
        if (m_trackingInfo.allUpstreams.contains(originBranch)) {
            m_trackingInfo.remoteBranch = originBranch;
        } else {
            m_trackingInfo.remoteBranch = m_trackingInfo.allUpstreams.first();
        }
        m_trackingInfo.hasRemote = true;
        qInfo() << "INFO: [GitLogDataManager] Multiple upstreams found, using:" << m_trackingInfo.remoteBranch;
    } else {
        m_trackingInfo.hasRemote = false;
        qInfo() << "INFO: [GitLogDataManager] No upstream branches found for:" << branch;
    }

    m_trackingInfo.localBranch = branch;
    return true;
}

void GitLogDataManager::assignMultiRemoteStatusToCommits(const QStringList &aheadCommits, const QStringList &behindCommits, const QString &remoteBranch)
{
    QSet<QString> aheadSet(aheadCommits.begin(), aheadCommits.end());
    QSet<QString> behindSet(behindCommits.begin(), behindCommits.end());
    
    for (auto &commit : m_commits) {
        commit.remoteRef = remoteBranch;
        
        bool isAhead = aheadSet.contains(commit.fullHash);
        bool isBehind = behindSet.contains(commit.fullHash);
        
        if (isAhead && isBehind) {
            // 不应该发生，但以防万一
            commit.remoteStatus = RemoteStatus::Diverged;
        } else if (isAhead) {
            commit.remoteStatus = RemoteStatus::Ahead;
        } else if (isBehind) {
            commit.remoteStatus = RemoteStatus::Behind;
        } else {
            // 既不在ahead也不在behind列表中，说明已同步
            commit.remoteStatus = RemoteStatus::Synchronized;
        }
    }
    
    // 如果有分叉情况（同时有ahead和behind），标记为Diverged
    if (!aheadCommits.isEmpty() && !behindCommits.isEmpty()) {
        qInfo() << "INFO: [GitLogDataManager] Branch has diverged from" << remoteBranch << "- ahead:" << aheadCommits.size() 
                << "behind:" << behindCommits.size();
    }
    
    // 额外信息：显示多个upstreams情况
    if (m_trackingInfo.allUpstreams.size() > 1) {
        qInfo() << "INFO: [GitLogDataManager] Multiple upstreams available:" << m_trackingInfo.allUpstreams 
                << ", currently using:" << remoteBranch;
    }
}

void GitLogDataManager::markCommitSources(QList<CommitInfo> &commits, const QString &localBranch, const QString &remoteBranch)
{
    // 获取本地分支的commits
    QStringList localArgs = {"log", "--format=%H", localBranch};
    QString localOutput, error;
    QSet<QString> localCommits;
    
    if (executeGitCommand(localArgs, localOutput, error)) {
        QStringList localHashes = localOutput.split('\n', Qt::SkipEmptyParts);
        localCommits = QSet<QString>(localHashes.begin(), localHashes.end());
    }
    
    // 获取远程分支的commits
    QStringList remoteArgs = {"log", "--format=%H", remoteBranch};
    QString remoteOutput;
    QSet<QString> remoteCommits;
    
    if (executeGitCommand(remoteArgs, remoteOutput, error)) {
        QStringList remoteHashes = remoteOutput.split('\n', Qt::SkipEmptyParts);
        remoteCommits = QSet<QString>(remoteHashes.begin(), remoteHashes.end());
    }
    
    // 标记每个commit的来源
    for (auto &commit : commits) {
        bool inLocal = localCommits.contains(commit.fullHash);
        bool inRemote = remoteCommits.contains(commit.fullHash);
        
        if (inLocal && inRemote) {
            commit.source = CommitSource::Both;
        } else if (inLocal) {
            commit.source = CommitSource::Local;
        } else if (inRemote) {
            commit.source = CommitSource::Remote;
        } else {
            commit.source = CommitSource::Local; // 默认为本地
        }
        
        // 构建分支列表
        commit.branches.clear();
        if (inLocal) {
            commit.branches.append(localBranch);
        }
        if (inRemote) {
            commit.branches.append(remoteBranch);
        }
    }
    
    qInfo() << QString("INFO: [GitLogDataManager] Marked commit sources: %1 total commits, %2 local-only, %3 remote-only, %4 both")
                       .arg(commits.size())
                       .arg(std::count_if(commits.begin(), commits.end(), [](const CommitInfo &c) { return c.source == CommitSource::Local; }))
                       .arg(std::count_if(commits.begin(), commits.end(), [](const CommitInfo &c) { return c.source == CommitSource::Remote; }))
                       .arg(std::count_if(commits.begin(), commits.end(), [](const CommitInfo &c) { return c.source == CommitSource::Both; }));
}

bool GitLogDataManager::shouldUpdateRemoteReferences(const QString &branch)
{
    if (branch.isEmpty() || branch == "HEAD") {
        qDebug() << "[GitLogDataManager] Skipping remote update for empty or HEAD branch";
        return false;
    }

    // 检查是否有远程分支
    QStringList remotesArgs = {"remote"};
    QString remotesOutput, error;
    
    if (!executeGitCommand(remotesArgs, remotesOutput, error)) {
        qDebug() << "[GitLogDataManager] No remotes found, skipping remote update";
        return false;
    }

    QStringList remotes = remotesOutput.split('\n', Qt::SkipEmptyParts);
    if (remotes.isEmpty()) {
        qDebug() << "[GitLogDataManager] No remote repositories configured";
        return false;
    }

    // === 修改：检查全局FETCH_HEAD文件而不是特定分支 ===
    QString globalCacheKey = "FETCH_HEAD";
    if (m_remoteRefTimestampCache.contains(globalCacheKey)) {
        qint64 lastUpdate = m_remoteRefTimestampCache[globalCacheKey];
        qint64 currentTime = QDateTime::currentSecsSinceEpoch();
        qint64 timeDiff = currentTime - lastUpdate;
        
        if (timeDiff < (REMOTE_REF_UPDATE_INTERVAL_MINUTES * 60)) {
            qInfo() << QString("INFO: [GitLogDataManager] Remote references updated %1 seconds ago, skipping update")
                               .arg(timeDiff);
            return false;
        }
    }

    // 检查FETCH_HEAD文件的时间戳（全局fetch指标）
    QString fetchHeadPath = QString("%1/.git/FETCH_HEAD").arg(m_repositoryPath);
    QFileInfo fetchHeadInfo(fetchHeadPath);
    
    if (fetchHeadInfo.exists()) {
        QDateTime lastModified = fetchHeadInfo.lastModified();
        qint64 timeDiff = lastModified.secsTo(QDateTime::currentDateTime());
        
        if (timeDiff < (REMOTE_REF_UPDATE_INTERVAL_MINUTES * 60)) {
            qInfo() << QString("INFO: [GitLogDataManager] FETCH_HEAD is recent (%1 seconds old), skipping update")
                               .arg(timeDiff);
            // 更新内存缓存时间戳
            m_remoteRefTimestampCache[globalCacheKey] = QDateTime::currentSecsSinceEpoch();
            return false;
        }
    }

    qInfo() << QString("INFO: [GitLogDataManager] Remote references need updating (last fetch > %1 minutes ago)")
                       .arg(REMOTE_REF_UPDATE_INTERVAL_MINUTES);
    return true;
}

bool GitLogDataManager::updateRemoteReferences(const QString &branch)
{
    Q_UNUSED(branch)  // 现在不依赖特定分支
    
    qInfo() << "INFO: [GitLogDataManager] Updating all remote references with fetch --all";

    // 先尝试快速连接测试
    QStringList testArgs = {"ls-remote", "--exit-code", "origin"};
    QString testOutput, testError;
    
    QProcess testProcess;
    testProcess.setWorkingDirectory(m_repositoryPath);
    testProcess.start("git", testArgs);
    
    if (!testProcess.waitForFinished(3000)) {  // 3秒超时测试
        qWarning() << "WARNING: [GitLogDataManager] Remote connectivity test timeout";
        testProcess.kill();
        return false;
    }

    if (testProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(testProcess.readAllStandardError());
        qWarning() << "WARNING: [GitLogDataManager] Remote connectivity test failed:" << error;
        return false;
    }

    // === 修改：执行全面的fetch操作，类似GitPullDialog ===
    QStringList fetchArgs = {"fetch", "--all", "--prune", "--quiet"};
    QString fetchOutput, fetchError;
    
    QProcess fetchProcess;
    fetchProcess.setWorkingDirectory(m_repositoryPath);
    fetchProcess.start("git", fetchArgs);
    
    if (!fetchProcess.waitForFinished(GIT_FETCH_TIMEOUT_SECONDS * 1000)) {
        qWarning() << QString("WARNING: [GitLogDataManager] Git fetch --all timeout (%1s)")
                              .arg(GIT_FETCH_TIMEOUT_SECONDS);
        fetchProcess.kill();
        return false;
    }

    if (fetchProcess.exitCode() != 0) {
        QString error = QString::fromUtf8(fetchProcess.readAllStandardError());
        qWarning() << QString("WARNING: [GitLogDataManager] Git fetch --all failed: %1").arg(error);
        return false;
    }

    // === 修改：更新全局时间戳缓存 ===
    QString globalCacheKey = "FETCH_HEAD";
    m_remoteRefTimestampCache[globalCacheKey] = QDateTime::currentSecsSinceEpoch();

    qInfo() << "INFO: [GitLogDataManager] Successfully updated all remote references with fetch --all";
    return true;
}

bool GitLogDataManager::updateRemoteReferencesAsync(const QString &branch)
{
    if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
        qWarning() << "WARNING: [GitLogDataManager] Previous process still running, cannot start async remote update";
        return false;
    }

    // 先做快速检查（不依赖特定分支）
    if (!shouldUpdateRemoteReferences(branch)) {
        Q_EMIT remoteReferencesUpdated(branch, true);  // 认为成功，因为不需要更新
        return true;
    }

    qInfo() << "INFO: [GitLogDataManager] Starting async fetch --all for all remote references";

    // 创建新的进程用于异步fetch
    m_currentProcess = new QProcess(this);
    m_currentProcess->setWorkingDirectory(m_repositoryPath);
    
    // 连接信号
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, branch](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus)
                
                bool success = (exitCode == 0);
                if (success) {
                    // === 修改：更新全局时间戳缓存 ===
                    QString globalCacheKey = "FETCH_HEAD";
                    m_remoteRefTimestampCache[globalCacheKey] = QDateTime::currentSecsSinceEpoch();
                    qInfo() << "INFO: [GitLogDataManager] Async fetch --all succeeded";
                } else {
                    QString error = QString::fromUtf8(m_currentProcess->readAllStandardError());
                    qWarning() << QString("WARNING: [GitLogDataManager] Async fetch --all failed: %1").arg(error);
                }
                
                // 清理进程
                m_currentProcess->deleteLater();
                m_currentProcess = nullptr;
                
                Q_EMIT remoteReferencesUpdated(branch, success);
            });

    // === 修改：启动异步fetch --all ===
    QStringList fetchArgs = {"fetch", "--all", "--prune", "--quiet"};
    m_currentProcess->start("git", fetchArgs);
    
    // 设置超时
    QTimer::singleShot(GIT_FETCH_TIMEOUT_SECONDS * 1000, this, [this]() {
        if (m_currentProcess && m_currentProcess->state() != QProcess::NotRunning) {
            qWarning() << "WARNING: [GitLogDataManager] Async fetch --all timeout";
            m_currentProcess->kill();
        }
    });

    return true;
}

bool GitLogDataManager::loadCommitHistoryEnsureHead(const QString &branch, int initialLimit)
{
    qInfo() << QString("INFO: [GitLogDataManager] Loading commit history ensuring HEAD is included (branch: %1, initial limit: %2)")
                       .arg(branch.isEmpty() ? "HEAD" : branch)
                       .arg(initialLimit);

    // 首先获取本地HEAD的hash用于后续检查
    QString localHeadHash;
    QStringList headArgs = {"rev-parse", "HEAD"};
    QString headOutput, headError;
    if (executeGitCommand(headArgs, headOutput, headError)) {
        localHeadHash = headOutput.trimmed();
        qInfo() << "INFO: [GitLogDataManager] Local HEAD to ensure:" << localHeadHash.left(8);
    } else {
        qWarning() << "WARNING: [GitLogDataManager] Failed to get HEAD hash, falling back to normal loading";
        return loadCommitHistory(branch, 0, initialLimit);
    }

    // 先尝试标准加载
    bool hasRemoteTracking = false;
    if (!branch.isEmpty() && branch != "HEAD") {
        if (loadAllRemoteTrackingInfo(branch) && m_trackingInfo.hasRemote) {
            hasRemoteTracking = true;
        }
    }

    // 执行初始加载
    bool loadSuccess = false;
    if (hasRemoteTracking && shouldLoadRemoteCommits(branch)) {
        loadSuccess = loadCommitHistoryWithRemote(branch, 0, initialLimit);
    } else {
        loadSuccess = loadCommitHistory(branch, 0, initialLimit);
    }

    if (!loadSuccess) {
        qWarning() << "WARNING: [GitLogDataManager] Initial commit loading failed";
        return false;
    }

    // 检查本地HEAD是否在加载的commits中
    bool headFound = false;
    for (const auto &commit : m_commits) {
        if (commit.fullHash == localHeadHash) {
            headFound = true;
            qInfo() << QString("INFO: [GitLogDataManager] Local HEAD %1 found in initial %2 commits")
                               .arg(localHeadHash.left(8))
                               .arg(m_commits.size());
            break;
        }
    }

    // 如果没有找到HEAD，扩展加载范围
    if (!headFound) {
        qInfo() << QString("INFO: [GitLogDataManager] Local HEAD %1 not found in initial %2 commits, expanding search...")
                           .arg(localHeadHash.left(8))
                           .arg(m_commits.size());

        // 清除当前commits，使用更大的限制重新加载
        m_commits.clear();
        
        // 使用更大的限制，通常本地HEAD应该在前500个commits中
        int expandedLimit = qMax(initialLimit * 5, 500);
        
        if (hasRemoteTracking && shouldLoadRemoteCommits(branch)) {
            loadSuccess = loadCommitHistoryWithRemote(branch, 0, expandedLimit);
        } else {
            loadSuccess = loadCommitHistory(branch, 0, expandedLimit);
        }

        if (!loadSuccess) {
            qWarning() << "WARNING: [GitLogDataManager] Expanded commit loading failed";
            return false;
        }

        // 再次检查HEAD是否找到
        headFound = false;
        for (const auto &commit : m_commits) {
            if (commit.fullHash == localHeadHash) {
                headFound = true;
                qInfo() << QString("INFO: [GitLogDataManager] Local HEAD %1 found after expanding to %2 commits")
                                   .arg(localHeadHash.left(8))
                                   .arg(m_commits.size());
                break;
            }
        }

        if (!headFound) {
            qWarning() << QString("WARNING: [GitLogDataManager] Local HEAD %1 still not found even after expanding to %2 commits")
                                  .arg(localHeadHash.left(8))
                                  .arg(m_commits.size());
        }
    }

    return true;
} 