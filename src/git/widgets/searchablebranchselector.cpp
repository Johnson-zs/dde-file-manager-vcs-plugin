#include "searchablebranchselector.h"

#include <QApplication>
#include <QListWidgetItem>
#include <QMouseEvent>
#include <QScrollBar>
#include <QDebug>
#include <QStyle>
#include <QStyleOption>
#include <QPainter>

SearchableBranchSelector::SearchableBranchSelector(QWidget *parent)
    : QWidget(parent),
      m_displayEdit(nullptr),
      m_searchEdit(nullptr),
      m_dropdownButton(nullptr),
      m_refreshButton(nullptr),
      m_dropdownFrame(nullptr),
      m_listWidget(nullptr),
      m_statusLabel(nullptr),
      m_mainLayout(nullptr),
      m_dropdownLayout(nullptr),
      m_showRemoteBranches(true),
      m_showTags(true),
      m_dropdownVisible(false),
      m_searchTimer(nullptr)
{
    qDebug() << "[SearchableBranchSelector] Initializing simplified branch selector";

    setupUI();
    setupDropdown();
    
    // 安装事件过滤器来监控应用程序的点击事件
    qApp->installEventFilter(this);

    qDebug() << "[SearchableBranchSelector] Simplified branch selector initialized successfully";
}

SearchableBranchSelector::~SearchableBranchSelector()
{
    qDebug() << "[SearchableBranchSelector] Destroying simplified branch selector";
    
    // 移除事件过滤器
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void SearchableBranchSelector::setupUI()
{
    setFixedHeight(32);
    setMinimumWidth(250);

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(2);

    // 显示当前选择的只读编辑框
    m_displayEdit = new QLineEdit;
    m_displayEdit->setReadOnly(true);
    m_displayEdit->setPlaceholderText(tr("Select branch or tag..."));
    m_displayEdit->setStyleSheet(
            "QLineEdit {"
            "    border: 1px solid #ccc;"
            "    border-radius: 4px;"
            "    padding: 4px 8px;"
            "    background-color: white;"
            "}"
            "QLineEdit:focus {"
            "    border-color: #0078d4;"
            "    outline: none;"
            "}");

    // 下拉按钮
    m_dropdownButton = new QPushButton("▼");
    m_dropdownButton->setFixedSize(24, 24);
    m_dropdownButton->setToolTip(tr("Show branch list"));
    m_dropdownButton->setStyleSheet(
            "QPushButton {"
            "    border: 1px solid #ccc;"
            "    border-radius: 4px;"
            "    background-color: #f8f9fa;"
            "    font-size: 10px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #e9ecef;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #dee2e6;"
            "}");

    // 刷新按钮
    m_refreshButton = new QPushButton("🔄");
    m_refreshButton->setFixedSize(24, 24);
    m_refreshButton->setToolTip(tr("Refresh branches and tags"));
    m_refreshButton->setStyleSheet(
            "QPushButton {"
            "    border: 1px solid #ccc;"
            "    border-radius: 4px;"
            "    background-color: #f8f9fa;"
            "    font-size: 10px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #e9ecef;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #dee2e6;"
            "}");

    m_mainLayout->addWidget(m_displayEdit, 1);
    m_mainLayout->addWidget(m_dropdownButton);
    m_mainLayout->addWidget(m_refreshButton);

    // 连接信号
    connect(m_dropdownButton, &QPushButton::clicked, this, &SearchableBranchSelector::onDropdownButtonClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &SearchableBranchSelector::onRefreshClicked);

    // 设置搜索定时器
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(300);
    connect(m_searchTimer, &QTimer::timeout, this, [this]() {
        filterItems(m_searchEdit->text());
    });
}

void SearchableBranchSelector::setupDropdown()
{
    // 创建下拉框架
    m_dropdownFrame = new QFrame(this);
    m_dropdownFrame->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    m_dropdownFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
    m_dropdownFrame->setLineWidth(1);
    m_dropdownFrame->hide();

    m_dropdownLayout = new QVBoxLayout(m_dropdownFrame);
    m_dropdownLayout->setContentsMargins(4, 4, 4, 4);
    m_dropdownLayout->setSpacing(2);

    // 搜索输入框（在下拉框内）
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("🔍 Search branches and tags..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(
            "QLineEdit {"
            "    border: 1px solid #ccc;"
            "    border-radius: 4px;"
            "    padding: 4px 8px;"
            "    background-color: white;"
            "    margin-bottom: 4px;"
            "}"
            "QLineEdit:focus {"
            "    border-color: #0078d4;"
            "    outline: none;"
            "}");
    m_dropdownLayout->addWidget(m_searchEdit);

    // 状态标签
    m_statusLabel = new QLabel;
    m_statusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; padding: 4px; }");
    m_statusLabel->hide();
    m_dropdownLayout->addWidget(m_statusLabel);

    // 列表控件
    m_listWidget = new QListWidget;
    m_listWidget->setMaximumHeight(300);
    m_listWidget->setMinimumWidth(300);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_dropdownLayout->addWidget(m_listWidget);

    // 连接搜索和列表信号
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SearchableBranchSelector::onSearchTextChanged);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_listWidget->currentItem()) {
            onItemDoubleClicked(m_listWidget->currentItem());
        }
    });
    connect(m_listWidget, &QListWidget::itemClicked, this, &SearchableBranchSelector::onItemClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &SearchableBranchSelector::onItemDoubleClicked);

    // 设置样式
    m_dropdownStyleSheet =
            "QFrame {"
            "    background-color: white;"
            "    border: 1px solid #ccc;"
            "    border-radius: 4px;"
            "}"
            "QListWidget {"
            "    border: none;"
            "    background-color: transparent;"
            "    outline: none;"
            "}"
            "QListWidget::item {"
            "    padding: 6px 8px;"
            "    border: none;"
            "    border-radius: 2px;"
            "}"
            "QListWidget::item:hover {"
            "    background-color: #f0f0f0;"
            "}"
            "QListWidget::item:selected {"
            "    background-color: #0078d4;"
            "    color: white;"
            "}"
            "QListWidget::item:disabled {"
            "    color: #999;"
            "    background-color: #f8f9fa;"
            "}";

    m_dropdownFrame->setStyleSheet(m_dropdownStyleSheet);
}

