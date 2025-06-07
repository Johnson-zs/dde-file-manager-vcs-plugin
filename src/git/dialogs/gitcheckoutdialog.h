#ifndef GITCHECKOUTDIALOG_H
#define GITCHECKOUTDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QProgressBar>

/**
 * @brief Git分支项数据结构
 */
struct BranchItem {
    enum Type { LocalBranch, RemoteBranch, Tag };
    
    QString name;                    // 分支/标签名称
    Type type;                       // 类型标识
    bool isCurrent = false;          // 是否当前分支
    QString lastCommitHash;          // 最后提交哈希
    QString lastCommitTime;          // 最后提交时间
    QString lastCommitAuthor;        // 最后提交作者
    QString upstreamBranch;          // 上游分支（本地分支）
    bool hasChanges = false;         // 是否有未提交更改
    
    // 构造函数
    BranchItem() = default;
    BranchItem(const QString &branchName, Type branchType) 
        : name(branchName), type(branchType) {}
};

/**
 * @brief 分支删除模式
 */
enum class BranchDeleteMode {
    SafeDelete,    // git branch -d (只删除已合并分支)
    ForceDelete    // git branch -D (强制删除任何分支)
};

/**
 * @brief 分支切换模式
 */
enum class CheckoutMode {
    Normal,        // git checkout
    Force,         // git checkout -f (丢弃本地更改)
    Stash          // git stash + checkout + stash pop
};

/**
 * @brief Git Checkout对话框 - TortoiseGit风格重构版本
 * 
 * 提供统一的分支和标签管理界面，支持树形分类显示、
 * 右键菜单操作和实用的Git分支管理功能
 */
class GitCheckoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitCheckoutDialog(const QString &repositoryPath, QWidget *parent = nullptr);
    ~GitCheckoutDialog();

Q_SIGNALS:
    /**
     * @brief 仓库状态发生变化的信号
     * @param repositoryPath 仓库路径
     */
    void repositoryStateChanged(const QString &repositoryPath);

private Q_SLOTS:
    // 用户交互相关槽函数
    void onSearchTextChanged(const QString &text);
    void onRefreshClicked();
    void onNewBranchClicked();
    void onNewTagClicked();
    void onSettingsClicked();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onItemSelectionChanged();
    void showContextMenu(const QPoint &pos);
    void onCheckoutClicked();
    void onCancelClicked();
    
    // 右键菜单操作槽函数
    void checkoutSelected();
    void forceCheckoutSelected();
    void newBranchFromSelected();
    void copyBranch();
    void renameBranch();
    void setUpstreamBranch();
    void unsetUpstreamBranch();
    void checkoutRemoteBranch();
    void showBranchLog();
    void compareWithCurrent();
    void createTagFromSelected();
    void deleteSelectedBranch();
    void pushTag();
    void deleteTag();
    void deleteRemoteTag();
    
    // 工具栏功能槽函数
    void fetchRemote();
    void toggleRemoteBranches();
    void toggleTags();
    void toggleAutoFetch();
    void toggleConfirmations();

private:
    // UI初始化方法
    void setupUI();
    void setupToolbar();
    void setupTreeWidget();
    void setupNewBranchSection();
    void setupButtonSection();
    void setupContextMenus();
    void setupSettingsMenu();
    
    // 数据管理方法
    void loadBranchData();
    void populateTreeWidget();
    void filterItems(const QString &filter);
    void clearTreeWidget();
    
    // 树形控件相关方法
    QTreeWidgetItem* createCategoryItem(const QString &title, int count, const QString &icon = QString());
    QTreeWidgetItem* createBranchItem(const BranchItem &item);
    void populateCategoryItems(QTreeWidgetItem *categoryItem, const QVector<BranchItem> &items, 
                              const QString &highlightText = QString());
    
    // 用户交互辅助方法
    QString getCurrentSelectedBranch() const;
    BranchItem::Type getCurrentSelectedType() const;
    BranchItem getCurrentSelectedBranchInfo() const;
    bool isValidSelection() const;
    void updateCheckoutButtonState();
    void showLoadingState(bool loading);
    
    // 分支操作核心方法
    void performCheckout(const QString &branchName, CheckoutMode mode);
    void performCheckoutWithChangeCheck(const QString &branchName, const BranchItem &branchInfo);
    void performBranchDelete(const QString &branchName, BranchDeleteMode mode);
    void createNewBranch();
    bool hasLocalChanges();
    void executeGitCommand(const QStringList &args, const QString &operation = QString());
    bool executeGitCommandWithResult(const QStringList &args, const QString &operation = QString());
    
    // 数据解析方法
    QVector<BranchItem> parseLocalBranches(const QString &output);
    QVector<BranchItem> parseRemoteBranches(const QString &output);
    QVector<BranchItem> parseTags(const QString &output);
    QString getCurrentBranch();
    QStringList getRemoteBranches();
    
    // 错误处理方法
    void handleGitError(const QString &operation, const QString &error);
    void showOperationResult(bool success, const QString &operation, const QString &message);

    // 成员变量 - UI组件
    QVBoxLayout *m_mainLayout;
    
    // 工具栏区域
    QHBoxLayout *m_toolbarLayout;
    QLineEdit *m_searchEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_newBranchButton;
    QPushButton *m_newTagButton;
    QPushButton *m_settingsButton;

    // 树形控件区域
    QTreeWidget *m_treeWidget;
    
    // 新建分支区域
    QHBoxLayout *m_newBranchLayout;
    QLabel *m_newBranchLabel;
    QLineEdit *m_newBranchEdit;
    QCheckBox *m_switchImmediatelyCheck;

    // 按钮区域
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_checkoutButton;
    QPushButton *m_cancelButton;

    // 状态显示
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;

    // 右键菜单
    QMenu *m_branchContextMenu;
    QMenu *m_remoteBranchContextMenu;
    QMenu *m_tagContextMenu;
    QMenu *m_settingsMenu;

    // 成员变量 - 数据和状态
    QString m_repositoryPath;
    QVector<BranchItem> m_localBranches;
    QVector<BranchItem> m_remoteBranches;
    QVector<BranchItem> m_tags;
    QString m_currentBranch;
    QString m_currentFilter;
    
    // UI状态
    bool m_isLoading;
    bool m_showRemoteBranches;
    bool m_showTags;
    bool m_autoFetch;
    bool m_confirmDangerousOps;
    QTreeWidgetItem *m_lastSelectedItem;
};

#endif // GITCHECKOUTDIALOG_H 