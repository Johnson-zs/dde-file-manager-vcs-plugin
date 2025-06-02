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
 */
class GitLogDataManager : public QObject
{
    Q_OBJECT

public:
    struct CommitInfo {
        QString shortHash;
        QString fullHash;
        QString message;
        QString author;
        QString date;
        QString graphInfo;
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

    explicit GitLogDataManager(const QString &repositoryPath, QObject *parent = nullptr);
    ~GitLogDataManager() = default;

    // === 基础设置 ===
    void setRepositoryPath(const QString &path) { m_repositoryPath = path; }
    void setFilePath(const QString &path) { m_filePath = path; }
    QString repositoryPath() const { return m_repositoryPath; }
    QString filePath() const { return m_filePath; }

    // === 数据加载接口 ===
    bool loadCommitHistory(const QString &branch = QString(), int offset = 0, int limit = 100);
    bool loadBranches();
    bool loadCommitDetails(const QString &commitHash);
    bool loadCommitFiles(const QString &commitHash);
    bool loadFileChangeStats(const QString &commitHash);
    bool loadFileDiff(const QString &commitHash, const QString &filePath);

    // === 数据获取接口 ===
    QList<CommitInfo> getCommits() const { return m_commits; }
    BranchInfo getBranchInfo() const { return m_branchInfo; }
    QString getCommitDetails(const QString &commitHash) const;
    QList<FileChangeInfo> getCommitFiles(const QString &commitHash) const;
    QString getFileDiff(const QString &commitHash, const QString &filePath) const;

    // === 缓存管理 ===
    void clearCache();
    void clearCommitCache();
    void clearFileCache();
    int getCacheSize() const;

    // === 统计信息 ===
    int getTotalCommitsLoaded() const { return m_commits.size(); }
    bool hasMoreCommits() const { return m_hasMoreCommits; }

Q_SIGNALS:
    // 数据加载完成信号
    void commitHistoryLoaded(const QList<CommitInfo> &commits, bool append);
    void branchesLoaded(const BranchInfo &branchInfo);
    void commitDetailsLoaded(const QString &commitHash, const QString &details);
    void commitFilesLoaded(const QString &commitHash, const QList<FileChangeInfo> &files);
    void fileStatsLoaded(const QString &commitHash, const QList<FileChangeInfo> &files);
    void fileDiffLoaded(const QString &commitHash, const QString &filePath, const QString &diff);

    // 错误信号
    void dataLoadError(const QString &operation, const QString &error);

private Q_SLOTS:
    void onProcessFinished();

private:
    // === 辅助方法 ===
    bool executeGitCommand(const QStringList &args, QString &output, QString &error);
    QList<CommitInfo> parseCommitHistory(const QString &output);
    BranchInfo parseBranchInfo(const QString &branchOutput, const QString &tagOutput, const QString &currentBranch);
    QList<FileChangeInfo> parseCommitFiles(const QString &output);
    QList<FileChangeInfo> parseFileStats(const QString &output, const QList<FileChangeInfo> &existingFiles);
    
    // === 成员变量 ===
    QString m_repositoryPath;
    QString m_filePath;  // 如果只查看特定文件的历史
    
    // 数据存储
    QList<CommitInfo> m_commits;
    BranchInfo m_branchInfo;
    bool m_hasMoreCommits;
    
    // 缓存
    QHash<QString, QString> m_commitDetailsCache;
    QHash<QString, QList<FileChangeInfo>> m_commitFilesCache;
    QHash<QString, QString> m_fileDiffCache;  // "commitHash:filePath" -> diff
    
    // 异步处理
    QProcess *m_currentProcess;
    QString m_pendingOperation;
    QString m_pendingCommitHash;
    QString m_pendingFilePath;
    
    // 配置
    static const int DEFAULT_COMMIT_LIMIT = 100;
    static const int MAX_CACHE_SIZE = 1000;
};

#endif // GITLOGDATAMANAGER_H 