void SearchableBranchSelector::setBranches(const QStringList &localBranches,
                                           const QStringList &remoteBranches,
                                           const QStringList &tags,
                                           const QString &currentBranch)
{
    qDebug() << "[SearchableBranchSelector] Setting branches - Local:" << localBranches.size()
             << "Remote:" << remoteBranches.size() << "Tags:" << tags.size()
             << "Current:" << currentBranch;

    // 清空现有数据
    m_allItems.clear();
    m_currentBranch = currentBranch;

    // 添加特殊项目
    m_allItems.append(BranchTagItem(tr("All Branches"), BranchTagItem::AllBranches));

    // 添加当前分支（如果有）
    if (!currentBranch.isEmpty()) {
        m_allItems.append(BranchTagItem(currentBranch, BranchTagItem::CurrentBranch, true));
    }

    // 添加本地分支
    for (const QString &branch : localBranches) {
        if (branch != currentBranch && !branch.isEmpty()) {
            m_allItems.append(BranchTagItem(branch, BranchTagItem::LocalBranch));
        }
    }

    // 添加远程分支
    for (const QString &branch : remoteBranches) {
        if (!branch.startsWith("origin/HEAD") && !branch.isEmpty()) {
            m_allItems.append(BranchTagItem(branch, BranchTagItem::RemoteBranch));
        }
    }

    // 添加标签
    for (const QString &tag : tags) {
        if (!tag.isEmpty()) {
            m_allItems.append(BranchTagItem(tag, BranchTagItem::Tag));
        }
    }

    // 重新填充下拉列表
    populateDropdown();

    // 设置当前选择
    if (!currentBranch.isEmpty()) {
        setCurrentSelection(currentBranch);
    } else {
        setCurrentSelection(tr("All Branches"));
    }

    qInfo() << QString("INFO: [SearchableBranchSelector] Loaded %1 total items")
                       .arg(m_allItems.size());
}

void SearchableBranchSelector::setCurrentSelection(const QString &branchName)
{
    if (branchName == m_selectedBranch) {
        return;
    }

    m_selectedBranch = branchName;
    updateDisplayText();

    qDebug() << "[SearchableBranchSelector] Selection changed to:" << branchName;
}

QString SearchableBranchSelector::getCurrentSelection() const
{
    return m_selectedBranch;
}

void SearchableBranchSelector::updateDisplayText()
{
    // 更新显示框的文本
    if (m_selectedBranch == "HEAD" || m_selectedBranch.isEmpty()) {
        m_displayEdit->setText(tr("All Branches"));
    } else {
        m_displayEdit->setText(m_selectedBranch);
    }
}

