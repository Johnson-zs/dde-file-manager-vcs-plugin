#ifndef GITFILEPREVIEWDIALOG_H
#define GITFILEPREVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <memory>

// Forward declarations
class LineNumberTextEdit;
class QSyntaxHighlighter;

/**
 * @brief Git文件预览对话框
 * 
 * 提供快速预览Git仓库中文件内容的功能，支持：
 * - 当前工作区文件预览
 * - 特定commit的文件预览
 * - 语法高亮
 * - 空格键快速关闭
 * - 行号显示
 */
class GitFilePreviewDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 预览当前工作区的文件
     * @param repositoryPath Git仓库路径
     * @param filePath 相对于仓库的文件路径
     * @param parent 父窗口
     */
    explicit GitFilePreviewDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    
    /**
     * @brief 预览特定commit的文件
     * @param repositoryPath Git仓库路径
     * @param filePath 相对于仓库的文件路径
     * @param commitHash commit哈希值
     * @param parent 父窗口
     */
    explicit GitFilePreviewDialog(const QString &repositoryPath, const QString &filePath, 
                                  const QString &commitHash, QWidget *parent = nullptr);

    /**
     * @brief 静态方法：显示文件预览对话框
     * @param repositoryPath Git仓库路径
     * @param filePath 相对于仓库的文件路径
     * @param parent 父窗口
     * @return 创建的对话框指针
     */
    static GitFilePreviewDialog* showFilePreview(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);
    
    /**
     * @brief 静态方法：显示特定commit的文件预览对话框
     * @param repositoryPath Git仓库路径
     * @param filePath 相对于仓库的文件路径
     * @param commitHash commit哈希值
     * @param parent 父窗口
     * @return 创建的对话框指针
     */
    static GitFilePreviewDialog* showFilePreviewAtCommit(const QString &repositoryPath, const QString &filePath, 
                                                         const QString &commitHash, QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private Q_SLOTS:
    void onCloseClicked();
    void onOpenFileClicked();
    void onShowInFolderClicked();

private:
    void setupUI();
    void loadFileContent();
    void loadFileContentAtCommit();
    void setupSyntaxHighlighter();
    QString detectFileType() const;
    
    QString m_repositoryPath;
    QString m_filePath;
    QString m_commitHash;
    bool m_isCommitMode;
    
    // UI组件
    QLabel *m_fileInfoLabel;
    LineNumberTextEdit *m_textEdit;
    QPushButton *m_openFileButton;
    QPushButton *m_showInFolderButton;
    QPushButton *m_closeButton;
    
    // 语法高亮器
    std::unique_ptr<QSyntaxHighlighter> m_syntaxHighlighter;
};

#endif // GITFILEPREVIEWDIALOG_H 