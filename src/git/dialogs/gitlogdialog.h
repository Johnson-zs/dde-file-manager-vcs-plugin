#ifndef GITLOGDIALOG_H
#define GITLOGDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QMenu>
#include <QAction>
#include <QScrollBar>
#include <QTimer>
#include <QStyledItemDelegate>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QInputDialog>

// Forward declarations
class GitOperationDialog;
class GitDialogManager;

/**
 * @brief Git提交历史查看器对话框 - GitKraken风格重构版本
 *
 * 实现现代化的三栏布局Git历史查看界面：
 * - 左侧：提交列表（支持图形化分支显示和智能无限滚动）
 * - 右上：提交详情（作者、时间、消息等）
 * - 右下：文件差异显示（语法高亮）
 *
 * 核心特性：
 * - GitKraken风格的用户界面
 * - 完整的右键菜单系统（reset、revert、cherry-pick等）
 * - 智能无限滚动替代分页
 * - 实时搜索和分支切换
 * - 文件级操作和差异查看
 */
class GitLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitLogDialog(const QString &repositoryPath, const QString &filePath = QString(), QWidget *parent = nullptr);
    ~GitLogDialog();

private Q_SLOTS:
    // === 核心界面交互 ===
    void onCommitSelectionChanged();
    void onRefreshClicked();
    void onBranchChanged();
    void onSearchTextChanged();
    void onScrollValueChanged(int value);

    // === 文件操作 ===
    void onFileSelectionChanged();
    void onFileDoubleClicked(QTreeWidgetItem *item, int column);

    // === 右键菜单 - Commit操作 ===
    void showCommitContextMenu(const QPoint &pos);
    void checkoutCommit();
    void createBranchFromCommit();
    void createTagFromCommit();
    void resetToCommit();
    void softResetToCommit();
    void mixedResetToCommit();
    void hardResetToCommit();
    void revertCommit();
    void cherryPickCommit();
    void showCommitDetails();
    void compareWithWorkingTree();
    void copyCommitHash();
    void copyShortHash();
    void copyCommitMessage();

    // === 右键菜单 - 文件操作 ===
    void showFileContextMenu(const QPoint &pos);
    void viewFileAtCommit();
    void showFileDiff();
    void showFileHistory();
    void showFileBlame();
    void openFile();
    void showInFolder();
    void copyFilePath();
    void copyFileName();

private:
    // === UI设置方法 ===
    void setupUI();
    void setupMainLayout();
    void setupCommitList();
    void setupRightPanel();
    void setupCommitDetails();
    void setupFilesList();
    void setupDiffView();
    void setupContextMenus();
    void setupCommitContextMenu();
    void setupFileContextMenu();
    void setupInfiniteScroll();

    // === 数据加载方法 ===
    void loadCommitHistory(bool append = false);
    void loadBranches();
    void loadCommitDetails(const QString &commitHash);
    void loadCommitFiles(const QString &commitHash);
    void loadFileDiff(const QString &commitHash, const QString &filePath);
    void loadMoreCommitsIfNeeded();
    void populateFilesList(const QStringList &fileLines);

    // === 搜索和过滤 ===
    void filterCommits(const QString &searchText);
    void highlightSearchResults(const QString &searchText);
    void startProgressiveSearch(const QString &searchText);
    void continueProgressiveSearch();
    void finishProgressiveSearch();
    void updateSearchStatus();
    void highlightItemMatches(QTreeWidgetItem *item, const QString &searchText);
    void clearItemHighlight(QTreeWidgetItem *item);

    // === 辅助方法 ===
    QString getCurrentSelectedCommitHash() const;
    QString getCurrentSelectedFilePath() const;
    void executeGitOperation(const QString &operation, const QStringList &args, bool needsConfirmation = false);
    void refreshAfterOperation();
    QIcon getFileStatusIcon(const QString &status) const;
    QString getFileStatusText(const QString &status) const;
    void applyDiffSyntaxHighlighting();
    void checkIfNeedMoreCommits();

    // === 成员变量 ===
    QString m_repositoryPath;
    QString m_filePath;

    // === UI组件 ===
    // 工具栏
    QComboBox *m_branchCombo;
    QLineEdit *m_searchEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_settingsButton;

    // 主布局
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    // 左侧：提交列表
    QTreeWidget *m_commitTree;
    QScrollBar *m_commitScrollBar;

    // 右上：提交详情
    QTextEdit *m_commitDetails;

    // 右中：修改文件列表
    QTreeWidget *m_changedFilesTree;

    // 右下：文件差异
    QTextEdit *m_diffView;
    QSyntaxHighlighter *m_diffHighlighter;

    // === 右键菜单 ===
    // Commit右键菜单
    QMenu *m_commitContextMenu;
    QAction *m_checkoutCommitAction;
    QAction *m_createBranchAction;
    QAction *m_createTagAction;
    QMenu *m_resetMenu;
    QAction *m_softResetAction;
    QAction *m_mixedResetAction;
    QAction *m_hardResetAction;
    QAction *m_revertCommitAction;
    QAction *m_cherryPickAction;
    QAction *m_showDetailsAction;
    QAction *m_compareWorkingTreeAction;
    QAction *m_copyHashAction;
    QAction *m_copyShortHashAction;
    QAction *m_copyMessageAction;

    // 文件右键菜单
    QMenu *m_fileContextMenu;
    QAction *m_viewFileAction;
    QAction *m_showFileDiffAction;
    QAction *m_showFileHistoryAction;
    QAction *m_showFileBlameAction;
    QAction *m_openFileAction;
    QAction *m_showFolderAction;
    QAction *m_copyFilePathAction;
    QAction *m_copyFileNameAction;

    // === 无限滚动相关 ===
    bool m_isLoadingMore;
    int m_currentOffset;
    QTimer *m_loadTimer;
    static const int COMMITS_PER_LOAD = 100;
    static const int PRELOAD_THRESHOLD = 10;

    // === 搜索相关 ===
    QString m_currentSearchText;
    QTimer *m_searchTimer;
    bool m_isSearching;
    bool m_searchLoadingMore;
    int m_searchTotalFound;
    QLabel *m_searchStatusLabel;

    // === 缓存和性能 ===
    QHash<QString, QString> m_commitDetailsCache;
    QHash<QString, QStringList> m_commitFilesCache;
    QHash<QString, QString> m_fileDiffCache;
};

/**
 * @brief Git差异语法高亮器
 *
 * 为diff输出提供语法高亮，支持：
 * - 添加行（绿色）
 * - 删除行（红色）
 * - 行号信息（蓝色）
 * - 文件路径（粗体）
 */
class GitDiffSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit GitDiffSyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> m_highlightingRules;

    QTextCharFormat m_addedLineFormat;
    QTextCharFormat m_removedLineFormat;
    QTextCharFormat m_lineNumberFormat;
    QTextCharFormat m_filePathFormat;
    QTextCharFormat m_contextFormat;
};

#endif   // GITLOGDIALOG_H
