#ifndef GITDIFFDIALOG_H
#define GITDIFFDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QFileInfo>
#include <QSizePolicy>

// 前向声明
class LineNumberTextEdit;

/**
 * @brief Git文件差异查看器对话框
 * 
 * 支持多种模式：
 * - 单文件模式：直接显示文件的diff内容
 * - 目录模式：左侧显示有变化的文件列表，右侧显示选中文件的diff内容
 * - Commit模式：显示特定commit中文件的差异
 * 
 * 新增功能：
 * - 带行号显示的diff视图
 * - 语法高亮支持
 * - 支持commit文件差异查看
 */
class GitDiffDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    
    // === 新增：支持commit模式的构造函数 ===
    explicit GitDiffDialog(const QString &repositoryPath, const QString &filePath, 
                          const QString &commitHash, QWidget *parent = nullptr);

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
    void loadCommitFileDiff(const QString &filePath, const QString &commitHash);  // 新增：commit文件差异加载
    void applySyntaxHighlighting();
    void populateFileList();
    QString getRelativePath(const QString &absolutePath) const;

    QString m_repositoryPath;
    QString m_filePath;
    QString m_commitHash;      // 新增：commit哈希
    bool m_isDirectory;
    bool m_isCommitMode;       // 新增：是否为commit模式

    // UI组件
    QSplitter *m_splitter;
    QListWidget *m_fileListWidget;
    LineNumberTextEdit *m_diffView;  // 使用带行号的文本编辑器
    QPushButton *m_refreshButton;
    QLabel *m_fileInfoLabel;

    // 文件列表相关
    QStringList m_changedFiles;
    QString m_currentSelectedFile;
};

#endif // GITDIFFDIALOG_H 