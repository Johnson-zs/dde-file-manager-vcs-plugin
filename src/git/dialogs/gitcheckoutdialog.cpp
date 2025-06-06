#include "gitcheckoutdialog.h"
#include "gitoperationdialog.h"
#include "gitdialogs.h"
#include "cache.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QMenu>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include <QSplitter>
#include <QDebug>
#include <QFont>
#include <QColor>
#include <QProcess>
#include <QTimer>

GitCheckoutDialog::GitCheckoutDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_isLoading(false), m_showRemoteBranches(true), m_showTags(true), m_autoFetch(false), m_confirmDangerousOps(true), m_lastSelectedItem(nullptr)
{
    setupUI();
    setupContextMenus();

    qDebug() << "[GitCheckoutDialog] Starting branch data loading for:" << repositoryPath;

    // 开始加载分支数据
    showLoadingState(true);
    loadBranchData();
}

GitCheckoutDialog::~GitCheckoutDialog() = default;

void GitCheckoutDialog::setupUI()
{
    setWindowTitle(tr("Git Checkout"));
    setModal(true);
    resize(750, 650);   // 增大窗口以容纳更多功能

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);

    // 设置各个UI区域
    setupToolbar();
    setupTreeWidget();
    setupNewBranchSection();
    setupButtonSection();

    // 组装布局
    m_mainLayout->addLayout(m_toolbarLayout);
    m_mainLayout->addWidget(m_treeWidget, 1);   // 占主要空间
    m_mainLayout->addLayout(m_newBranchLayout);

    // 状态显示区域
    m_statusLabel = new QLabel(tr("Ready"));
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_mainLayout->addWidget(m_statusLabel);
    m_mainLayout->addWidget(m_progressBar);

    m_mainLayout->addLayout(m_buttonLayout);

    qDebug() << "[GitCheckoutDialog] UI setup completed";
}

void GitCheckoutDialog::setupToolbar()
{
    m_toolbarLayout = new QHBoxLayout;

    // 搜索框
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("🔍 Search branches and tags..."));
    m_searchEdit->setClearButtonEnabled(true);

    // 快捷操作按钮
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setToolTip(tr("Fetch remote branches and refresh list"));

    m_newBranchButton = new QPushButton(tr("New Branch"));
    m_newBranchButton->setToolTip(tr("Create a new branch from current HEAD"));

    m_newTagButton = new QPushButton(tr("New Tag"));
    m_newTagButton->setToolTip(tr("Create a new tag from current HEAD"));

    m_settingsButton = new QPushButton(tr("⚙"));
    m_settingsButton->setToolTip(tr("Branch management settings"));
    setupSettingsMenu();

    // 连接信号
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &GitCheckoutDialog::onSearchTextChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &GitCheckoutDialog::onRefreshClicked);
    connect(m_newBranchButton, &QPushButton::clicked,
            this, &GitCheckoutDialog::onNewBranchClicked);
    connect(m_newTagButton, &QPushButton::clicked,
            this, &GitCheckoutDialog::onNewTagClicked);
    connect(m_settingsButton, &QPushButton::clicked,
            this, &GitCheckoutDialog::onSettingsClicked);

    m_toolbarLayout->addWidget(m_searchEdit, 1);
    m_toolbarLayout->addWidget(m_refreshButton);
    m_toolbarLayout->addWidget(m_newBranchButton);
    m_toolbarLayout->addWidget(m_newTagButton);
    m_toolbarLayout->addWidget(m_settingsButton);
}

void GitCheckoutDialog::setupTreeWidget()
{
    m_treeWidget = new QTreeWidget(this);

    // 设置列标题
    QStringList headers;
    headers << tr("Name") << tr("Status") << tr("Last Commit");
    m_treeWidget->setHeaderLabels(headers);

    // 设置树形控件属性
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setSortingEnabled(false);
    m_treeWidget->setUniformRowHeights(true);

    // 设置列宽度
    m_treeWidget->header()->resizeSection(0, 250);   // Name列
    m_treeWidget->header()->resizeSection(1, 120);   // Status列
    m_treeWidget->header()->setStretchLastSection(true);   // Last Commit列自动拉伸

    // 连接信号
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &GitCheckoutDialog::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &GitCheckoutDialog::showContextMenu);
    connect(m_treeWidget, &QTreeWidget::itemSelectionChanged,
            this, &GitCheckoutDialog::onItemSelectionChanged);

    qDebug() << "[GitCheckoutDialog] Tree widget setup completed";
}

void GitCheckoutDialog::setupNewBranchSection()
{
    m_newBranchLayout = new QHBoxLayout;

    m_newBranchLabel = new QLabel(tr("Create new branch:"));
    m_newBranchEdit = new QLineEdit;
    m_newBranchEdit->setPlaceholderText(tr("Enter new branch name..."));

    m_switchImmediatelyCheck = new QCheckBox(tr("Switch immediately"));
    m_switchImmediatelyCheck->setToolTip(tr("Switch to the new branch after creation"));
    m_switchImmediatelyCheck->setChecked(true);

    // 连接新建分支输入框的文本变化信号
    connect(m_newBranchEdit, &QLineEdit::textChanged,
            this, &GitCheckoutDialog::updateCheckoutButtonState);

    m_newBranchLayout->addWidget(m_newBranchLabel);
    m_newBranchLayout->addWidget(m_newBranchEdit, 1);
    m_newBranchLayout->addWidget(m_switchImmediatelyCheck);
}

void GitCheckoutDialog::setupButtonSection()
{
    m_buttonLayout = new QHBoxLayout;

    // 添加一个"Close"按钮，让用户明确知道可以关闭对话框
    auto *closeButton = new QPushButton(tr("Close"));
    closeButton->setToolTip(tr("Close this dialog"));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    m_buttonLayout->addWidget(closeButton);
    m_buttonLayout->addStretch();   // 中间弹簧

    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setToolTip(tr("Cancel current operation"));
    m_checkoutButton = new QPushButton(tr("Checkout"));
    m_checkoutButton->setDefault(true);
    m_checkoutButton->setEnabled(false);   // 初始禁用，等有选择时启用

    // 连接信号
    connect(m_cancelButton, &QPushButton::clicked,
            this, &GitCheckoutDialog::onCancelClicked);
    connect(m_checkoutButton, &QPushButton::clicked,
            this, &GitCheckoutDialog::onCheckoutClicked);

    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_checkoutButton);
}

