#ifndef GITCOMMITDIALOG_H
#define GITCOMMITDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>

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

#endif // GITCOMMITDIALOG_H 