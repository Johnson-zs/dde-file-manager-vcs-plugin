#ifndef GITSTATUSDIALOG_H
#define GITSTATUSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QIcon>

/**
 * @brief Git仓库状态查看器对话框
 * 
 * 显示Git仓库的当前状态，包括分支信息、已暂存文件、
 * 已修改文件和未跟踪文件的详细列表
 */
class GitStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitStatusDialog(const QString &repositoryPath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onRefreshClicked();
    void onFileDoubleClicked(QListWidgetItem *item);

private:
    void setupUI();
    void loadRepositoryStatus();
    void updateStatusSummary();
    QString getStatusDescription(const QString &status) const;
    QIcon getStatusIcon(const QString &status) const;

    QString m_repositoryPath;

    // UI组件
    QLabel *m_branchLabel;
    QLabel *m_statusSummary;
    QListWidget *m_stagedFilesList;
    QListWidget *m_unstagedFilesList;
    QListWidget *m_untrackedFilesList;
    QPushButton *m_refreshButton;
};

#endif // GITSTATUSDIALOG_H 