void GitCheckoutDialog::setupContextMenus()
{
    // 本地分支右键菜单
    m_branchContextMenu = new QMenu(this);
    m_branchContextMenu->addAction(tr("Checkout"), this, &GitCheckoutDialog::checkoutSelected);
    m_branchContextMenu->addAction(tr("Force Checkout"), this, &GitCheckoutDialog::forceCheckoutSelected);
    m_branchContextMenu->addSeparator();
    m_branchContextMenu->addAction(tr("New Branch From Here"), this, &GitCheckoutDialog::newBranchFromSelected);
    m_branchContextMenu->addAction(tr("Copy Branch"), this, &GitCheckoutDialog::copyBranch);
    m_branchContextMenu->addAction(tr("Rename Branch"), this, &GitCheckoutDialog::renameBranch);
    m_branchContextMenu->addSeparator();
    m_branchContextMenu->addAction(tr("Set Upstream"), this, &GitCheckoutDialog::setUpstreamBranch);
    m_branchContextMenu->addAction(tr("Unset Upstream"), this, &GitCheckoutDialog::unsetUpstreamBranch);
    m_branchContextMenu->addSeparator();
    m_branchContextMenu->addAction(tr("Show Log"), this, &GitCheckoutDialog::showBranchLog);
    m_branchContextMenu->addAction(tr("Compare with Current"), this, &GitCheckoutDialog::compareWithCurrent);
    m_branchContextMenu->addSeparator();
    m_branchContextMenu->addAction(tr("Create Tag Here"), this, &GitCheckoutDialog::createTagFromSelected);
    m_branchContextMenu->addSeparator();
    m_branchContextMenu->addAction(tr("Delete Branch"), this, &GitCheckoutDialog::deleteSelectedBranch);

    // 远程分支右键菜单
    m_remoteBranchContextMenu = new QMenu(this);
    m_remoteBranchContextMenu->addAction(tr("Checkout as New Branch"), this, &GitCheckoutDialog::checkoutRemoteBranch);
    m_remoteBranchContextMenu->addAction(tr("New Branch From Here"), this, &GitCheckoutDialog::newBranchFromSelected);
    m_remoteBranchContextMenu->addSeparator();
    m_remoteBranchContextMenu->addAction(tr("Show Log"), this, &GitCheckoutDialog::showBranchLog);
    m_remoteBranchContextMenu->addAction(tr("Compare with Current"), this, &GitCheckoutDialog::compareWithCurrent);
    m_remoteBranchContextMenu->addSeparator();
    m_remoteBranchContextMenu->addAction(tr("Create Tag Here"), this, &GitCheckoutDialog::createTagFromSelected);

    // 标签右键菜单
    m_tagContextMenu = new QMenu(this);
    m_tagContextMenu->addAction(tr("Checkout Tag"), this, &GitCheckoutDialog::checkoutSelected);
    m_tagContextMenu->addAction(tr("Checkout as New Branch"), this, &GitCheckoutDialog::newBranchFromSelected);
    m_tagContextMenu->addSeparator();
    m_tagContextMenu->addAction(tr("Show History from Tag"), this, &GitCheckoutDialog::showBranchLog);
    m_tagContextMenu->addSeparator();
    m_tagContextMenu->addAction(tr("Push Tag"), this, &GitCheckoutDialog::pushTag);
    m_tagContextMenu->addAction(tr("Delete Tag"), this, &GitCheckoutDialog::deleteTag);
    m_tagContextMenu->addAction(tr("Delete Remote Tag"), this, &GitCheckoutDialog::deleteRemoteTag);

    qDebug() << "[GitCheckoutDialog] Context menus setup completed";
}

void GitCheckoutDialog::setupSettingsMenu()
{
    m_settingsMenu = new QMenu(this);

    // 创建可勾选的菜单项
    QAction *showRemoteAction = m_settingsMenu->addAction(tr("Show Remote Branches"));
    showRemoteAction->setCheckable(true);
    showRemoteAction->setChecked(m_showRemoteBranches);
    connect(showRemoteAction, &QAction::triggered, this, &GitCheckoutDialog::toggleRemoteBranches);

    QAction *showTagsAction = m_settingsMenu->addAction(tr("Show Tags"));
    showTagsAction->setCheckable(true);
    showTagsAction->setChecked(m_showTags);
    connect(showTagsAction, &QAction::triggered, this, &GitCheckoutDialog::toggleTags);

    m_settingsMenu->addSeparator();

    QAction *autoFetchAction = m_settingsMenu->addAction(tr("Auto-fetch on Refresh"));
    autoFetchAction->setCheckable(true);
    autoFetchAction->setChecked(m_autoFetch);
    connect(autoFetchAction, &QAction::triggered, this, &GitCheckoutDialog::toggleAutoFetch);

    QAction *confirmDangerousAction = m_settingsMenu->addAction(tr("Confirm Dangerous Operations"));
    confirmDangerousAction->setCheckable(true);
    confirmDangerousAction->setChecked(m_confirmDangerousOps);
    connect(confirmDangerousAction, &QAction::triggered, this, &GitCheckoutDialog::toggleConfirmations);

    m_settingsButton->setMenu(m_settingsMenu);
}

// 数据加载方法
void GitCheckoutDialog::loadBranchData()
{
    qDebug() << "[GitCheckoutDialog] Loading branch data";

    m_currentBranch = getCurrentBranch();
    qDebug() << "[GitCheckoutDialog] Current branch/tag:" << m_currentBranch;

    // 解析本地分支
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "branch", "-v" });

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_localBranches = parseLocalBranches(output);
        qDebug() << "[GitCheckoutDialog] Loaded" << m_localBranches.size() << "local branches";
    }

    // 解析远程分支
    process.start("git", { "branch", "-rv" });
    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_remoteBranches = parseRemoteBranches(output);
        qDebug() << "[GitCheckoutDialog] Loaded" << m_remoteBranches.size() << "remote branches";
    }

    // 解析标签
    process.start("git", { "tag", "-l" });
    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_tags = parseTags(output);

        // 检查当前是否在某个tag上
        for (auto &tag : m_tags) {
            if (tag.name == m_currentBranch) {
                tag.isCurrent = true;
                qDebug() << "[GitCheckoutDialog] Current tag found:" << tag.name;
                break;
            }
        }

        qDebug() << "[GitCheckoutDialog] Loaded" << m_tags.size() << "tags";
    }

    // 填充树形控件
    populateTreeWidget();

    // 更新UI状态
    showLoadingState(false);
    m_statusLabel->setText(tr("Loaded %1 branches, %2 tags. Current: %3")
                                   .arg(m_localBranches.size() + m_remoteBranches.size())
                                   .arg(m_tags.size())
                                   .arg(m_currentBranch));
}