void SearchableBranchSelector::syncDropdownState()
{
    // 确保内部状态与实际显示状态同步
    bool actuallyVisible = m_dropdownFrame && m_dropdownFrame->isVisible();
    
    if (m_dropdownVisible != actuallyVisible) {
        qDebug() << "[SearchableBranchSelector] State sync: internal=" << m_dropdownVisible 
                 << "actual=" << actuallyVisible << "- correcting";
        m_dropdownVisible = actuallyVisible;
    }
    
    // 更新按钮文本以反映当前状态
    m_dropdownButton->setText(m_dropdownVisible ? "▲" : "▼");
}

void SearchableBranchSelector::populateDropdown()
{
    m_listWidget->clear();

    // 按类型分组显示
    QList<BranchTagItem> specialItems;
    QList<BranchTagItem> localItems;
    QList<BranchTagItem> remoteItems;
    QList<BranchTagItem> tagItems;

    for (const auto &item : m_allItems) {
        switch (item.type) {
        case BranchTagItem::AllBranches:
        case BranchTagItem::CurrentBranch:
            specialItems.append(item);
            break;
        case BranchTagItem::LocalBranch:
            localItems.append(item);
            break;
        case BranchTagItem::RemoteBranch:
            remoteItems.append(item);
            break;
        case BranchTagItem::Tag:
            tagItems.append(item);
            break;
        }
    }

    // 添加特殊项目
    if (!specialItems.isEmpty()) {
        addCategoryItems("", specialItems);
        if (!localItems.isEmpty() || (!remoteItems.isEmpty() && m_showRemoteBranches) || (!tagItems.isEmpty() && m_showTags)) {
            addSeparator();
        }
    }

    // 添加本地分支
    if (!localItems.isEmpty()) {
        addSeparator(tr("Local Branches"));
        addCategoryItems("", localItems);
    }

    // 添加远程分支
    if (!remoteItems.isEmpty() && m_showRemoteBranches) {
        addSeparator(tr("Remote Branches"));
        addCategoryItems("", remoteItems);
    }

    // 添加标签
    if (!tagItems.isEmpty() && m_showTags) {
        addSeparator(tr("Tags"));
        addCategoryItems("", tagItems);
    }

    // 更新状态
    int visibleItems = specialItems.size() + localItems.size() + (m_showRemoteBranches ? remoteItems.size() : 0) + (m_showTags ? tagItems.size() : 0);

    if (visibleItems == 0) {
        m_statusLabel->setText(tr("No branches or tags found"));
        m_statusLabel->show();
    } else {
        m_statusLabel->hide();
    }

    // 选中当前项目
    selectCurrentItemInDropdown();
}

void SearchableBranchSelector::filterItems(const QString &searchText)
{
    if (searchText.isEmpty()) {
        populateDropdown();
        return;
    }

    m_listWidget->clear();

    // 搜索并添加匹配的项目
    QList<BranchTagItem> matchedItems;

    for (const auto &item : m_allItems) {
        // 跳过不显示的类型
        if (item.type == BranchTagItem::RemoteBranch && !m_showRemoteBranches) {
            continue;
        }
        if (item.type == BranchTagItem::Tag && !m_showTags) {
            continue;
        }

        if (matchesSearch(item, searchText)) {
            matchedItems.append(item);
        }
    }

    if (matchedItems.isEmpty()) {
        m_statusLabel->setText(tr("No matches found for '%1'").arg(searchText));
        m_statusLabel->show();
    } else {
        m_statusLabel->hide();
        addCategoryItems("", matchedItems, searchText);
        // 搜索后也尝试选中当前项目（如果它在搜索结果中）
        selectCurrentItemInDropdown();
    }
}

void SearchableBranchSelector::addCategoryItems(const QString &categoryName,
                                                const QList<BranchTagItem> &items,
                                                const QString &searchText)
{
    Q_UNUSED(categoryName)

    for (const auto &item : items) {
        auto *listItem = new QListWidgetItem;
        listItem->setText(item.displayName);
        listItem->setIcon(item.getIcon());
        listItem->setData(Qt::UserRole, QVariant::fromValue(item));

        // 如果是当前分支，设置特殊样式
        if (item.isCurrent) {
            QFont font = listItem->font();
            font.setBold(true);
            listItem->setFont(font);
        }

        // 如果有搜索文本，高亮匹配部分
        if (!searchText.isEmpty() && item.name.contains(searchText, Qt::CaseInsensitive)) {
            listItem->setBackground(QBrush(QColor(255, 255, 0, 50)));
        }

        m_listWidget->addItem(listItem);
    }
}

