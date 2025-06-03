#ifndef GITLOGDIALOG_H
#define GITLOGDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QScrollBar>
#include <QTimer>
#include <QKeyEvent>
#include <QEvent>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

// Include data manager for struct types
#include "gitlogdatamanager.h"

// Forward declarations for other components
class GitCommitDetailsWidget;
class GitLogSearchManager;
class GitLogContextMenuManager;
class LineNumberTextEdit;
class SearchableBranchSelector;
class GitFilePreviewDialog;
class CharacterAnimationWidget;

/**
 * @brief Git差异文本的语法高亮器
 *
 * 为diff文本提供颜色高亮，包括：
 * - 添加的行（绿色背景）
 * - 删除的行（红色背景）
 * - 行号信息（蓝色粗体）
 * - 文件路径（紫色粗体）
 * - 上下文行（灰色）
 */
class GitDiffSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit GitDiffSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    QTextCharFormat m_addedLineFormat;
    QTextCharFormat m_removedLineFormat;
    QTextCharFormat m_lineNumberFormat;
    QTextCharFormat m_filePathFormat;
    QTextCharFormat m_contextFormat;
};

/**
 * @brief 重构后的Git提交历史查看器对话框
 *
 * 采用组合模式，将原来的大类拆分为多个专门的组件：
 * - GitCommitDetailsWidget: 提交详情展示
 * - GitLogDataManager: 数据加载和缓存管理
 * - GitLogSearchManager: 搜索和过滤功能
 * - GitLogContextMenuManager: 右键菜单管理
 *
 * 主要职责简化为：
 * - UI布局管理
 * - 组件间的协调
 * - 用户交互的响应
 */
class GitLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitLogDialog(const QString &repositoryPath, const QString &filePath = QString(), QWidget *parent = nullptr);
    explicit GitLogDialog(const QString &repositoryPath, const QString &filePath, const QString &initialBranch, QWidget *parent = nullptr);
    ~GitLogDialog();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    // === 基础界面交互 ===
    void onCommitSelectionChanged();
    void onFileSelectionChanged();
    void onFileDoubleClicked(QTreeWidgetItem *item, int column);
    void onRefreshClicked();
    void onSettingsClicked();
    void onBranchSelectorChanged(const QString &branchName);
    void onSearchTextChanged();
    void onScrollValueChanged(int value);

    // === 数据管理器信号响应 ===
    void onCommitHistoryLoaded(const QList<GitLogDataManager::CommitInfo> &commits, bool append);
    void onBranchesLoaded(const GitLogDataManager::BranchInfo &branchInfo);
    void onCommitDetailsLoaded(const QString &commitHash, const QString &details);
    void onCommitFilesLoaded(const QString &commitHash, const QList<GitLogDataManager::FileChangeInfo> &files);
    void onFileStatsLoaded(const QString &commitHash, const QList<GitLogDataManager::FileChangeInfo> &files);
    void onFileDiffLoaded(const QString &commitHash, const QString &filePath, const QString &diff);
    void onDataLoadError(const QString &operation, const QString &error);
    void onRemoteStatusUpdated(const QString &branch);
    void onRemoteReferencesUpdated(const QString &branch, bool success);

    // === 搜索管理器信号响应 ===
    void onSearchStarted(const QString &searchText);
    void onSearchCompleted(const QString &searchText, int totalResults);
    void onSearchCleared();
    void onMoreDataNeeded();

    // === 右键菜单信号响应 ===
    void onGitOperationRequested(const QString &operation, const QStringList &args, bool needsConfirmation);
    void onCompareWithWorkingTreeRequested(const QString &commitHash);
    void onShowFileDiffRequested(const QString &commitHash, const QString &filePath);
    void onViewFileAtCommitRequested(const QString &commitHash, const QString &filePath);
    void onShowFileHistoryRequested(const QString &filePath);
    void onShowFileBlameRequested(const QString &filePath);
    void onOpenFileRequested(const QString &filePath);
    void onShowFileInFolderRequested(const QString &filePath);

    // === 辅助操作 ===
    void previewSelectedFile();
    void loadMoreCommitsIfNeeded();
    void refreshAfterOperation();
    void selectFirstLocalCommit();
    void loadCommitsForInitialBranch(const QString &branch);

    // === 加载状态管理 ===
    void showLoadingStatus(const QString &message);  // 显示加载状态
    void hideLoadingStatus();                        // 隐藏加载状态

private:
    // === UI初始化 ===
    void initializeDialog(const QString &repositoryPath, const QString &filePath, const QString &initialBranch);
    void setupUI();
    void setupMainLayout();
    void setupToolbar();
    void setupCommitList();
    void setupFilesList();
    void setupDiffView();
    void setupInfiniteScroll();
    void connectSignals();

    // === 辅助方法 ===
    QString getCurrentSelectedCommitHash() const;
    QString getCurrentSelectedFilePath() const;
    void populateCommitList(const QList<GitLogDataManager::CommitInfo> &commits, bool append);
    void populateFilesList(const QList<GitLogDataManager::FileChangeInfo> &files);
    QIcon getFileStatusIcon(const QString &status) const;
    QString formatChangeStats(int additions, int deletions) const;
    void setChangeStatsColor(QTreeWidgetItem *item, int additions, int deletions) const;

    // === 远程状态渲染方法 ===
    QIcon getRemoteStatusIcon(GitLogDataManager::RemoteStatus status) const;
    QColor getRemoteStatusColor(GitLogDataManager::RemoteStatus status) const;
    QString getRemoteStatusTooltip(GitLogDataManager::RemoteStatus status, const QString &remoteRef) const;
    QString getRemoteStatusText(GitLogDataManager::RemoteStatus status) const;
    QColor getCommitSourceColor(GitLogDataManager::CommitSource source) const;

    // === 基础成员变量 ===
    QString m_repositoryPath;
    QString m_filePath;
    QString m_initialBranch;

    // === 核心组件（组合模式） ===
    GitCommitDetailsWidget *m_commitDetailsWidget;
    GitLogDataManager *m_dataManager;
    GitLogSearchManager *m_searchManager;
    GitLogContextMenuManager *m_contextMenuManager;

    // === UI组件 ===
    // 工具栏
    QHBoxLayout *m_toolbarLayout;
    SearchableBranchSelector *m_branchSelector;
    QLineEdit *m_searchEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_settingsButton;
    QLabel *m_searchStatusLabel;

    // 主布局
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    // === 加载状态指示器 ===
    QWidget *m_loadingWidget;               // 加载状态容器
    CharacterAnimationWidget *m_loadingAnimation;  // 加载动画组件

    // 左侧：提交列表
    QTreeWidget *m_commitTree;
    QScrollBar *m_commitScrollBar;

    // 右中：修改文件列表
    QTreeWidget *m_changedFilesTree;

    // 右下：文件差异
    LineNumberTextEdit *m_diffView;
    QSyntaxHighlighter *m_diffHighlighter;

    // === 无限滚动相关 ===
    QTimer *m_loadTimer;
    bool m_isLoadingMore;
    static const int PRELOAD_THRESHOLD = 10;

    // === 文件预览 ===
    GitFilePreviewDialog *m_currentPreviewDialog;

    // === 功能控制选项 ===
    bool m_enableChangeStats;
};

#endif   // GITLOGDIALOG_H