void GitCheckoutDialog::populateTreeWidget()
{
    if (!m_currentFilter.isEmpty()) {
        filterItems(m_currentFilter);
        return;
    }

    qDebug() << "[GitCheckoutDialog] Populating tree widget";

    clearTreeWidget();

    // 创建分类节点
    QTreeWidgetItem *localItem = nullptr;
    QTreeWidgetItem *remoteItem = nullptr;
    QTreeWidgetItem *tagItem = nullptr;

    // 本地分支
    if (!m_localBranches.isEmpty()) {
        localItem = createCategoryItem(tr("📁 Local Branches"), m_localBranches.size());
        m_treeWidget->addTopLevelItem(localItem);
        populateCategoryItems(localItem, m_localBranches);
        localItem->setExpanded(true);
    }

    // 远程分支
    if (!m_remoteBranches.isEmpty() && m_showRemoteBranches) {
        remoteItem = createCategoryItem(tr("📁 Remote Branches"), m_remoteBranches.size());
        m_treeWidget->addTopLevelItem(remoteItem);
        populateCategoryItems(remoteItem, m_remoteBranches);
        remoteItem->setExpanded(false);   // 默认折叠远程分支
    }

    // 标签
    if (!m_tags.isEmpty() && m_showTags) {
        tagItem = createCategoryItem(tr("📁 Tags"), m_tags.size());
        m_treeWidget->addTopLevelItem(tagItem);
        populateCategoryItems(tagItem, m_tags);
        tagItem->setExpanded(false);   // 默认折叠标签
    }

    qDebug() << "[GitCheckoutDialog] Tree widget populated successfully";
}

void GitCheckoutDialog::clearTreeWidget()
{
    m_treeWidget->clear();
    m_lastSelectedItem = nullptr;
}

void GitCheckoutDialog::onCheckoutClicked()
{
    QString target;
    bool shouldCreateNewBranch = false;

    if (!m_newBranchEdit->text().isEmpty()) {
        target = m_newBranchEdit->text();
        shouldCreateNewBranch = true;
    } else if (m_treeWidget->currentItem()) {
        BranchItem branchInfo = getCurrentSelectedBranchInfo();
        if (!branchInfo.name.isEmpty()) {
            target = branchInfo.name;
        }
    }

    if (target.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please select a branch/tag or enter a new branch name."));
        return;
    }

    if (shouldCreateNewBranch) {
        createNewBranch();
    } else {
        // 检查是否为当前分支
        BranchItem branchInfo = getCurrentSelectedBranchInfo();
        if (branchInfo.isCurrent && branchInfo.type == BranchItem::LocalBranch) {
            QMessageBox::information(this, tr("Current Branch"),
                                     tr("'%1' is already the current branch.").arg(target));
            return;
        }

        // 根据类型执行不同的切换逻辑
        if (branchInfo.type == BranchItem::LocalBranch) {
            performCheckout(target, CheckoutMode::Normal);
        } else if (branchInfo.type == BranchItem::RemoteBranch) {
            checkoutRemoteBranch();
        } else if (branchInfo.type == BranchItem::Tag) {
            performCheckout(target, CheckoutMode::Normal);
        }
    }
}

void GitCheckoutDialog::onCancelClicked()
{
    // Cancel按钮现在用于取消当前操作，而不是关闭对话框
    // 清空选择和输入
    m_treeWidget->clearSelection();
    m_newBranchEdit->clear();
    m_statusLabel->setText(tr("Ready"));
    updateCheckoutButtonState();

    qDebug() << "[GitCheckoutDialog] Operation cancelled by user";
}

void GitCheckoutDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    if (!item || !item->parent()) {
        // 如果点击的是分类节点，展开/折叠
        if (item) {
            item->setExpanded(!item->isExpanded());
        }
        return;
    }

    // 获取分支信息
    BranchItem branchInfo = item->data(0, Qt::UserRole).value<BranchItem>();
    if (branchInfo.name.isEmpty()) {
        qWarning() << "[GitCheckoutDialog] Invalid branch item data";
        return;
    }

    // 检查是否为当前分支
    if (branchInfo.isCurrent && branchInfo.type == BranchItem::LocalBranch) {
        QMessageBox::information(this, tr("Current Branch"),
                                 tr("'%1' is already the current branch.").arg(branchInfo.name));
        return;
    }

    // 根据类型执行不同的切换逻辑
    if (branchInfo.type == BranchItem::LocalBranch) {
        // 本地分支 - 使用与右键菜单相同的逻辑
        performCheckoutWithChangeCheck(branchInfo.name, branchInfo);
    } else if (branchInfo.type == BranchItem::RemoteBranch) {
        // 远程分支需要创建本地分支
        checkoutRemoteBranch();
    } else if (branchInfo.type == BranchItem::Tag) {
        // 标签切换到detached HEAD状态 - 也检查本地更改
        performCheckoutWithChangeCheck(branchInfo.name, branchInfo);
    }
}

void GitCheckoutDialog::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    if (!item) {
        qDebug() << "[GitCheckoutDialog] No item at position:" << pos;
        return;
    }

    // 确保选中右键点击的项目
    m_treeWidget->setCurrentItem(item);

    QMenu *menu = nullptr;

    if (item->parent()) {
        // 这是一个分支/标签项目
        QString categoryText = item->parent()->text(0);
        qDebug() << "[GitCheckoutDialog] Context menu for category:" << categoryText;

        if (categoryText.contains(tr("Local Branches"))) {
            menu = m_branchContextMenu;
        } else if (categoryText.contains(tr("Remote Branches"))) {
            menu = m_remoteBranchContextMenu;
        } else if (categoryText.contains(tr("Tags"))) {
            menu = m_tagContextMenu;
        }
    } else {
        // 这是一个分类节点，不显示菜单
        qDebug() << "[GitCheckoutDialog] Category node clicked, no context menu";
        return;
    }

    if (menu) {
        qDebug() << "[GitCheckoutDialog] Showing context menu with" << menu->actions().size() << "actions";
        menu->exec(m_treeWidget->mapToGlobal(pos));
    } else {
        qWarning() << "[GitCheckoutDialog] No appropriate context menu found";
    }
}

void GitCheckoutDialog::onItemSelectionChanged()
{
    QTreeWidgetItem *item = m_treeWidget->currentItem();

    // 更新checkout按钮状态
    updateCheckoutButtonState();

    if (item && item->parent()) {
        // 这是一个分支/标签项目
        BranchItem branchInfo = item->data(0, Qt::UserRole).value<BranchItem>();
        if (!branchInfo.name.isEmpty()) {
            // 在新建分支输入框中显示选中的分支名
            m_newBranchEdit->setText(branchInfo.name);

            // 更新状态标签
            QString statusText;
            if (branchInfo.isCurrent) {
                if (branchInfo.type == BranchItem::Tag) {
                    statusText = tr("Selected: %1 (Current Tag)").arg(branchInfo.name);
                } else {
                    statusText = tr("Selected: %1 (Current Branch)").arg(branchInfo.name);
                }
            } else {
                switch (branchInfo.type) {
                case BranchItem::LocalBranch:
                    statusText = tr("Selected: %1 (Local Branch)").arg(branchInfo.name);
                    break;
                case BranchItem::RemoteBranch:
                    statusText = tr("Selected: %1 (Remote Branch)").arg(branchInfo.name);
                    break;
                case BranchItem::Tag:
                    statusText = tr("Selected: %1 (Tag)").arg(branchInfo.name);
                    break;
                }
            }

            m_statusLabel->setText(statusText);
        }
    } else {
        // 选中了分类节点或没有选中
        m_newBranchEdit->clear();
        m_statusLabel->setText(tr("Ready"));
    }
}