void SearchableBranchSelector::addSeparator(const QString &text)
{
    if (!text.isEmpty()) {
        auto *separatorItem = new QListWidgetItem(text);
        separatorItem->setFlags(Qt::NoItemFlags);
        QFont font = separatorItem->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() - 1);
        separatorItem->setFont(font);
        separatorItem->setBackground(QBrush(QColor(240, 240, 240)));
        m_listWidget->addItem(separatorItem);
    } else {
        // 添加空的分隔符
        auto *separatorItem = new QListWidgetItem("");
        separatorItem->setFlags(Qt::NoItemFlags);
        separatorItem->setSizeHint(QSize(0, 4));
        separatorItem->setBackground(QBrush(QColor(240, 240, 240)));
        m_listWidget->addItem(separatorItem);
    }
}

bool SearchableBranchSelector::matchesSearch(const BranchTagItem &item, const QString &searchText) const
{
    return item.name.contains(searchText, Qt::CaseInsensitive) || item.displayName.contains(searchText, Qt::CaseInsensitive);
}

void SearchableBranchSelector::selectItem(const QString &branchName)
{
    setCurrentSelection(branchName);
    emit selectionChanged(branchName);
}

void SearchableBranchSelector::showDropdown()
{
    // 先同步状态
    syncDropdownState();
    
    if (m_dropdownVisible) {
        return;
    }

    // 计算下拉框位置
    QPoint globalPos = mapToGlobal(QPoint(0, height()));
    m_dropdownFrame->move(globalPos);
    m_dropdownFrame->resize(width(), qMin(350, m_listWidget->sizeHint().height() + 80));

    // 清空搜索框
    m_searchEdit->clear();

    // 重新填充列表（显示所有项目）并自动选中当前项目
    populateDropdown();

    // 显示下拉框
    m_dropdownFrame->show();
    m_dropdownFrame->raise();
    m_dropdownFrame->activateWindow();

    // 更新状态 - 确保状态与实际显示同步
    m_dropdownVisible = true;
    m_dropdownButton->setText("▲");

    // 聚焦到搜索框
    m_searchEdit->setFocus();

    qDebug() << "[SearchableBranchSelector] Dropdown shown";
}

void SearchableBranchSelector::hideDropdown()
{
    // 先同步状态
    syncDropdownState();
    
    if (!m_dropdownVisible) {
        return;
    }

    // 隐藏下拉框
    m_dropdownFrame->hide();
    
    // 立即更新状态 - 确保状态与实际显示同步
    m_dropdownVisible = false;
    m_dropdownButton->setText("▼");

    // 将焦点返回到主组件
    m_displayEdit->setFocus();

    qDebug() << "[SearchableBranchSelector] Dropdown hidden";
}

void SearchableBranchSelector::onSearchTextChanged()
{
    // 延迟搜索以避免频繁更新
    m_searchTimer->start();
}

void SearchableBranchSelector::onItemClicked(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }

    BranchTagItem branchItem = item->data(Qt::UserRole).value<BranchTagItem>();
    if (!branchItem.name.isEmpty()) {
        selectItem(branchItem.name);
        // 单击选择后隐藏下拉框，提供更好的用户体验
        hideDropdown();
    }
}

void SearchableBranchSelector::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
        return;
    }

    BranchTagItem branchItem = item->data(Qt::UserRole).value<BranchTagItem>();
    if (!branchItem.name.isEmpty()) {
        selectItem(branchItem.name);
        emit branchActivated(branchItem.name);
        hideDropdown();
    }
}

void SearchableBranchSelector::onDropdownButtonClicked()
{
    // 在切换前先同步状态
    syncDropdownState();
    
    if (m_dropdownVisible) {
        hideDropdown();
    } else {
        showDropdown();
    }
}

void SearchableBranchSelector::onRefreshClicked()
{
    emit refreshRequested();
}

void SearchableBranchSelector::setShowRemoteBranches(bool show)
{
    if (m_showRemoteBranches != show) {
        m_showRemoteBranches = show;
        if (m_dropdownVisible) {
            populateDropdown();
        }
    }
}

void SearchableBranchSelector::setShowTags(bool show)
{
    if (m_showTags != show) {
        m_showTags = show;
        if (m_dropdownVisible) {
            populateDropdown();
        }
    }
}

void SearchableBranchSelector::setPlaceholderText(const QString &text)
{
    m_displayEdit->setPlaceholderText(text);
}

