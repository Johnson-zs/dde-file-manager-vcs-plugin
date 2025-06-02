#ifndef GITLOGDATAMANAGER_H
#define GITLOGDATAMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QProcess>
#include <QPair>

/**
 * @brief Git日志数据管理器
 * 
 * 专门负责Git相关数据的加载、解析和缓存管理：
 * - 提交历史加载
 * - 分支和标签数据
 * - 提交详情和文件变更
 * - 差异数据
 * - 智能缓存管理
 * - 远程分支状态跟踪
 */
class GitLogDataManager : public QObject
{
    Q_OBJECT

public:
    enum class RemoteStatus {
        Unknown,        // 未知状态
        Synchronized,   // 与远程同步
        Ahead,          // 本地领先
        Behind,         // 本地落后
        Diverged,       // 分叉状态
        NotTracked      // 未跟踪远程分支
    };

    enum class CommitSource {
        Local,          // 本地分支上的commit
        Remote,         // 仅存在于远程分支的commit
        Both            // 本地和远程都有的commit
    };

    struct CommitInfo {
        QString shortHash;
        QString fullHash;
        QString message;
        QString author;
        QString date;
        QString graphInfo;
        RemoteStatus remoteStatus = RemoteStatus::Unknown;  // 远程状态
        QString remoteRef;                                  // 对应的远程引用
        CommitSource source = CommitSource::Local;          // commit来源
        QStringList branches;                               // 包含此commit的分支列表
    };

    struct FileChangeInfo {
        QString status;
        QString filePath;
        int additions = 0;
        int deletions = 0;
        bool statsLoaded = false;
    };

    struct BranchInfo {
        QStringList localBranches;
        QStringList remoteBranches;
        QStringList tags;
        QString currentBranch;
    };

    struct BranchTrackingInfo {
        QString localBranch;
        QString remoteBranch;
        int aheadCount = 0;
        int behindCount = 0;
        bool hasRemote = false;
        QStringList allRemotes;  // 所有相关的remote
        QStringList allUpstreams;  // 所有上游分支
    };

    explicit GitLogDataManager(const QString &repositoryPath, QObject *parent = nullptr);
    ~GitLogDataManager() = default;

    // === 基础设置 ===
    void setRepositoryPath(const QString &path) { m_repositoryPath = path; }
    void setFilePath(const QString &path) { m_filePath = path; }
    QString repositoryPath() const { return m_repositoryPath; }
    QString filePath() const { return m_filePath; }

    // === 数据加载接口 ===
    bool loadCommitHistory(const QString &branch = QString(), int offset = 0, int limit = 100);
    bool loadCommitHistoryWithRemote(const QString &branch = QString(), int offset = 0, int limit = 100);  // 新增：包含远程commits的历史
    bool loadBranches();
    bool loadCommitDetails(const QString &commitHash);
    bool loadCommitFiles(const QString &commitHash);
    bool loadFileChangeStats(const QString &commitHash);
    bool loadFileDiff(const QString &commitHash, const QString &filePath);
    bool loadBranchTrackingInfo(const QString &branch);
    bool loadAllRemoteTrackingInfo(const QString &branch);  // 新增：加载所有remote信息
    void updateCommitRemoteStatus(const QString &branch);
    bool shouldLoadRemoteCommits(const QString &branch);  // 移到public：判断是否需要加载远程commits

    // === 智能远程引用更新 ===
    bool shouldUpdateRemoteReferences(const QString &branch);  // 检查是否需要更新远程引用
    bool updateRemoteReferences(const QString &branch);        // 更新远程引用
    bool updateRemoteReferencesAsync(const QString &branch);   // 异步更新远程引用

    // === 数据获取接口 ===
    QList<CommitInfo> getCommits() const { return m_commits; }
    BranchInfo getBranchInfo() const { return m_branchInfo; }
    BranchTrackingInfo getBranchTrackingInfo() const { return m_trackingInfo; }
    QString getCommitDetails(const QString &commitHash) const;
    QList<FileChangeInfo> getCommitFiles(const QString &commitHash) const;
    QString getFileDiff(const QString &commitHash, const QString &filePath) const;

    // === Git命令执行 ===
    bool executeGitCommand(const QStringList &args, QString &output, QString &error);

    // === 缓存管理 ===
    void clearCache();
    void clearCommitCache();
    void clearFileCache();
    void clearRemoteRefTimestampCache();  // 新增：清除远程引用时间戳缓存
    int getCacheSize() const;