void GitCheckoutDialog::onSearchTextChanged(const QString &text)
{
    m_currentFilter = text;
    populateTreeWidget();   // 重新填充并应用过滤
    qDebug() << "[GitCheckoutDialog] Search filter applied:" << text;
}

void GitCheckoutDialog::onRefreshClicked()
{
    qDebug() << "[GitCheckoutDialog] Refreshing branch data";

    if (m_autoFetch) {
        // 如果启用了自动获取，先执行fetch
        fetchRemote();
    }

    showLoadingState(true);
    loadBranchData();
}

void GitCheckoutDialog::onNewBranchClicked()
{
    bool ok;
    QString branchName = QInputDialog::getText(this, tr("Create New Branch"),
                                               tr("Enter new branch name:"),
                                               QLineEdit::Normal, QString(), &ok);

    if (ok && !branchName.trimmed().isEmpty()) {
        m_newBranchEdit->setText(branchName.trimmed());

        // 如果设置了立即切换，直接创建并切换
        if (m_switchImmediatelyCheck->isChecked()) {
            createNewBranch();
        }
    }
}

void GitCheckoutDialog::onNewTagClicked()
{
    bool ok;
    QString tagName = QInputDialog::getText(this, tr("Create New Tag"),
                                            tr("Enter new tag name:"),
                                            QLineEdit::Normal, QString(), &ok);

    if (ok && !tagName.trimmed().isEmpty()) {
        executeGitCommand({ "tag", tagName.trimmed() }, tr("Create Tag"));

        // 刷新标签列表
        onRefreshClicked();
    }
}

void GitCheckoutDialog::onSettingsClicked()
{
    // Settings按钮点击会显示下拉菜单（已在setupSettingsMenu中配置）
}

void GitCheckoutDialog::checkoutSelected()
{
    QString branchName = getCurrentSelectedBranch();
    if (branchName.isEmpty()) return;

    BranchItem branchInfo = getCurrentSelectedBranchInfo();

    // 使用统一的checkout逻辑
    performCheckoutWithChangeCheck(branchName, branchInfo);
}

void GitCheckoutDialog::forceCheckoutSelected()
{
    QString branchName = getCurrentSelectedBranch();
    if (branchName.isEmpty()) return;

    if (m_confirmDangerousOps) {
        QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("Force Checkout"),
                                                               tr("Force checkout will discard all local changes!\n\n"
                                                                  "This action cannot be undone. Are you sure you want to continue?"),
                                                               QMessageBox::Yes | QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    performCheckout(branchName, CheckoutMode::Force);
}

void GitCheckoutDialog::newBranchFromSelected()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    bool ok;
    QString branchName = QInputDialog::getText(this, tr("Create New Branch"),
                                               tr("Enter name for new branch from '%1':").arg(selectedBranch),
                                               QLineEdit::Normal, QString(), &ok);

    if (ok && !branchName.trimmed().isEmpty()) {
        QStringList args = { "checkout", "-b", branchName.trimmed(), selectedBranch };
        bool success = executeGitCommandWithResult(args, tr("Create new branch from %1").arg(selectedBranch));

        // 不再自动关闭对话框
        if (success) {
            m_statusLabel->setText(tr("✓ Successfully created branch %1 from %2").arg(branchName.trimmed(), selectedBranch));
        }
    }
}

void GitCheckoutDialog::copyBranch()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    bool ok;
    QString branchName = QInputDialog::getText(this, tr("Copy Branch"),
                                               tr("Enter name for branch copy of '%1':").arg(selectedBranch),
                                               QLineEdit::Normal, selectedBranch + "_copy", &ok);

    if (ok && !branchName.trimmed().isEmpty()) {
        QStringList args = { "branch", branchName.trimmed(), selectedBranch };
        executeGitCommand(args, tr("Copy branch %1").arg(selectedBranch));
    }
}

void GitCheckoutDialog::renameBranch()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    BranchItem branchInfo = getCurrentSelectedBranchInfo();
    if (branchInfo.isCurrent) {
        QMessageBox::information(this, tr("Current Branch"),
                                 tr("Cannot rename the current branch. Please switch to another branch first."));
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Branch"),
                                            tr("Enter new name for branch '%1':").arg(selectedBranch),
                                            QLineEdit::Normal, selectedBranch, &ok);

    if (ok && !newName.trimmed().isEmpty() && newName.trimmed() != selectedBranch) {
        QStringList args = { "branch", "-m", selectedBranch, newName.trimmed() };
        executeGitCommand(args, tr("Rename branch"));
    }
}

void GitCheckoutDialog::setUpstreamBranch()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    QStringList remoteBranches = getRemoteBranches();
    if (remoteBranches.isEmpty()) {
        QMessageBox::information(this, tr("No Remote Branches"),
                                 tr("No remote branches found. Please add a remote repository first."));
        return;
    }

    bool ok;
    QString upstream = QInputDialog::getItem(this, tr("Set Upstream Branch"),
                                             tr("Select upstream branch for '%1':").arg(selectedBranch),
                                             remoteBranches, 0, false, &ok);

    if (ok && !upstream.isEmpty()) {
        QStringList args = { "branch", "--set-upstream-to=" + upstream, selectedBranch };
        executeGitCommand(args, tr("Set upstream branch"));
    }
}

void GitCheckoutDialog::unsetUpstreamBranch()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    QStringList args = { "branch", "--unset-upstream", selectedBranch };
    executeGitCommand(args, tr("Unset upstream branch"));
}

void GitCheckoutDialog::checkoutRemoteBranch()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    // 从远程分支名称中提取本地分支名称
    QString localBranchName = selectedBranch;
    if (localBranchName.startsWith("origin/")) {
        localBranchName = localBranchName.mid(7);   // 移除 "origin/" 前缀
    } else {
        // 处理其他远程名称
        int slashIndex = localBranchName.indexOf('/');
        if (slashIndex > 0) {
            localBranchName = localBranchName.mid(slashIndex + 1);
        }
    }

    bool ok;
    QString branchName = QInputDialog::getText(this, tr("Checkout Remote Branch"),
                                               tr("Enter local branch name for remote '%1':").arg(selectedBranch),
                                               QLineEdit::Normal, localBranchName, &ok);

    if (ok && !branchName.trimmed().isEmpty()) {
        QStringList args = { "checkout", "-b", branchName.trimmed(), selectedBranch };
        bool success = executeGitCommandWithResult(args, tr("Checkout remote branch"));

        // 不再自动关闭对话框
        if (success) {
            m_statusLabel->setText(tr("✓ Successfully checked out remote branch %1 as %2").arg(selectedBranch, branchName.trimmed()));
        }
    }
}