void SearchableBranchSelector::keyPressEvent(QKeyEvent *event)
{
    // 在处理按键事件前先同步状态
    syncDropdownState();
    
    if (m_dropdownVisible) {
        switch (event->key()) {
        case Qt::Key_Escape:
            hideDropdown();
            event->accept();
            return;
        case Qt::Key_Up:
            navigateList(-1);
            event->accept();
            return;
        case Qt::Key_Down:
            navigateList(1);
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_listWidget->currentItem()) {
                onItemDoubleClicked(m_listWidget->currentItem());
            }
            event->accept();
            return;
        }
    } else {
        // 如果下拉框没有显示，某些按键可以显示它
        switch (event->key()) {
        case Qt::Key_Down:
        case Qt::Key_F4:
        case Qt::Key_Space:
            showDropdown();
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

void SearchableBranchSelector::focusInEvent(QFocusEvent *event)
{
    // 聚焦到显示框
    m_displayEdit->setFocus();
    QWidget::focusInEvent(event);
}

void SearchableBranchSelector::focusOutEvent(QFocusEvent *event)
{
    qDebug() << "[SearchableBranchSelector] Focus out event, reason:" << event->reason();
    
    // 简化焦点丢失处理 - 使用更短的延迟并简化逻辑
    QTimer::singleShot(50, this, [this]() {
        // 先同步状态
        syncDropdownState();
        
        // 检查焦点是否仍在组件相关的控件上
        QWidget *focusWidget = QApplication::focusWidget();
        bool shouldHide = true;
        
        if (focusWidget) {
            // 检查焦点是否在主组件或下拉框的任何子控件上
            QWidget *parent = focusWidget;
            while (parent) {
                if (parent == this || parent == m_dropdownFrame) {
                    shouldHide = false;
                    break;
                }
                parent = parent->parentWidget();
            }
        }
        
        // 如果焦点完全离开了组件，隐藏下拉框
        if (shouldHide && m_dropdownVisible) {
            qDebug() << "[SearchableBranchSelector] Focus left component, hiding dropdown";
            hideDropdown();
        }
    });

    QWidget::focusOutEvent(event);
}

void SearchableBranchSelector::navigateList(int direction)
{
    if (!m_listWidget || m_listWidget->count() == 0) {
        return;
    }

    int currentRow = m_listWidget->currentRow();
    int newRow = currentRow + direction;

    // 跳过分隔符项目
    while (newRow >= 0 && newRow < m_listWidget->count()) {
        QListWidgetItem *item = m_listWidget->item(newRow);
        if (item && (item->flags() & Qt::ItemIsEnabled)) {
            break;
        }
        newRow += direction;
    }

    if (newRow >= 0 && newRow < m_listWidget->count()) {
        m_listWidget->setCurrentRow(newRow);
        m_listWidget->scrollToItem(m_listWidget->currentItem());
    }
}

bool SearchableBranchSelector::eventFilter(QObject *obj, QEvent *event)
{
    // 只在下拉框可见时处理鼠标点击事件
    if (obj == qApp && event->type() == QEvent::MouseButtonPress) {
        // 先同步状态
        syncDropdownState();
        
        if (m_dropdownVisible) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint globalPos = mouseEvent->globalPosition().toPoint();
            
            // 检查点击是否在组件区域内
            QRect mainWidgetRect = QRect(mapToGlobal(QPoint(0, 0)), size());
            QRect dropdownRect = m_dropdownFrame->geometry();
            
            // 如果点击在主组件或下拉框外部，隐藏下拉框
            if (!mainWidgetRect.contains(globalPos) && !dropdownRect.contains(globalPos)) {
                qDebug() << "[SearchableBranchSelector] Click outside component, hiding dropdown";
                hideDropdown();
            }
        }
    }
    
    return QWidget::eventFilter(obj, event);
}

void SearchableBranchSelector::selectCurrentItemInDropdown()
{
    if (!m_listWidget || m_selectedBranch.isEmpty()) {
        return;
    }

    // 遍历列表项，找到匹配当前选择的项目
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (!item || !(item->flags() & Qt::ItemIsEnabled)) {
            continue;
        }

        // 获取项目数据
        QVariant itemData = item->data(Qt::UserRole);
        if (itemData.isValid()) {
            BranchTagItem branchItem = itemData.value<BranchTagItem>();
            if (branchItem.name == m_selectedBranch) {
                // 找到匹配的项目，选中它
                m_listWidget->setCurrentItem(item);
                m_listWidget->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                
                qDebug() << "[SearchableBranchSelector] Auto-selected item in dropdown:" << m_selectedBranch;
                return;
            }
        }
    }

    qDebug() << "[SearchableBranchSelector] Could not find current selection in dropdown:" << m_selectedBranch;
}
