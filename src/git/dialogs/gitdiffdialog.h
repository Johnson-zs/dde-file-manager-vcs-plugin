#ifndef GITDIFFDIALOG_H
#define GITDIFFDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QSizePolicy>

/**
 * @brief Git文件差异查看器对话框
 * 
 * 支持单文件和目录两种模式：
 * - 单文件模式：直接显示文件的diff内容
 * - 目录模式：左侧显示有变化的文件列表，右侧显示选中文件的diff内容
 */
class GitDiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onRefreshClicked();
    void onFileItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void setupSingleFileUI();
    void setupDirectoryUI();
    void loadFileDiff();
    void loadDirectoryDiff();
    void loadSingleFileDiff(const QString &filePath);
    void applySyntaxHighlighting();
    void populateFileList();
    QString getRelativePath(const QString &absolutePath) const;

    QString m_repositoryPath;
    QString m_filePath;
    bool m_isDirectory;

    // UI组件
    QSplitter *m_splitter;
    QListWidget *m_fileListWidget;
    QTextEdit *m_diffView;
    QPushButton *m_refreshButton;
    QLabel *m_fileInfoLabel;

    // 文件列表相关
    QStringList m_changedFiles;
    QString m_currentSelectedFile;
};

#endif // GITDIFFDIALOG_H 