void GitCheckoutDialog::showBranchLog()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    qDebug() << "[GitCheckoutDialog] Show log for branch:" << selectedBranch;

    // 使用 GitDialogManager 显示分支日志，传递选中的分支作为初始分支
    GitDialogManager::instance()->showLogDialog(m_repositoryPath, QString(), selectedBranch, this);
}

void GitCheckoutDialog::compareWithCurrent()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty() || selectedBranch == m_currentBranch) {
        QMessageBox::information(this, tr("Compare Branches"),
                                 tr("Cannot compare branch with itself."));
        return;
    }

    qDebug() << "[GitCheckoutDialog] Compare" << selectedBranch << "with" << m_currentBranch;

    // 使用新的分支比较对话框
    GitDialogManager::instance()->showBranchComparisonDialog(
            m_repositoryPath, m_currentBranch, selectedBranch, this);
}

void GitCheckoutDialog::createTagFromSelected()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    bool ok;
    QString tagName = QInputDialog::getText(this, tr("Create Tag"),
                                            tr("Enter tag name for '%1':").arg(selectedBranch),
                                            QLineEdit::Normal, QString(), &ok);

    if (ok && !tagName.trimmed().isEmpty()) {
        QStringList args = { "tag", tagName.trimmed(), selectedBranch };
        executeGitCommand(args, tr("Create tag"));
    }
}

void GitCheckoutDialog::deleteSelectedBranch()
{
    QString selectedBranch = getCurrentSelectedBranch();
    if (selectedBranch.isEmpty()) return;

    BranchItem branchInfo = getCurrentSelectedBranchInfo();
    if (branchInfo.isCurrent) {
        QMessageBox::warning(this, tr("Cannot Delete"),
                             tr("Cannot delete the current branch. Please switch to another branch first."));
        return;
    }

    // 询问删除模式
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Delete Branch"));
    msgBox.setText(tr("Choose delete mode for branch '%1':").arg(selectedBranch));
    msgBox.setInformativeText(tr("Safe Delete: Only delete if branch is fully merged\n"
                                 "Force Delete: Delete branch regardless of merge status"));

    QPushButton *safeButton = msgBox.addButton(tr("Safe Delete"), QMessageBox::AcceptRole);
    QPushButton *forceButton = msgBox.addButton(tr("Force Delete"), QMessageBox::DestructiveRole);
    msgBox.addButton(QMessageBox::Cancel);

    msgBox.exec();

    if (msgBox.clickedButton() == safeButton) {
        performBranchDelete(selectedBranch, BranchDeleteMode::SafeDelete);
    } else if (msgBox.clickedButton() == forceButton) {
        if (m_confirmDangerousOps) {
            QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("Force Delete Branch"),
                                                                   tr("Force delete will permanently remove branch '%1' even if it's not merged!\n\n"
                                                                      "This action cannot be undone. Are you sure?")
                                                                           .arg(selectedBranch),
                                                                   QMessageBox::Yes | QMessageBox::No);

            if (ret == QMessageBox::Yes) {
                performBranchDelete(selectedBranch, BranchDeleteMode::ForceDelete);
            }
        } else {
            performBranchDelete(selectedBranch, BranchDeleteMode::ForceDelete);
        }
    }
}

void GitCheckoutDialog::pushTag()
{
    QString selectedTag = getCurrentSelectedBranch();   // 对于标签，也使用这个方法获取名称
    if (selectedTag.isEmpty()) return;

    QStringList args = { "push", "origin", selectedTag };
    executeGitCommand(args, tr("Push tag"));
}

