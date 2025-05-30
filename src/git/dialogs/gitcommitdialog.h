#ifndef GITCOMMITDIALOG_H
#define GITCOMMITDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QLineEdit>
#include <QComboBox>

/**
 * @brief Git提交信息输入对话框
 *
 * 支持普通提交、amend提交和空提交等功能，支持文件选择和预览
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
    void onFileItemChanged(QTreeWidgetItem *item, int column);
    void onFileSelectionChanged();
    void onFilterChanged();
    void onRefreshFiles();
    void onStageSelected();
    void onUnstageSelected();
    void onSelectAll();
    void onSelectNone();

private:
    void setupUI();
    void loadChangedFiles();
    void loadLastCommitMessage();
    bool validateCommitMessage();
    void commitChanges();
    void refreshFilesList();
    void updateFileCountLabels(int stagedCount, int modifiedCount, int untrackedCount);
    void updateButtonStates();
    void stageFile(const QString &filePath);
    void unstageFile(const QString &filePath);
    QString getFileStatus(const QString &filePath) const;
    QIcon getStatusIcon(const QString &status) const;
    void applyFileFilter();

    QString m_repositoryPath;
    QStringList m_files;
    QString m_lastCommitMessage;
    bool m_isAmendMode;
    bool m_isAllowEmpty;

    // UI组件
    QSplitter *m_mainSplitter;

    // 选项区域
    QGroupBox *m_optionsGroup;
    QCheckBox *m_amendCheckBox;
    QCheckBox *m_allowEmptyCheckBox;
    QLabel *m_optionsLabel;

    // 消息区域
    QGroupBox *m_messageGroup;
    QTextEdit *m_messageEdit;
    QLabel *m_messageHintLabel;

    // 文件选择区域
    QGroupBox *m_filesGroup;
    QComboBox *m_fileFilterCombo;
    QLineEdit *m_fileSearchEdit;
    QTreeWidget *m_changedFilesList;
    QLabel *m_stagedCountLabel;
    QLabel *m_modifiedCountLabel;
    QLabel *m_untrackedCountLabel;

    // 操作按钮
    QPushButton *m_refreshButton;
    QPushButton *m_stageSelectedButton;
    QPushButton *m_unstageSelectedButton;
    QPushButton *m_selectAllButton;
    QPushButton *m_selectNoneButton;

    QPushButton *m_commitButton;
    QPushButton *m_cancelButton;
};

#endif   // GITCOMMITDIALOG_H
