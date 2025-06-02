#ifndef GITPULLDIALOG_H
#define GITPULLDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QListWidget>
#include <QProgressBar>
#include <QTimer>
#include <QDateTime>
#include <QMessageBox>

#include "gitcommandexecutor.h"

class GitOperationService;
class CharacterAnimationWidget;

/**
 * @brief 智能Git拉取对话框
 * 
 * 提供完整的Git拉取功能，包括：
 * - 本地状态检测和远程更新预览
 * - 多种合并策略选择（Merge、Rebase、Fast-forward）
 * - 冲突预防和安全模式
 * - 自动化选项和操作历史
 * - 智能的错误处理和用户指导
 */
class GitPullDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitPullDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    ~GitPullDialog();

    /**
     * @brief 合并策略枚举
     */
    enum class MergeStrategy {
        Merge,              // 标准合并
        Rebase,             // 变基合并
        FastForwardOnly     // 仅快进合并
    };

    /**
     * @brief Git拉取选项配置
     */
    struct PullOptions {
        QString remoteName;           // 远程仓库名称
        QString remoteBranch;         // 远程分支名称
        MergeStrategy strategy;       // 合并策略
        bool fastForwardOnly;         // 仅快进合并
        bool prune;                   // 修剪远程分支
        bool autoStash;               // 自动stash
        bool recurseSubmodules;       // 递归子模块
        bool dryRun;                  // 预演模式
    };

    /**
     * @brief 远程更新信息结构
     */
    struct RemoteUpdateInfo {
        QString hash;                 // 提交哈希
        QString shortHash;            // 短哈希
        QString message;              // 提交消息
        QString author;               // 作者
        QDateTime timestamp;          // 时间戳
        QString remoteBranch;         // 远程分支
    };

private slots:
    void onRemoteChanged();
    void onStrategyChanged();
    void onAutoStashToggled(bool enabled);
    void showRemoteManager();
    void fetchUpdates();
    void stashAndPull();
    void executePull();
    void executeDryRun();
    void onPullCompleted(bool success, const QString &message);
    void handleConflicts();
    void updateLocalStatus();
    void refreshRemoteUpdates();
    
    // 延迟加载槽函数
    void delayedDataLoad();
    
    // 异步命令完成槽函数
    void onFetchCommandFinished(const QString &command, GitCommandExecutor::Result result, const QString &output, const QString &error);

private:
    // === UI设置方法 ===
    void setupUI();
    void setupStatusGroup();
    void setupConfigGroup();
    void setupUpdatesGroup();
    void setupButtonGroup();
    void setupConnections();

    // === 数据加载方法 ===
    void loadRepositoryInfo();
    void loadRepositoryInfoAsync();
    void loadRemotes();
    void loadBranches();
    void loadRemoteBranches();
    void loadRemoteUpdates();
    void loadActualRemoteUpdates();
    void checkLocalChanges();

    // === 验证和安全方法 ===
    void validatePullOptions();
    bool hasLocalChanges();
    bool hasUncommittedChanges();
    void previewPullResult();
    void executePullWithOptions(const PullOptions &options);

    // === 冲突处理方法 ===
    void abortMerge();

    // === 进度显示方法 ===
    void showProgress(const QString &message);
    void hideProgress();

    // === 辅助方法 ===
    void updateUI();
    void updateStatusLabels();
    void updateUpdatesList();
    void enableControls(bool enabled);
    QString formatUpdateInfo(const RemoteUpdateInfo &update) const;
    QString getLocalStatusDescription() const;
    QString getMergeStrategyDescription(MergeStrategy strategy) const;

    // === 成员变量 ===
    QString m_repositoryPath;
    GitOperationService *m_operationService;

    // === UI组件 ===
    // 状态组
    QGroupBox *m_statusGroup;
    QLabel *m_workingTreeLabel;
    QLabel *m_stagingAreaLabel;
    QLabel *m_localChangesLabel;
    QLabel *m_currentBranchLabel;

    // 配置组
    QGroupBox *m_configGroup;
    QComboBox *m_remoteCombo;
    QComboBox *m_remoteBranchCombo;
    QComboBox *m_strategyCombo;
    QCheckBox *m_ffOnlyCheckBox;
    QCheckBox *m_pruneCheckBox;
    QCheckBox *m_autoStashCheckBox;
    QCheckBox *m_submodulesCheckBox;

    // 更新预览组
    QGroupBox *m_updatesGroup;
    QListWidget *m_updatesWidget;
    QLabel *m_updatesCountLabel;
    QLabel *m_downloadStatsLabel;

    // 按钮组
    QPushButton *m_remoteManagerButton;
    QPushButton *m_fetchButton;
    QPushButton *m_stashPullButton;
    QPushButton *m_dryRunButton;
    QPushButton *m_pullButton;
    QPushButton *m_cancelButton;

    // 进度显示
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;
    CharacterAnimationWidget *m_animationWidget;

    // === 数据成员 ===
    QVector<RemoteUpdateInfo> m_remoteUpdates;
    QStringList m_remotes;
    QStringList m_remoteBranches;
    QString m_currentBranch;
    bool m_hasLocalChanges;
    bool m_hasUncommittedChanges;
    bool m_isOperationInProgress;
    bool m_isDryRunInProgress;
    bool m_isDataLoaded;
    QTimer *m_statusUpdateTimer;
};

#endif // GITPULLDIALOG_H 