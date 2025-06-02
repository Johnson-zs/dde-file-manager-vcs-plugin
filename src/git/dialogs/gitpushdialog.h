#ifndef GITPUSHDIALOG_H
#define GITPUSHDIALOG_H

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

class GitOperationService;

/**
 * @brief 专业级Git推送对话框
 *
 * 提供完整的Git推送功能，包括：
 * - 仓库状态概览和未推送提交显示
 * - 远程仓库选择和目标分支映射
 * - 高级推送选项（强制推送、推送标签等）
 * - 安全机制和影响评估
 * - 实时操作进度和结果反馈
 */
class GitPushDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitPushDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    ~GitPushDialog();

    /**
     * @brief Git推送选项配置
     */
    struct PushOptions
    {
        QString remoteName;   // 远程仓库名称
        QString localBranch;   // 本地分支名称
        QString remoteBranch;   // 远程分支名称
        bool forceWithLease;   // 安全强制推送
        bool pushTags;   // 推送标签
        bool setUpstream;   // 设置上游分支
        bool pushAllBranches;   // 推送所有分支
        bool dryRun;   // 预演模式
    };

    /**
     * @brief 提交信息结构
     */
    struct CommitInfo
    {
        QString hash;   // 提交哈希
        QString shortHash;   // 短哈希
        QString message;   // 提交消息
        QString author;   // 作者
        QDateTime timestamp;   // 时间戳
        QStringList modifiedFiles;   // 修改的文件列表
    };

private slots:
    void onRemoteChanged();
    void onBranchChanged();
    void onForceToggled(bool enabled);
    void onTagsToggled(bool enabled);
    void onUpstreamToggled(bool enabled);
    void showRemoteManager();
    void previewChanges();
    void executePush();
    void executeDryRun();
    void onPushCompleted(bool success, const QString &message);
    void updateRepositoryStatus();
    void refreshRemoteStatus();

private:
    // === UI设置方法 ===
    void setupUI();
    void setupStatusGroup();
    void setupConfigGroup();
    void setupCommitsGroup();
    void setupButtonGroup();
    void setupConnections();

    // === 数据加载方法 ===
    void loadRepositoryInfo();
    void loadRemotes();
    void loadBranches();
    void loadRemoteBranches();
    void loadUnpushedCommits();
    void loadRemoteStatus();

    // === 验证和安全方法 ===
    void validatePushOptions();
    bool confirmForcePush();
    bool checkRemoteStatus();
    void showImpactAssessment();
    void showQuickPreview();
    void executePushWithOptions(const PushOptions &options);

    // === 辅助方法 ===
    void updateUI();
    void updateStatusLabels();
    void updateCommitsList();
    void enableControls(bool enabled);
    QString formatCommitInfo(const CommitInfo &commit) const;
    QString getStatusDescription() const;
    QString getFileChangesPreview() const;

    // === 成员变量 ===
    QString m_repositoryPath;
    GitOperationService *m_operationService;

    // === UI组件 ===
    // 状态组
    QGroupBox *m_statusGroup;
    QLabel *m_currentBranchLabel;
    QLabel *m_unpushedCountLabel;
    QLabel *m_remoteStatusLabel;
    QLabel *m_lastPushLabel;

    // 配置组
    QGroupBox *m_configGroup;
    QComboBox *m_remoteCombo;
    QComboBox *m_localBranchCombo;
    QComboBox *m_remoteBranchCombo;
    QCheckBox *m_forceCheckBox;
    QCheckBox *m_tagsCheckBox;
    QCheckBox *m_upstreamCheckBox;
    QCheckBox *m_allBranchesCheckBox;

    // 提交列表组
    QGroupBox *m_commitsGroup;
    QListWidget *m_commitsWidget;
    QLabel *m_commitsCountLabel;

    // 按钮组
    QPushButton *m_remoteManagerButton;
    QPushButton *m_previewButton;
    QPushButton *m_dryRunButton;
    QPushButton *m_pushButton;
    QPushButton *m_cancelButton;

    // 进度显示
    QProgressBar *m_progressBar;
    QLabel *m_progressLabel;

    // === 数据成员 ===
    QVector<CommitInfo> m_unpushedCommits;
    QStringList m_remotes;
    QStringList m_localBranches;
    QStringList m_remoteBranches;
    QString m_currentBranch;
    bool m_isOperationInProgress;
    bool m_isDryRunInProgress;
    QTimer *m_statusUpdateTimer;
};

#endif   // GITPUSHDIALOG_H
