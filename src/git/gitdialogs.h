#ifndef GITDIALOGS_H
#define GITDIALOGS_H

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
#include <QProgressBar>
#include <QProcess>
#include <QTimer>
#include <QListWidget>
#include <QScrollArea>

#include "gitcommandexecutor.h"

/**
 * @brief Git日志查看器对话框
 *
 * 功能丰富的Git提交历史查看界面，参考GitKraken的设计
 */
class GitLogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitLogDialog(const QString &repositoryPath, const QString &filePath = QString(), QWidget *parent = nullptr);

private Q_SLOTS:
    void onCommitSelectionChanged();
    void onRefreshClicked();
    void onBranchChanged();
    void onSearchTextChanged();
    void onLoadMoreClicked();

private:
    void setupUI();
    void loadCommitHistory(bool append = false);
    void loadBranches();
    void setupCommitList();
    void setupCommitDetails();
    void loadCommitDiff(const QString &commitHash);

    QString m_repositoryPath;
    QString m_filePath;

    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;

    // 左侧提交列表
    QTreeWidget *m_commitTree;
    QComboBox *m_branchCombo;
    QLineEdit *m_searchEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_loadMoreButton;

    // 右上：提交详情
    QTextEdit *m_commitDetails;

    // 右下：文件差异
    QTextEdit *m_diffView;

    // 分页相关
    int m_currentOffset;
    static const int COMMITS_PER_PAGE = 100;
};

/**
 * @brief Git操作进度对话框 - 重构版本
 *
 * 使用GitCommandExecutor提供统一的Git操作界面
 * 支持实时输出、取消操作、重试机制
 */
class GitOperationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitOperationDialog(const QString &operation, QWidget *parent = nullptr);
    ~GitOperationDialog();

    /**
     * @brief 执行Git命令
     * @param repoPath 仓库路径
     * @param arguments Git命令参数
     * @param timeout 超时时间（毫秒），默认30秒
     */
    void executeCommand(const QString &repoPath, const QStringList &arguments, int timeout = 30000);

    /**
     * @brief 设置操作描述
     * @param description 操作描述文本
     */
    void setOperationDescription(const QString &description);

    /**
     * @brief 获取命令执行结果
     * @return 执行结果枚举
     */
    GitCommandExecutor::Result getExecutionResult() const;

private Q_SLOTS:
    void onCommandFinished(const QString &command, GitCommandExecutor::Result result,
                           const QString &output, const QString &error);
    void onOutputReady(const QString &output, bool isError);
    void onCancelClicked();
    void onRetryClicked();
    void onDetailsToggled(bool visible);

private:
    void setupUI();
    void setupProgressSection();
    void setupOutputSection();
    void setupButtonSection();
    void updateUIState(bool isExecuting);
    void showResult(GitCommandExecutor::Result result, const QString &output, const QString &error);

    QString m_operation;
    QString m_currentDescription;
    QStringList m_lastArguments;
    QString m_lastRepoPath;
    GitCommandExecutor::Result m_executionResult;

    // UI组件
    QLabel *m_statusLabel;
    QLabel *m_descriptionLabel;
    QProgressBar *m_progressBar;
    QTextEdit *m_outputText;
    QScrollArea *m_outputScrollArea;
    QPushButton *m_cancelButton;
    QPushButton *m_retryButton;
    QPushButton *m_closeButton;
    QPushButton *m_detailsButton;
    QWidget *m_outputWidget;
    QWidget *m_buttonWidget;

    // 核心组件
    GitCommandExecutor *m_executor;
    bool m_isExecuting;
    bool m_showDetails;
};

/**
 * @brief Git Checkout对话框
 */
class GitCheckoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCheckoutDialog(const QString &repositoryPath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onCheckoutClicked();
    void onCancelClicked();
    void onBranchDoubleClicked();

private:
    void setupUI();
    void loadBranches();
    void loadTags();

    QString m_repositoryPath;
    QListWidget *m_branchList;
    QListWidget *m_tagList;
    QLineEdit *m_newBranchEdit;
    QPushButton *m_checkoutButton;
    QPushButton *m_cancelButton;
};

/**
 * @brief Git提交信息输入对话框
 */
class GitCommitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent = nullptr);

    QString getCommitMessage() const;
    QStringList getSelectedFiles() const;

private Q_SLOTS:
    void onCommitClicked();
    void onCancelClicked();
    void onMessageChanged();

private:
    void setupUI();
    void loadStagedFiles();

    QString m_repositoryPath;
    QStringList m_files;

    QTextEdit *m_messageEdit;
    QListWidget *m_fileList;
    QPushButton *m_commitButton;
    QPushButton *m_cancelButton;
};

#endif   // GITDIALOGS_H