    // === 统计信息 ===
    int getTotalCommitsLoaded() const { return m_commits.size(); }
    bool hasMoreCommits() const { return m_hasMoreCommits; }

Q_SIGNALS:
    /**
     * @brief 提交历史加载完成信号
     * @param commits 提交信息列表
     * @param append 是否为追加模式
     */
    void commitHistoryLoaded(const QList<CommitInfo> &commits, bool append = false);

    /**
     * @brief 分支信息加载完成信号
     * @param branchInfo 分支信息
     */
    void branchesLoaded(const BranchInfo &branchInfo);

    /**
     * @brief 提交详情加载完成信号
     * @param commitHash 提交哈希
     * @param details 详细信息
     */
    void commitDetailsLoaded(const QString &commitHash, const QString &details);

    /**
     * @brief 提交文件列表加载完成信号
     * @param commitHash 提交哈希
     * @param files 文件变更信息列表
     */
    void commitFilesLoaded(const QString &commitHash, const QList<FileChangeInfo> &files);

    /**
     * @brief 文件统计信息加载完成信号
     * @param commitHash 提交哈希
     * @param files 包含统计信息的文件列表
     */
    void fileStatsLoaded(const QString &commitHash, const QList<FileChangeInfo> &files);

    /**
     * @brief 文件差异加载完成信号
     * @param commitHash 提交哈希
     * @param filePath 文件路径
     * @param diff 差异内容
     */
    void fileDiffLoaded(const QString &commitHash, const QString &filePath, const QString &diff);

    /**
     * @brief 远程状态更新完成信号
     * @param branch 分支名称
     */
    void remoteStatusUpdated(const QString &branch);

    /**
     * @brief 远程引用更新完成信号
     * @param branch 分支名称
     * @param success 是否成功
     */
    void remoteReferencesUpdated(const QString &branch, bool success);

    /**
     * @brief 数据加载错误信号
     * @param operation 操作名称
     * @param error 错误信息
     */
    void dataLoadError(const QString &operation, const QString &error);

private Q_SLOTS:
    void onProcessFinished();

private:
    // === 辅助方法 ===
    QList<CommitInfo> parseCommitHistory(const QString &output);
    BranchInfo parseBranchInfo(const QString &branchOutput, const QString &tagOutput, const QString &currentBranch);
    QList<FileChangeInfo> parseCommitFiles(const QString &output);
    QList<FileChangeInfo> parseFileStats(const QString &output, const QList<FileChangeInfo> &existingFiles);
    BranchTrackingInfo parseBranchTrackingInfo(const QString &output, const QString &branch);
    void assignRemoteStatusToCommits(const QStringList &aheadCommits, const QStringList &behindCommits);
    void assignMultiRemoteStatusToCommits(const QStringList &aheadCommits, const QStringList &behindCommits, const QString &remoteBranch);  // 新增：多remote状态分配
    void markCommitSources(QList<CommitInfo> &commits, const QString &localBranch, const QString &remoteBranch);  // 新增：标记commit来源
    
    // === 成员变量 ===
    QString m_repositoryPath;
    QString m_filePath;  // 如果只查看特定文件的历史
    
    // 数据存储
    QList<CommitInfo> m_commits;
    BranchInfo m_branchInfo;
    BranchTrackingInfo m_trackingInfo;
    bool m_hasMoreCommits;
    
    // 缓存
    QHash<QString, QString> m_commitDetailsCache;
    QHash<QString, QList<FileChangeInfo>> m_commitFilesCache;
    QHash<QString, QString> m_fileDiffCache;  // "commitHash:filePath" -> diff
    QHash<QString, BranchTrackingInfo> m_trackingInfoCache;  // branch -> tracking info
    QHash<QString, qint64> m_remoteRefTimestampCache;       // branch -> last update timestamp
    
    // 异步处理
    QProcess *m_currentProcess;
    QString m_pendingOperation;
    QString m_pendingCommitHash;
    QString m_pendingFilePath;
    
    // 配置
    static const int DEFAULT_COMMIT_LIMIT = 100;
    static const int MAX_CACHE_SIZE = 1000;
    static const int REMOTE_REF_UPDATE_INTERVAL_MINUTES = 30;  // 远程引用更新间隔（分钟）
    static const int GIT_FETCH_TIMEOUT_SECONDS = 10;           // Git fetch超时时间（秒）
};

#endif // GITLOGDATAMANAGER_H 