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
 * @brief Git操作进度对话框
 */
class GitOperationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitOperationDialog(const QString &operation, QWidget *parent = nullptr);
    
    void executeCommand(const QString &workingDir, const QStringList &arguments);
    
private Q_SLOTS:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void updateOutput();
    
private:
    void setupUI();
    
    QString m_operation;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QTextEdit *m_outputText;
    QPushButton *m_closeButton;
    QProcess *m_process;
    QTimer *m_outputTimer;
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