void GitCheckoutDialog::deleteTag()
{
    QString selectedTag = getCurrentSelectedBranch();
    if (selectedTag.isEmpty()) return;

    if (m_confirmDangerousOps) {
        QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Delete Tag"),
                                                                tr("Are you sure you want to delete tag '%1'?").arg(selectedTag),
                                                                QMessageBox::Yes | QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    QStringList args = { "tag", "-d", selectedTag };
    executeGitCommand(args, tr("Delete tag"));
}

void GitCheckoutDialog::deleteRemoteTag()
{
    QString selectedTag = getCurrentSelectedBranch();
    if (selectedTag.isEmpty()) return;

    if (m_confirmDangerousOps) {
        QMessageBox::StandardButton ret = QMessageBox::warning(this, tr("Delete Remote Tag"),
                                                               tr("Are you sure you want to delete remote tag '%1'?\n\n"
                                                                  "This will remove the tag from the remote repository.")
                                                                       .arg(selectedTag),
                                                               QMessageBox::Yes | QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    QStringList args = { "push", "origin", ":refs/tags/" + selectedTag };
    executeGitCommand(args, tr("Delete remote tag"));
}

// 工具栏功能实现
void GitCheckoutDialog::fetchRemote()
{
    qDebug() << "[GitCheckoutDialog] Fetching remote branches";
    executeGitCommand({ "fetch", "--all", "--prune" }, tr("Fetch remote branches"));
}

void GitCheckoutDialog::toggleRemoteBranches()
{
    m_showRemoteBranches = !m_showRemoteBranches;
    qDebug() << "[GitCheckoutDialog] Show remote branches:" << m_showRemoteBranches;

    // 更新菜单项状态
    for (QAction *action : m_settingsMenu->actions()) {
        if (action->text() == tr("Show Remote Branches")) {
            action->setChecked(m_showRemoteBranches);
            break;
        }
    }

    populateTreeWidget();
}

void GitCheckoutDialog::toggleTags()
{
    m_showTags = !m_showTags;
    qDebug() << "[GitCheckoutDialog] Show tags:" << m_showTags;

    // 更新菜单项状态
    for (QAction *action : m_settingsMenu->actions()) {
        if (action->text() == tr("Show Tags")) {
            action->setChecked(m_showTags);
            break;
        }
    }

    populateTreeWidget();
}

void GitCheckoutDialog::toggleAutoFetch()
{
    m_autoFetch = !m_autoFetch;
    qDebug() << "[GitCheckoutDialog] Auto-fetch on refresh:" << m_autoFetch;

    // 更新菜单项状态
    for (QAction *action : m_settingsMenu->actions()) {
        if (action->text() == tr("Auto-fetch on Refresh")) {
            action->setChecked(m_autoFetch);
            break;
        }
    }
}

void GitCheckoutDialog::toggleConfirmations()
{
    m_confirmDangerousOps = !m_confirmDangerousOps;
    qDebug() << "[GitCheckoutDialog] Confirm dangerous operations:" << m_confirmDangerousOps;

    // 更新菜单项状态
    for (QAction *action : m_settingsMenu->actions()) {
        if (action->text() == tr("Confirm Dangerous Operations")) {
            action->setChecked(m_confirmDangerousOps);
            break;
        }
    }
}

// 分支操作核心方法
void GitCheckoutDialog::performBranchDelete(const QString &branchName, BranchDeleteMode mode)
{
    qDebug() << "[GitCheckoutDialog] Deleting branch:" << branchName << "mode:" << static_cast<int>(mode);

    QStringList args;
    QString operation;

    switch (mode) {
    case BranchDeleteMode::SafeDelete:
        args = { "branch", "-d", branchName };
        operation = tr("Safe delete branch");
        break;

    case BranchDeleteMode::ForceDelete:
        args = { "branch", "-D", branchName };
        operation = tr("Force delete branch");
        break;
    }

    executeGitCommand(args, operation);
}

QStringList GitCheckoutDialog::getRemoteBranches()
{
    QStringList remoteBranches;

    for (const auto &branch : m_remoteBranches) {
        remoteBranches.append(branch.name);
    }

    return remoteBranches;
}

// 完善搜索过滤功能
void GitCheckoutDialog::filterItems(const QString &filter)
{
    if (filter.isEmpty()) {
        // 如果没有过滤条件，显示所有项目
        populateTreeWidget();
        return;
    }

    clearTreeWidget();

    // 过滤本地分支
    QVector<BranchItem> filteredLocal;
    for (const auto &branch : m_localBranches) {
        if (branch.name.contains(filter, Qt::CaseInsensitive)) {
            filteredLocal.append(branch);
        }
    }

    // 过滤远程分支
    QVector<BranchItem> filteredRemote;
    if (m_showRemoteBranches) {
        for (const auto &branch : m_remoteBranches) {
            if (branch.name.contains(filter, Qt::CaseInsensitive)) {
                filteredRemote.append(branch);
            }
        }
    }

    // 过滤标签
    QVector<BranchItem> filteredTags;
    if (m_showTags) {
        for (const auto &tag : m_tags) {
            if (tag.name.contains(filter, Qt::CaseInsensitive)) {
                filteredTags.append(tag);
            }
        }
    }

    // 创建过滤后的分类节点
    if (!filteredLocal.isEmpty()) {
        QTreeWidgetItem *localItem = createCategoryItem(tr("📁 Local Branches"), filteredLocal.size());
        m_treeWidget->addTopLevelItem(localItem);
        populateCategoryItems(localItem, filteredLocal, filter);
        localItem->setExpanded(true);
    }

    if (!filteredRemote.isEmpty()) {
        QTreeWidgetItem *remoteItem = createCategoryItem(tr("📁 Remote Branches"), filteredRemote.size());
        m_treeWidget->addTopLevelItem(remoteItem);
        populateCategoryItems(remoteItem, filteredRemote, filter);
        remoteItem->setExpanded(true);
    }

    if (!filteredTags.isEmpty()) {
        QTreeWidgetItem *tagItem = createCategoryItem(tr("📁 Tags"), filteredTags.size());
        m_treeWidget->addTopLevelItem(tagItem);
        populateCategoryItems(tagItem, filteredTags, filter);
        tagItem->setExpanded(true);
    }

    // 更新状态信息
    int totalFiltered = filteredLocal.size() + filteredRemote.size() + filteredTags.size();
    m_statusLabel->setText(tr("Found %1 items matching '%2'").arg(totalFiltered).arg(filter));
}

// 为BranchItem注册Qt元类型系统
Q_DECLARE_METATYPE(BranchItem)

QString GitCheckoutDialog::getCurrentBranch()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "rev-parse", "--abbrev-ref", "HEAD" });

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        qDebug() << "[GitCheckoutDialog] Current branch:" << output;

        // 如果是detached HEAD状态，尝试获取当前的tag或commit
        if (output == "HEAD") {
            // 检查是否在某个tag上
            QProcess tagProcess;
            tagProcess.setWorkingDirectory(m_repositoryPath);
            tagProcess.start("git", { "describe", "--exact-match", "--tags", "HEAD" });

            if (tagProcess.waitForFinished(3000)) {
                QString tagOutput = QString::fromUtf8(tagProcess.readAllStandardOutput()).trimmed();
                if (!tagOutput.isEmpty()) {
                    qDebug() << "[GitCheckoutDialog] Current tag:" << tagOutput;
                    return tagOutput;   // 返回tag名称
                }
            }

            // 如果不在tag上，返回短commit hash
            QProcess hashProcess;
            hashProcess.setWorkingDirectory(m_repositoryPath);
            hashProcess.start("git", { "rev-parse", "--short", "HEAD" });

            if (hashProcess.waitForFinished(3000)) {
                QString hashOutput = QString::fromUtf8(hashProcess.readAllStandardOutput()).trimmed();
                qDebug() << "[GitCheckoutDialog] Current commit:" << hashOutput;
                return QString("detached@%1").arg(hashOutput);
            }
        }

        return output;
    }

    qWarning() << "[GitCheckoutDialog] Failed to get current branch";
    return QString();
}

// 用户交互辅助方法
QString GitCheckoutDialog::getCurrentSelectedBranch() const
{
    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (!item || !item->parent()) {
        return QString();   // 没有选择或选择的是分类节点
    }

    BranchItem branchInfo = item->data(0, Qt::UserRole).value<BranchItem>();
    return branchInfo.name;
}

BranchItem::Type GitCheckoutDialog::getCurrentSelectedType() const
{
    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (!item || !item->parent()) {
        return BranchItem::LocalBranch;   // 默认类型
    }

    BranchItem branchInfo = item->data(0, Qt::UserRole).value<BranchItem>();
    return branchInfo.type;
}

BranchItem GitCheckoutDialog::getCurrentSelectedBranchInfo() const
{
    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (!item || !item->parent()) {
        return BranchItem();   // 空对象
    }

    return item->data(0, Qt::UserRole).value<BranchItem>();
}

bool GitCheckoutDialog::isValidSelection() const
{
    return !getCurrentSelectedBranch().isEmpty();
}

