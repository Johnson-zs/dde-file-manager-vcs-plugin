#ifndef GITSTASHDIALOG_H
#define GITSTASHDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QEvent>
#include <QLineEdit>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>

// Forward declarations
class GitOperationService;
class LineNumberTextEdit;

#include "../gitstashutils.h"

/**
 * @brief Git Stash管理对话框
 * 
 * 提供完整的stash管理功能，包括：
 * - 显示stash列表
 * - 预览stash内容
 * - 应用/删除stash
 * - 从stash创建分支
 * - 创建新stash
 */
class GitStashDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitStashDialog(const QString &repositoryPath, QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onRefreshClicked();
    void onStashSelectionChanged();
    void onStashDoubleClicked(QListWidgetItem *item);
    void onCreateStashClicked();
    void onApplyStashClicked();
    void onApplyKeepStashClicked();
    void onDeleteStashClicked();
    void onCreateBranchClicked();
    void onShowDiffClicked();
    void showStashContextMenu(const QPoint &pos);
    void onOperationCompleted(const QString &operation, bool success, const QString &message);

private:
    void setupUI();
    void setupStashList();
    void setupPreviewArea();
    void setupButtonArea();
    void setupContextMenu();
    void loadStashList();
    void refreshStashPreview(int stashIndex);
    void updateButtonStates();
    void clearPreview();
    
    // Stash操作方法
    void createNewStash();
    void applySelectedStash(bool keepStash = false);
    void deleteSelectedStash();
    void createBranchFromSelectedStash();
    void showSelectedStashDiff();
    
    // 辅助方法
    QString formatStashDisplayText(const GitStashInfo &info);
    int getSelectedStashIndex() const;
    GitStashInfo getSelectedStashInfo() const;
    bool hasSelectedStash() const;

    QString m_repositoryPath;
    GitOperationService *m_operationService;
    QList<GitStashInfo> m_stashList;

    // UI组件
    QSplitter *m_mainSplitter;              // 主分割器
    
    // 左侧：Stash列表
    QGroupBox *m_stashListGroup;
    QListWidget *m_stashListWidget;
    QLabel *m_stashCountLabel;
    
    // 右侧：预览区域
    QGroupBox *m_previewGroup;
    QLabel *m_previewTitleLabel;
    LineNumberTextEdit *m_previewTextEdit;  // 带行号的预览区域
    
    // 底部：操作按钮
    QGroupBox *m_buttonGroup;
    QPushButton *m_refreshButton;
    QPushButton *m_createStashButton;
    QPushButton *m_applyButton;
    QPushButton *m_applyKeepButton;
    QPushButton *m_deleteButton;
    QPushButton *m_createBranchButton;
    QPushButton *m_showDiffButton;
    QPushButton *m_closeButton;
    
    // 右键菜单
    QMenu *m_contextMenu;
    QAction *m_applyAction;
    QAction *m_applyKeepAction;
    QAction *m_deleteAction;
    QAction *m_createBranchAction;
    QAction *m_showDiffAction;
    QAction *m_refreshAction;
};

#endif // GITSTASHDIALOG_H 