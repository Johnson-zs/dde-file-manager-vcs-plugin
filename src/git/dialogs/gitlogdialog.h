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

#endif // GITLOGDIALOG_H 