void GitCheckoutDialog::updateCheckoutButtonState()
{
    bool hasValidSelection = false;
    bool hasNewBranchName = !m_newBranchEdit->text().trimmed().isEmpty();

    QTreeWidgetItem *item = m_treeWidget->currentItem();
    if (item && item->parent()) {
        // 选中了分支/标签项目
        BranchItem branchInfo = item->data(0, Qt::UserRole).value<BranchItem>();
        if (!branchInfo.name.isEmpty()) {
            hasValidSelection = true;

            // 如果选中的是当前分支，禁用checkout按钮
            if (branchInfo.isCurrent && branchInfo.type == BranchItem::LocalBranch) {
                hasValidSelection = false;
            }
        }
    }

    // 启用checkout按钮的条件：有有效选择或有新分支名称
    m_checkoutButton->setEnabled(hasValidSelection || hasNewBranchName);

    // 更新按钮文本
    if (hasNewBranchName && !hasValidSelection) {
        m_checkoutButton->setText(tr("Create Branch"));
    } else if (hasValidSelection) {
        QTreeWidgetItem *currentItem = m_treeWidget->currentItem();
        if (currentItem) {
            BranchItem branchInfo = currentItem->data(0, Qt::UserRole).value<BranchItem>();
            if (branchInfo.type == BranchItem::Tag) {
                m_checkoutButton->setText(tr("Checkout Tag"));
            } else if (branchInfo.type == BranchItem::RemoteBranch) {
                m_checkoutButton->setText(tr("Checkout Remote"));
            } else {
                m_checkoutButton->setText(tr("Checkout"));
            }
        }
    } else {
        m_checkoutButton->setText(tr("Checkout"));
    }
}

void GitCheckoutDialog::showLoadingState(bool loading)
{
    m_isLoading = loading;
    m_progressBar->setVisible(loading);
    m_statusLabel->setText(loading ? tr("Loading...") : tr("Ready"));

    // 禁用/启用相关控件
    m_refreshButton->setEnabled(!loading);
    m_newBranchButton->setEnabled(!loading);
    m_newTagButton->setEnabled(!loading);
    m_treeWidget->setEnabled(!loading);
}

// 分支操作核心方法实现
void GitCheckoutDialog::performCheckout(const QString &branchName, CheckoutMode mode)
{
    qDebug() << "[GitCheckoutDialog] Performing checkout:" << branchName << "mode:" << static_cast<int>(mode);

    QStringList args;
    QString operation;

    switch (mode) {
    case CheckoutMode::Normal:
        args = { "checkout", branchName };
        operation = tr("Checkout branch");
        break;

    case CheckoutMode::Force:
        args = { "checkout", "-f", branchName };
        operation = tr("Force checkout branch");
        break;

    case CheckoutMode::Stash:
        // 先暂存，再切换，最后恢复
        if (executeGitCommandWithResult({ "stash", "push", "-m", tr("Auto-stash for checkout") }, tr("Stash changes"))) {
            args = { "checkout", branchName };
            operation = tr("Checkout branch (with stash)");
        } else {
            return;   // 暂存失败，不继续操作
        }
        break;
    }

    bool success = executeGitCommandWithResult(args, operation);

    // 如果是stash模式且切换成功，尝试恢复暂存
    if (success && mode == CheckoutMode::Stash) {
        executeGitCommandWithResult({ "stash", "pop" }, tr("Restore stashed changes"));
    }

    // 不再自动关闭对话框，让用户可以继续操作
    if (success) {
        // 清空新建分支输入框
        m_newBranchEdit->clear();
        // 更新状态显示
        m_statusLabel->setText(tr("✓ Successfully switched to %1").arg(branchName));
    }
}

void GitCheckoutDialog::createNewBranch()
{
    QString branchName = m_newBranchEdit->text().trimmed();
    if (branchName.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please enter a branch name."));
        return;
    }

    // 验证分支名称的有效性
    if (branchName.contains(' ') || branchName.contains('\t')) {
        QMessageBox::warning(this, tr("Warning"), tr("Branch name cannot contain spaces."));
        return;
    }

    QStringList args = { "checkout", "-b", branchName };
    bool success = executeGitCommandWithResult(args, tr("Create new branch"));

    // 不再自动关闭对话框
    if (success) {
        // 清空输入框
        m_newBranchEdit->clear();
        // 更新状态显示
        m_statusLabel->setText(tr("✓ Successfully created and switched to branch %1").arg(branchName));
    }
}

bool GitCheckoutDialog::hasLocalChanges()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "status", "--porcelain" });

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        qDebug() << "[GitCheckoutDialog] Git status output lines:" << lines.size();

        // 检查是否有会阻止 checkout 的更改
        bool hasBlockingChanges = false;

        for (const QString &line : lines) {
            if (line.length() < 2) continue;

            QChar indexStatus = line.at(0);   // 暂存区状态
            QChar workTreeStatus = line.at(1);   // 工作区状态

            qDebug() << "[GitCheckoutDialog] File status:" << line.left(2) << "File:" << line.mid(3);

            // 检查会阻止 checkout 的状态：
            // 1. 暂存区有更改 (A, M, D, R, C)
            // 2. 工作区有修改 (M) 或删除 (D)
            // 注意：未跟踪文件 (??) 不会阻止 checkout

            if (indexStatus != ' ' && indexStatus != '?') {
                // 暂存区有更改
                hasBlockingChanges = true;
                qDebug() << "[GitCheckoutDialog] Found staged changes:" << line;
                break;
            }

            if (workTreeStatus == 'M' || workTreeStatus == 'D') {
                // 工作区有修改或删除
                hasBlockingChanges = true;
                qDebug() << "[GitCheckoutDialog] Found working tree changes:" << line;
                break;
            }

            // 未跟踪文件 (??) 和忽略的文件 (!!) 不会阻止 checkout，跳过
        }

        qDebug() << "[GitCheckoutDialog] Has blocking changes:" << hasBlockingChanges;
        return hasBlockingChanges;
    }

    qWarning() << "[GitCheckoutDialog] Failed to check git status, assuming no changes";
    return false;   // 如果检查失败，假设没有更改
}

void GitCheckoutDialog::executeGitCommand(const QStringList &args, const QString &operation)
{
    executeGitCommandWithResult(args, operation);
}

