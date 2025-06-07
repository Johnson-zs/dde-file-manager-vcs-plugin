#ifndef GITFILESYSTEMWATCHER_H
#define GITFILESYSTEMWATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QStringList>

/**
 * @brief Git仓库实时文件系统监控器
 * 
 * 核心功能：
 * 1. 监控Git元数据文件变化（.git/index, .git/HEAD等）
 * 2. 监控工作目录文件变化（所有被Git跟踪的文件）
 * 3. 智能过滤和异步处理，避免性能问题
 * 4. 实时响应（100ms内）文件变化并触发更新
 */
class GitFileSystemWatcher : public QObject
{
    Q_OBJECT

public:
    explicit GitFileSystemWatcher(QObject *parent = nullptr);
    ~GitFileSystemWatcher();

    /**
     * @brief 添加Git仓库到监控列表
     * @param repositoryPath Git仓库根目录路径
     */
    void addRepository(const QString &repositoryPath);

    /**
     * @brief 从监控列表中移除Git仓库
     * @param repositoryPath Git仓库根目录路径
     */
    void removeRepository(const QString &repositoryPath);

    /**
     * @brief 获取当前监控的仓库列表
     * @return 仓库路径列表
     */
    QStringList getWatchedRepositories() const;

    /**
     * @brief 检查指定仓库是否正在被监控
     * @param repositoryPath 仓库路径
     * @return 是否正在监控
     */
    bool isWatching(const QString &repositoryPath) const;

Q_SIGNALS:
    /**
     * @brief 仓库发生变化时发出的信号
     * @param repositoryPath 发生变化的仓库路径
     */
    void repositoryChanged(const QString &repositoryPath);

private Q_SLOTS:
    /**
     * @brief 文件变化处理槽函数
     * @param path 变化的文件路径
     */
    void onFileChanged(const QString &path);

    /**
     * @brief 目录变化处理槽函数
     * @param path 变化的目录路径
     */
    void onDirectoryChanged(const QString &path);

    /**
     * @brief 防抖更新处理槽函数
     */
    void onDelayedUpdate();

    /**
     * @brief 定期清理无效路径
     */
    void onCleanupPaths();

private:
    /**
     * @brief 设置仓库监控
     * @param repositoryPath 仓库路径
     */
    void setupRepositoryWatching(const QString &repositoryPath);

    /**
     * @brief 移除仓库监控
     * @param repositoryPath 仓库路径
     */
    void removeRepositoryWatching(const QString &repositoryPath);

    /**
     * @brief 获取仓库的Git元数据文件
     * @param repositoryPath 仓库路径
     * @return Git文件路径列表
     */
    QStringList getGitMetadataFiles(const QString &repositoryPath) const;

    /**
     * @brief 获取仓库的被跟踪文件
     * @param repositoryPath 仓库路径
     * @return 被跟踪文件路径列表
     */
    QStringList getTrackedFiles(const QString &repositoryPath) const;

    /**
     * @brief 获取仓库的重要目录
     * @param repositoryPath 仓库路径
     * @return 重要目录路径列表
     */
    QStringList getImportantDirectories(const QString &repositoryPath) const;

    /**
     * @brief 检查文件是否应该被监控
     * @param filePath 文件路径
     * @return 是否应该监控
     */
    bool shouldWatchFile(const QString &filePath) const;

    /**
     * @brief 检查目录是否应该被监控
     * @param dirPath 目录路径
     * @param repositoryPath 仓库路径
     * @return 是否应该监控
     */
    bool shouldWatchDirectory(const QString &dirPath, const QString &repositoryPath) const;

    /**
     * @brief 从文件路径获取仓库路径
     * @param filePath 文件路径
     * @return 仓库路径，如果不属于任何监控仓库则返回空
     */
    QString getRepositoryFromPath(const QString &filePath) const;

    /**
     * @brief 调度仓库更新（防抖处理）
     * @param repositoryPath 仓库路径
     */
    void scheduleUpdate(const QString &repositoryPath);

    /**
     * @brief 批量添加监控路径
     * @param paths 路径列表
     * @param isFile 是否为文件路径
     */
    void addWatchPaths(const QStringList &paths, bool isFile = true);

    /**
     * @brief 检测并添加新建的子目录到监控
     * @param changedDirPath 发生变化的目录路径
     * @param repositoryPath 仓库路径
     */
    void checkAndAddNewDirectories(const QString &changedDirPath, const QString &repositoryPath);

private:
    QFileSystemWatcher *m_fileWatcher;           ///< Qt文件系统监控器
    QTimer *m_updateTimer;                       ///< 防抖更新定时器
    QTimer *m_cleanupTimer;                      ///< 清理定时器

    QSet<QString> m_repositories;                ///< 监控的仓库集合
    QSet<QString> m_pendingUpdates;              ///< 待处理更新的仓库集合
    
    QHash<QString, QStringList> m_repoFiles;     ///< 每个仓库的监控文件
    QHash<QString, QStringList> m_repoDirs;      ///< 每个仓库的监控目录

    // 配置常量
    static constexpr int UPDATE_DELAY_MS = 100;        ///< 更新延迟时间
    static constexpr int CLEANUP_INTERVAL_MS = 30000;  ///< 清理间隔时间
    static constexpr int MAX_FILES_PER_REPO = 5000;    ///< 每个仓库最大监控文件数
};

#endif // GITFILESYSTEMWATCHER_H
