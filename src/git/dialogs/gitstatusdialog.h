#ifndef GITSTATUSDIALOG_H
#define GITSTATUSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QIcon>
#include <QTextEdit>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QAction>

/**
 * @brief Git仓库状态查看器对话框
 * 
 * 显示Git仓库的当前状态，包括分支信息、已暂存文件、
 * 已修改文件和未跟踪文件的详细列表，支持文件操作和差异预览
 */
class GitStatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitStatusDialog(const QString &repositoryPath, QWidget *parent = nullptr);

private Q_SLOTS:
    void onRefreshClicked();
    void onFileDoubleClicked(QTreeWidgetItem *item, int column);
    void onFileSelectionChanged();
    void showFileContextMenu(const QPoint &pos);
    void stageSelectedFiles();
    void unstageSelectedFiles();
    void addSelectedFiles();
    void resetSelectedFiles();
    void commitSelectedFiles();

private:
    void setupUI();
    void loadRepositoryStatus();
    void updateStatusSummary();
    void refreshDiffPreview(const QString &filePath, const QString &status);
    void setupContextMenu();
    void updateButtonStates();
    QString getStatusDescription(const QString &status) const;
    QIcon getStatusIcon(const QString &status) const;
    QList<QTreeWidgetItem*> getSelectedFiles() const;
    bool hasSelectedFiles() const;

    QString m_repositoryPath;

    // UI组件
    QLabel *m_branchLabel;
    QLabel *m_statusSummary;
    QSplitter *m_mainSplitter;              // 主分割器
    QSplitter *m_listSplitter;              // 文件列表分割器
    QTreeWidget *m_workingTreeWidget;       // 工作区文件列表（使用TreeWidget支持多选）
    QTreeWidget *m_stagingAreaWidget;       // 暂存区文件列表
    QTextEdit *m_diffPreviewWidget;         // 差异预览区域
    
    // 操作按钮
    QPushButton *m_refreshButton;
    QPushButton *m_stageSelectedBtn;        // 暂存选中
    QPushButton *m_unstageSelectedBtn;      // 取消暂存
    QPushButton *m_addSelectedBtn;          // 添加选中
    QPushButton *m_resetSelectedBtn;        // 重置选中
    QPushButton *m_commitBtn;               // 提交按钮
    
    // 右键菜单
    QMenu *m_contextMenu;
    QAction *m_addAction;
    QAction *m_removeAction;
    QAction *m_revertAction;
    QAction *m_stageAction;
    QAction *m_unstageAction;
    QAction *m_diffAction;
};

#endif // GITSTATUSDIALOG_H 