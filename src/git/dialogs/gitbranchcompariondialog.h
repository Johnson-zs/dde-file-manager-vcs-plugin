#ifndef GITBRANCHCOMPARIONDIALOG_H
#define GITBRANCHCOMPARIONDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>

/**
 * @brief Git分支比较对话框
 * 
 * 提供两个分支之间的详细比较，包括：
 * - 提交差异列表
 * - 文件变更列表
 * - 详细的差异内容
 */
class GitBranchComparisonDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitBranchComparisonDialog(const QString &repositoryPath, 
                                       const QString &baseBranch, 
                                       const QString &compareBranch, 
                                       QWidget *parent = nullptr);
    ~GitBranchComparisonDialog();

private slots:
    void onCommitSelectionChanged();
    void onFileSelectionChanged();
    void onRefreshClicked();
    void onSwapBranchesClicked();

private:
    void setupUI();
    void setupCommitList();
    void setupFileList();
    void setupDiffView();
    void loadComparison();
    void loadCommitDifferences();
    void loadFileDifferences();
    void showCommitDiff(const QString &commitHash);
    void showFileDiff(const QString &filePath);

    // UI组件
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_headerLayout;
    QSplitter *m_mainSplitter;
    QSplitter *m_leftSplitter;
    
    // 头部信息
    QLabel *m_comparisonLabel;
    QPushButton *m_swapButton;
    QPushButton *m_refreshButton;
    
    // 左侧面板
    QTabWidget *m_leftTabWidget;
    QTreeWidget *m_commitList;      // 提交差异列表
    QTreeWidget *m_fileList;        // 文件变更列表
    
    // 右侧差异显示
    QTextEdit *m_diffView;
    
    // 数据
    QString m_repositoryPath;
    QString m_baseBranch;
    QString m_compareBranch;
    
    struct CommitInfo {
        QString hash;
        QString shortHash;
        QString subject;
        QString author;
        QString date;
        QString direction;  // "ahead" or "behind"
    };
    
    struct FileInfo {
        QString path;
        QString status;     // A, M, D, R, etc.
        QString oldPath;    // for renamed files
    };
    
    QVector<CommitInfo> m_commits;
    QVector<FileInfo> m_files;
};

#endif // GITBRANCHCOMPARIONDIALOG_H 