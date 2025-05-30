#ifndef GITCOMMITDIALOG_H
#define GITCOMMITDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>

/**
 * @brief Git提交信息输入对话框
 * 
 * 支持普通提交、amend提交和空提交等功能
 */
class GitCommitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCommitDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    explicit GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent = nullptr);

    QString getCommitMessage() const;
    QStringList getSelectedFiles() const;
    bool isAmendMode() const;
    bool isAllowEmpty() const;

private Q_SLOTS:
    void onCommitClicked();
    void onCancelClicked();
    void onMessageChanged();
    void onAmendToggled(bool enabled);
    void onAllowEmptyToggled(bool enabled);

private:
    void setupUI();
    void loadStagedFiles();
    void loadLastCommitMessage();
    bool validateCommitMessage();
    void commitChanges();
    void refreshStagedFilesDisplay();

    QString m_repositoryPath;
    QStringList m_files;
    QString m_lastCommitMessage;
    bool m_isAmendMode;
    bool m_isAllowEmpty;

    // UI组件
    QGroupBox *m_optionsGroup;
    QCheckBox *m_amendCheckBox;
    QCheckBox *m_allowEmptyCheckBox;
    QLabel *m_optionsLabel;
    
    QGroupBox *m_messageGroup;
    QTextEdit *m_messageEdit;
    QLabel *m_messageHintLabel;
    
    QGroupBox *m_filesGroup;
    QListWidget *m_fileList;
    QLabel *m_filesCountLabel;
    
    QPushButton *m_commitButton;
    QPushButton *m_cancelButton;
};

#endif // GITCOMMITDIALOG_H 