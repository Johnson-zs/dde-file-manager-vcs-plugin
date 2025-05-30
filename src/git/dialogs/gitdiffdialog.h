#ifndef GITDIFFDIALOG_H
#define GITDIFFDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>

/**
 * @brief Git文件差异查看器对话框
 */
class GitDiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onRefreshClicked();

private:
    void setupUI();
    void loadFileDiff();
    void applySyntaxHighlighting();

    QString m_repositoryPath;
    QString m_filePath;

    QTextEdit *m_diffView;
    QPushButton *m_refreshButton;
    QLabel *m_fileInfoLabel;
};

#endif // GITDIFFDIALOG_H 