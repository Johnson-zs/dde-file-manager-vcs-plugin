#ifndef SEARCHABLEBRANCHSELECTOR_H
#define SEARCHABLEBRANCHSELECTOR_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QIcon>

/**
 * @brief 分支/标签项目数据结构
 */
struct BranchTagItem
{
    enum Type {
        LocalBranch,
        RemoteBranch,
        Tag,
        CurrentBranch,
        AllBranches
    };

    QString name;           // 分支/标签名称
    QString displayName;    // 显示名称（可能包含图标和额外信息）
    Type type;              // 类型
    bool isCurrent;         // 是否为当前分支

    BranchTagItem() : type(LocalBranch), isCurrent(false) {}
    BranchTagItem(const QString &n, Type t, bool current = false)
        : name(n), type(t), isCurrent(current) {
        updateDisplayName();
    }

    void updateDisplayName() {
        switch (type) {
        case CurrentBranch:
            displayName = QString("● %1 (current)").arg(name);
            break;
        case LocalBranch:
            displayName = name;
            break;
        case RemoteBranch:
            displayName = name;
            break;
        case Tag:
            displayName = QString("🏷 %1").arg(name);
            break;
        case AllBranches:
            displayName = name;
            break;
        }
    }

    QIcon getIcon() const {
        switch (type) {
        case CurrentBranch:
            return QIcon::fromTheme("vcs-normal");
        case LocalBranch:
            return QIcon::fromTheme("folder");
        case RemoteBranch:
            return QIcon::fromTheme("network-workgroup");
        case Tag:
            return QIcon::fromTheme("vcs-tag");
        case AllBranches:
            return QIcon::fromTheme("view-list-tree");
        }
        return QIcon();
    }
};

Q_DECLARE_METATYPE(BranchTagItem)

/**
 * @brief 简化的可搜索分支选择器组件
 * 
 * 提供类似ComboBox但带搜索功能的分支/标签选择体验：
 * - 显示当前选择的分支名
 * - 点击下拉按钮显示所有分支和标签
 * - 支持实时搜索过滤
 * - 键盘导航支持
 */
class SearchableBranchSelector : public QWidget
{
    Q_OBJECT

public:
    explicit SearchableBranchSelector(QWidget *parent = nullptr);
    ~SearchableBranchSelector();

    // === 数据管理 ===
    void setBranches(const QStringList &localBranches, 
                    const QStringList &remoteBranches,
                    const QStringList &tags,
                    const QString &currentBranch = QString());
    
    void setCurrentSelection(const QString &branchName);
    QString getCurrentSelection() const;
    
    // === 外观控制 ===
    void setShowRemoteBranches(bool show);
    void setShowTags(bool show);
    void setPlaceholderText(const QString &text);
    
    // === 状态查询 ===
    bool getShowRemoteBranches() const { return m_showRemoteBranches; }
    bool getShowTags() const { return m_showTags; }

Q_SIGNALS:
    void selectionChanged(const QString &branchName);
    void branchActivated(const QString &branchName);
    void refreshRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    void onSearchTextChanged();
    void onItemClicked(QListWidgetItem *item);
    void onItemDoubleClicked(QListWidgetItem *item);
    void onDropdownButtonClicked();
    void onRefreshClicked();
    void hideDropdown();
    void showDropdown();

private:
    // === UI设置 ===
    void setupUI();
    void setupDropdown();
    
    // === 数据处理 ===
    void populateDropdown();
    void filterItems(const QString &searchText);
    void addCategoryItems(const QString &categoryName, 
                         const QList<BranchTagItem> &items,
                         const QString &searchText = QString());
    void addSeparator(const QString &text = QString());
    
    // === 辅助方法 ===
    bool matchesSearch(const BranchTagItem &item, const QString &searchText) const;
    void selectItem(const QString &branchName);
    void navigateList(int direction);
    void updateDisplayText();
    void syncDropdownState();  // 确保下拉框状态同步
    void selectCurrentItemInDropdown();  // 在下拉框中选中当前选择的项目
    
    // === 成员变量 ===
    // UI组件
    QLineEdit *m_displayEdit;      // 显示当前选择的只读编辑框
    QLineEdit *m_searchEdit;       // 搜索输入框（在下拉框中）
    QPushButton *m_dropdownButton;
    QPushButton *m_refreshButton;
    QFrame *m_dropdownFrame;
    QListWidget *m_listWidget;
    QLabel *m_statusLabel;
    
    // 布局
    QHBoxLayout *m_mainLayout;
    QVBoxLayout *m_dropdownLayout;
    
    // 数据
    QList<BranchTagItem> m_allItems;  // 所有分支和标签的统一列表
    QString m_currentBranch;
    QString m_selectedBranch;
    
    // 设置
    bool m_showRemoteBranches;
    bool m_showTags;
    
    // 状态
    bool m_dropdownVisible;
    QTimer *m_searchTimer;
    
    // 样式
    QString m_dropdownStyleSheet;
};

#endif // SEARCHABLEBRANCHSELECTOR_H 