#ifndef GITCLEANDIALOG_H
#define GITCLEANDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QTimer>

// Forward declarations
class GitOperationService;

/**
 * @brief Git Clean对话框
 * 
 * 提供安全的git clean操作界面，包括：
 * - 选择clean选项（force, directories, ignored files等）
 * - 预览将要删除的文件
 * - 确认操作前的安全检查
 * - 实时显示操作进度
 */
class GitCleanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCleanDialog(const QString &repositoryPath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onPreviewClicked();
    void onCleanClicked();
    void onRefreshClicked();
    void onOptionsChanged();
    void onOperationCompleted(const QString &operation, bool success, const QString &message);
    void onFileSelectionChanged();

private:
    void setupUI();
    void setupOptionsArea();
    void setupPreviewArea();
    void setupButtonArea();
    void loadFilePreview();
    void updateButtonStates();
    void clearPreview();
    void performCleanOperation();
    void showWarningMessage();
    
    // 辅助方法
    bool hasSelectedOptions() const;
    QString getOptionsDescription() const;
    int getFileCount() const;
    QStringList getSelectedFiles() const;

    QString m_repositoryPath;
    GitOperationService *m_operationService;
    QStringList m_previewFiles;
    bool m_isOperationInProgress;

    // UI组件
    QSplitter *m_mainSplitter;              // 主分割器
    
    // 左侧：选项区域
    QGroupBox *m_optionsGroup;
    QCheckBox *m_forceCheckBox;             // -f: 强制删除
    QCheckBox *m_directoriesCheckBox;       // -d: 删除目录
    QCheckBox *m_ignoredCheckBox;           // -x: 删除被忽略的文件
    QCheckBox *m_onlyIgnoredCheckBox;       // -X: 只删除被忽略的文件
    QLabel *m_warningLabel;                 // 警告信息
    QLabel *m_optionsDescLabel;             // 选项描述
    
    // 右侧：预览区域
    QGroupBox *m_previewGroup;
    QLabel *m_previewTitleLabel;
    QListWidget *m_fileListWidget;          // 文件列表
    QLabel *m_fileCountLabel;               // 文件数量
    QProgressBar *m_progressBar;            // 进度条
    
    // 底部：操作按钮
    QGroupBox *m_buttonGroup;
    QPushButton *m_previewButton;           // 预览按钮
    QPushButton *m_refreshButton;           // 刷新按钮
    QPushButton *m_cleanButton;             // 执行清理按钮
    QPushButton *m_cancelButton;            // 取消按钮
};

#endif // GITCLEANDIALOG_H 