bool GitCheckoutDialog::executeGitCommandWithResult(const QStringList &args, const QString &operation)
{
    qDebug() << "[GitCheckoutDialog] Executing Git command:" << args << "for operation:" << operation;

    GitOperationDialog *dialog = new GitOperationDialog(operation.isEmpty() ? tr("Git Operation") : operation, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // 设置操作描述
    if (!operation.isEmpty()) {
        dialog->setOperationDescription(operation);
    }

    // 连接对话框的 finished 信号，确保无论如何关闭都能处理
    connect(dialog, &QDialog::finished, this, [this, operation](int result) {
        bool success = (result == QDialog::Accepted);
        qDebug() << "[GitCheckoutDialog] GitOperationDialog finished with result:" << result;

        // 无论成功失败，都需要处理后续逻辑
        if (success) {
            qDebug() << "[GitCheckoutDialog] Git operation completed successfully";

            // 优化：不删除整个仓库缓存，而是重置为空状态让系统重新扫描
            // 这样可以保持仓库路径在缓存中，避免isInsideRepositoryFile失效
            if (operation.contains("checkout") || operation.contains("branch")) {
                // 对于分支切换操作，重置文件状态但保留仓库路径
                QTimer::singleShot(50, this, [this]() {
                    // 使用空的版本信息重置缓存，保留仓库路径但清空文件状态
                    QHash<QString, Global::ItemVersion> emptyVersionInfo;
                    emptyVersionInfo.insert(m_repositoryPath, Global::ItemVersion::NormalVersion);
                    Global::Cache::instance().resetVersion(m_repositoryPath, emptyVersionInfo);
                    
                    qDebug() << "[GitCheckoutDialog] Reset repository cache to trigger refresh while preserving repository path";
                    
                    // 发送信号通知状态变化
                    emit repositoryStateChanged(m_repositoryPath);
                });
            }

            showOperationResult(true, operation, tr("Operation completed successfully."));
        } else {
            qWarning() << "[GitCheckoutDialog] Git operation failed or was cancelled";
            showOperationResult(false, operation, tr("Operation failed or was cancelled."));
        }

        // 刷新分支数据以更新当前分支状态
        if (operation.contains("checkout") || operation.contains("branch") || operation.contains("tag")) {
            QTimer::singleShot(100, this, [this]() {
                loadBranchData();   // 延迟刷新确保操作完全完成
            });
        }
    });

    // 执行命令
    dialog->executeCommand(m_repositoryPath, args);

    // 使用 show() 而不是 exec()，这样对话框可以独立运行
    dialog->show();

    // 对于异步操作，我们假设会成功，实际结果通过信号处理
    return true;
}

void GitCheckoutDialog::showOperationResult(bool success, const QString &operation, const QString &message)
{
    if (success) {
        m_statusLabel->setText(tr("✓ %1").arg(message));
        qDebug() << "[GitCheckoutDialog] Operation succeeded:" << operation;
    } else {
        m_statusLabel->setText(tr("✗ %1").arg(message));
        qWarning() << "[GitCheckoutDialog] Operation failed:" << operation;
    }
}

// 数据解析方法 - 需要实现
QVector<BranchItem> GitCheckoutDialog::parseLocalBranches(const QString &output)
{
    QVector<BranchItem> branches;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        BranchItem item;
        item.type = BranchItem::LocalBranch;

        if (trimmed.startsWith("* ")) {
            item.isCurrent = true;
            trimmed = trimmed.mid(2).trimmed();
        }

        // 解析分支名称和最后提交信息
        QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            item.name = parts.first();
            if (parts.size() > 1) {
                item.lastCommitHash = parts[1];
            }
        }

        branches.append(item);
    }

    return branches;
}

QVector<BranchItem> GitCheckoutDialog::parseRemoteBranches(const QString &output)
{
    QVector<BranchItem> branches;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.contains("->")) continue;   // 跳过符号链接

        BranchItem item;
        item.type = BranchItem::RemoteBranch;

        // 解析远程分支名称和最后提交信息
        QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            item.name = parts.first();
            if (parts.size() > 1) {
                item.lastCommitHash = parts[1];
            }
        }

        branches.append(item);
    }

    return branches;
}

QVector<BranchItem> GitCheckoutDialog::parseTags(const QString &output)
{
    QVector<BranchItem> tags;
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        BranchItem item;
        item.type = BranchItem::Tag;
        item.name = trimmed;

        tags.append(item);
    }

    return tags;
}

// 树形控件相关方法 - 需要实现
QTreeWidgetItem *GitCheckoutDialog::createCategoryItem(const QString &title, int count, const QString &icon)
{
    QTreeWidgetItem *item = new QTreeWidgetItem;
    item->setText(0, QString("%1 (%2)").arg(title).arg(count));
    item->setFlags(Qt::ItemIsEnabled);   // 分类节点不可选择

    // 设置字体样式
    QFont font = item->font(0);
    font.setBold(true);
    item->setFont(0, font);

    return item;
}

QTreeWidgetItem *GitCheckoutDialog::createBranchItem(const BranchItem &item)
{
    QTreeWidgetItem *treeItem = new QTreeWidgetItem;

    // 设置分支名称
    QString displayName = item.name;
    if (item.isCurrent) {
        displayName = "● " + displayName;   // 当前分支/标签标记

        // 为当前项设置特殊样式
        QFont font = treeItem->font(0);
        font.setBold(true);
        treeItem->setFont(0, font);

        // 设置背景色
        treeItem->setBackground(0, QColor(230, 255, 230));   // 浅绿色背景
        treeItem->setBackground(1, QColor(230, 255, 230));
        treeItem->setBackground(2, QColor(230, 255, 230));
    }
    treeItem->setText(0, displayName);

    // 设置状态信息
    QString status;
    if (item.isCurrent) {
        if (item.type == BranchItem::Tag) {
            status = tr("[Current Tag]");
        } else {
            status = tr("[Current]");
        }
    } else if (item.type == BranchItem::RemoteBranch) {
        status = tr("[Remote]");
    } else if (item.type == BranchItem::Tag) {
        status = tr("[Tag]");
    }
    treeItem->setText(1, status);

    // 设置最后提交信息
    if (!item.lastCommitHash.isEmpty()) {
        treeItem->setText(2, item.lastCommitHash.left(8));   // 显示短哈希
    }

    // 存储完整的分支信息
    treeItem->setData(0, Qt::UserRole, QVariant::fromValue(item));

    return treeItem;
}

void GitCheckoutDialog::populateCategoryItems(QTreeWidgetItem *categoryItem, const QVector<BranchItem> &items, const QString &highlightText)
{
    for (const BranchItem &item : items) {
        QTreeWidgetItem *branchItem = createBranchItem(item);
        categoryItem->addChild(branchItem);

        // 如果有高亮文本，设置高亮样式
        if (!highlightText.isEmpty() && item.name.contains(highlightText, Qt::CaseInsensitive)) {
            QFont font = branchItem->font(0);
            font.setBold(true);
            branchItem->setFont(0, font);
        }
    }
}

void GitCheckoutDialog::performCheckoutWithChangeCheck(const QString &branchName, const BranchItem &branchInfo)
{
    // 检查是否有本地更改 - 只对非当前分支检查
    if (!branchInfo.isCurrent && hasLocalChanges()) {
        QMessageBox::StandardButton ret = QMessageBox::question(this, tr("Local Changes Detected"),
                                                                tr("You have uncommitted changes. How would you like to proceed?\n\n"
                                                                   "• Stash: Temporarily save your changes and restore them after checkout\n"
                                                                   "• Force: Discard your changes and checkout anyway\n"
                                                                   "• Cancel: Keep your changes and stay on current branch"),
                                                                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        switch (ret) {
        case QMessageBox::Save:   // Stash
            performCheckout(branchName, CheckoutMode::Stash);
            break;
        case QMessageBox::Discard:   // Force
            performCheckout(branchName, CheckoutMode::Force);
            break;
        default:   // Cancel
            return;
        }
    } else {
        performCheckout(branchName, CheckoutMode::Normal);
    }
}
