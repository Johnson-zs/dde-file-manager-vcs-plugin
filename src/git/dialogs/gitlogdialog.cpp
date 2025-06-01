#include "gitlogdialog.h"
#include "gitoperationdialog.h"
#include "gitdialogs.h"
#include "widgets/linenumbertextedit.h"
#include "gitfilepreviewdialog.h"

#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QSizePolicy>
#include <QFont>
#include <QDateTime>
#include <QDir>
#include <QTabWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QProcess>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextBlock>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>

GitLogDialog::GitLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent),
      m_repositoryPath(repositoryPath),
      m_filePath(filePath),
      m_branchCombo(nullptr),
      m_searchEdit(nullptr),
      m_refreshButton(nullptr),
      m_settingsButton(nullptr),
      m_mainSplitter(nullptr),
      m_rightSplitter(nullptr),
      m_commitTree(nullptr),
      m_commitScrollBar(nullptr),
      m_commitDetails(nullptr),
      m_changedFilesTree(nullptr),
      m_diffView(nullptr),
      m_diffHighlighter(nullptr),
      m_commitContextMenu(nullptr),
      m_fileContextMenu(nullptr),
      m_isLoadingMore(false),
      m_currentOffset(0),
      m_loadTimer(nullptr),
      m_searchTimer(nullptr),
      m_isSearching(false),
      m_searchLoadingMore(false),
      m_searchTotalFound(0),
      m_searchStatusLabel(nullptr),
      m_currentPreviewDialog(nullptr),
      m_enableChangeStats(true)  // 默认启用改动统计
{
    qInfo() << "INFO: [GitLogDialog] Initializing GitKraken-style log dialog for repository:" << repositoryPath;
    
    setupUI();
    setupContextMenus();
    setupInfiniteScroll();
    
    // 安装事件过滤器来捕获文件列表的键盘事件
    m_changedFilesTree->installEventFilter(this);
    
    // 加载数据
    loadBranches();
    loadCommitHistory();
    
    qInfo() << "INFO: [GitLogDialog] GitKraken-style log dialog initialized successfully";
}

GitLogDialog::~GitLogDialog()
{
    qInfo() << "INFO: [GitLogDialog] Destroying GitKraken-style log dialog";
}

void GitLogDialog::setupUI()
{
    setWindowTitle(m_filePath.isEmpty() ? 
        tr("Git Log - Repository") : 
        tr("Git Log - %1").arg(QFileInfo(m_filePath).fileName()));
    
    setModal(false);
    setMinimumSize(1200, 800);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1600, 1000);
    setAttribute(Qt::WA_DeleteOnClose);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    setupMainLayout();
    
    // 添加工具栏布局
    auto *toolbarLayout = new QHBoxLayout;
    toolbarLayout->setSpacing(8);

    // 分支选择
    toolbarLayout->addWidget(new QLabel(tr("Branch:")));
    m_branchCombo = new QComboBox;
    m_branchCombo->setMinimumWidth(180);
    m_branchCombo->setToolTip(tr("Select branch to view commit history"));
    toolbarLayout->addWidget(m_branchCombo);

    toolbarLayout->addSpacing(16);

    // 搜索框
    toolbarLayout->addWidget(new QLabel(tr("Search:")));
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search commits, authors, messages..."));
    m_searchEdit->setMinimumWidth(250);
    m_searchEdit->setToolTip(tr("Search in commit messages, authors, and hashes"));
    toolbarLayout->addWidget(m_searchEdit);

    // 搜索状态标签
    m_searchStatusLabel = new QLabel;
    m_searchStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");
    m_searchStatusLabel->hide(); // 初始隐藏
    toolbarLayout->addWidget(m_searchStatusLabel);

    toolbarLayout->addSpacing(16);

    // 操作按钮
    m_refreshButton = new QPushButton(tr("Refresh"));
    m_refreshButton->setIcon(QIcon::fromTheme("view-refresh"));
    m_refreshButton->setToolTip(tr("Refresh commit history"));
    toolbarLayout->addWidget(m_refreshButton);

    m_settingsButton = new QPushButton(tr("Settings"));
    m_settingsButton->setIcon(QIcon::fromTheme("configure"));
    m_settingsButton->setToolTip(tr("Configure log display options"));
    toolbarLayout->addWidget(m_settingsButton);

    toolbarLayout->addStretch();

    // 连接信号
    connect(m_branchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitLogDialog::onBranchChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &GitLogDialog::onSearchTextChanged);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &GitLogDialog::onRefreshClicked);
    connect(m_settingsButton, &QPushButton::clicked,
            this, &GitLogDialog::onSettingsClicked);
    
    mainLayout->addLayout(toolbarLayout);
    mainLayout->addWidget(m_mainSplitter);
}

void GitLogDialog::setupMainLayout()
{
    // 主分割器（水平）
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    
    setupCommitList();
    setupRightPanel();
    
    // 设置分割器比例：左侧40%，右侧60%
    m_mainSplitter->addWidget(m_commitTree);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setSizes({400, 600});
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 3);
}

void GitLogDialog::setupCommitList()
{
    m_commitTree = new QTreeWidget;
    m_commitTree->setHeaderLabels({tr("Graph"), tr("Message"), tr("Author"), tr("Date"), tr("Hash")});
    m_commitTree->setRootIsDecorated(false);
    m_commitTree->setAlternatingRowColors(true);
    m_commitTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_commitTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置列宽
    m_commitTree->setColumnWidth(0, 60);   // Graph
    m_commitTree->setColumnWidth(1, 300);  // Message
    m_commitTree->setColumnWidth(2, 120);  // Author
    m_commitTree->setColumnWidth(3, 120);  // Date
    m_commitTree->setColumnWidth(4, 80);   // Hash
    
    // 设置字体
    QFont commitFont("Consolas", 9);
    m_commitTree->setFont(commitFont);
    
    // 连接信号
    connect(m_commitTree, &QTreeWidget::itemSelectionChanged,
            this, &GitLogDialog::onCommitSelectionChanged);
    connect(m_commitTree, &QTreeWidget::customContextMenuRequested,
            this, &GitLogDialog::showCommitContextMenu);
}

void GitLogDialog::setupRightPanel()
{
    // 右侧分割器（垂直）
    m_rightSplitter = new QSplitter(Qt::Vertical);
    
    setupCommitDetails();
    setupFilesList();
    setupDiffView();
    
    m_rightSplitter->addWidget(m_commitDetails);
    m_rightSplitter->addWidget(m_changedFilesTree);
    m_rightSplitter->addWidget(m_diffView);
    
    // 设置比例：详情20%，文件列表30%，差异50%
    m_rightSplitter->setSizes({200, 300, 500});
    m_rightSplitter->setStretchFactor(0, 1);
    m_rightSplitter->setStretchFactor(1, 1);
    m_rightSplitter->setStretchFactor(2, 2);
}

void GitLogDialog::setupCommitDetails()
{
    m_commitDetails = new QTextEdit;
    m_commitDetails->setReadOnly(true);
    m_commitDetails->setMaximumHeight(200);
    m_commitDetails->setFont(QFont("Consolas", 9));
    m_commitDetails->setPlainText(tr("Select a commit to view details..."));
    
    // 启用HTML支持以显示彩色文本
    m_commitDetails->setAcceptRichText(true);
    
    // 设置样式
    m_commitDetails->setStyleSheet(
        "QTextEdit {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "}"
    );
}

void GitLogDialog::setupFilesList()
{
    m_changedFilesTree = new QTreeWidget;
    m_changedFilesTree->setHeaderLabels({tr("Status"), tr("File"), tr("Changes")});
    m_changedFilesTree->setRootIsDecorated(false);
    m_changedFilesTree->setAlternatingRowColors(true);
    m_changedFilesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    // 设置列宽
    m_changedFilesTree->setColumnWidth(0, 60);   // Status
    m_changedFilesTree->setColumnWidth(1, 300);  // File
    m_changedFilesTree->setColumnWidth(2, 100);  // Changes
    
    // 连接信号
    connect(m_changedFilesTree, &QTreeWidget::itemSelectionChanged,
            this, &GitLogDialog::onFileSelectionChanged);
    connect(m_changedFilesTree, &QTreeWidget::itemDoubleClicked,
            this, &GitLogDialog::onFileDoubleClicked);
    connect(m_changedFilesTree, &QTreeWidget::customContextMenuRequested,
            this, &GitLogDialog::showFileContextMenu);
}

void GitLogDialog::setupDiffView()
{
    m_diffView = new LineNumberTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 9));
    m_diffView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_diffView->setPlainText(tr("Select a file to view changes..."));
    
    // 设置语法高亮
    m_diffHighlighter = new GitDiffSyntaxHighlighter(m_diffView->document());
}

void GitLogDialog::setupInfiniteScroll()
{
    m_commitScrollBar = m_commitTree->verticalScrollBar();
    connect(m_commitScrollBar, &QScrollBar::valueChanged,
            this, &GitLogDialog::onScrollValueChanged);
    
    // 设置加载定时器
    m_loadTimer = new QTimer(this);
    m_loadTimer->setSingleShot(true);
    m_loadTimer->setInterval(300); // 300ms延迟
    connect(m_loadTimer, &QTimer::timeout,
            this, &GitLogDialog::loadMoreCommitsIfNeeded);
    
    // 设置搜索定时器
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(500); // 500ms延迟
    connect(m_searchTimer, &QTimer::timeout,
            this, [this]() { startProgressiveSearch(m_currentSearchText); });
    
    // 添加一个定时器来检查是否需要初始加载更多内容
    QTimer::singleShot(1000, this, [this]() {
        // 延迟检查，确保界面已经完全显示
        checkIfNeedMoreCommits();
    });
}

void GitLogDialog::loadCommitHistory(bool append)
{
    if (m_isLoadingMore && append) {
        return; // 避免重复加载
    }
    
    if (append) {
        m_isLoadingMore = true;
    } else {
        m_commitTree->clear();
        m_currentOffset = 0;
        m_commitDetailsCache.clear();
        m_commitFilesCache.clear();
        m_fileDiffCache.clear();
    }
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "log"
         << "--oneline"
         << "--graph"
         << "--pretty=format:%h|%s|%an|%ad|%H"
         << "--date=short"
         << QString("--skip=%1").arg(m_currentOffset)
         << QString("--max-count=%1").arg(COMMITS_PER_LOAD);
    
    // 如果指定了文件路径，只显示该文件的历史
    if (!m_filePath.isEmpty()) {
        QDir repoDir(m_repositoryPath);
        QString relativePath = repoDir.relativeFilePath(m_filePath);
        args << "--" << relativePath;
    }
    
    // 如果选择了特定分支
    QString currentBranch = m_branchCombo->currentData().toString();
    if (!currentBranch.isEmpty() && currentBranch != "HEAD") {
        args.insert(1, currentBranch);
    }
    
    qDebug() << "[GitLogDialog] Loading commits with args:" << args;
    
    process.start("git", args);
    if (!process.waitForFinished(10000)) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Failed to load commit history: %1").arg(process.errorString()));
        m_isLoadingMore = false;
        return;
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    int loadedCount = 0;
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) continue;
        
        // 解析git log输出 - 正确处理graph和commit信息的分离
        // 使用正则表达式找到commit hash的位置，从那里开始解析commit数据
        QRegularExpression commitRegex(R"(([a-f0-9]{7,})\|(.+)\|(.+)\|(.+)\|([a-f0-9]{40})$)");
        QRegularExpressionMatch match = commitRegex.match(line);
        
        if (match.hasMatch()) {
            auto *item = new QTreeWidgetItem(m_commitTree);
            
            // 提取commit数据
            QString shortHash = match.captured(1);
            QString message = match.captured(2);
            QString author = match.captured(3);
            QString date = match.captured(4);
            QString fullHash = match.captured(5);
            
            // 提取graph部分 - 从行开始到commit hash之前的所有内容
            int commitDataStart = match.capturedStart();
            QString graphPart = line.left(commitDataStart).trimmed();
            
            // Graph列 - 保留原始的图形信息，但简化显示
            if (graphPart.isEmpty()) {
                item->setText(0, "●"); // 简单的圆点表示commit
            } else {
                // 保留图形的基本结构，但清理和简化显示
                QString cleanGraph = graphPart;
                cleanGraph.replace("*", "●");
                // 限制显示长度，避免过长的graph字符串
                if (cleanGraph.length() > 10) {
                    cleanGraph = cleanGraph.left(8) + "…";
                }
                item->setText(0, cleanGraph);
            }
            item->setToolTip(0, graphPart.isEmpty() ? "Commit" : graphPart);
            
            // Message列
            item->setText(1, message.trimmed());
            
            // Author列
            item->setText(2, author.trimmed());
            
            // Date列
            item->setText(3, date.trimmed());
            
            // Hash列 - 显示短哈希
            item->setText(4, shortHash);
            item->setData(4, Qt::UserRole, fullHash); // 存储完整哈希
            
            // 设置工具提示
            item->setToolTip(1, message.trimmed());
            item->setToolTip(4, fullHash);
            
            loadedCount++;
        } else {
            // 如果正则表达式匹配失败，尝试备用解析方法
            qDebug() << "[GitLogDialog] Failed to parse line with regex, trying fallback:" << line;
            
            // 备用方法：从右侧查找最后5个|分隔的字段
            QStringList allParts = line.split('|');
            if (allParts.size() >= 5) {
                // 取最后5个部分作为commit数据
                QStringList commitParts = allParts.mid(allParts.size() - 5);
                
                auto *item = new QTreeWidgetItem(m_commitTree);
                
                // Graph部分是除了最后5个字段之外的所有内容
                QStringList graphParts = allParts.mid(0, allParts.size() - 5);
                QString graphPart = graphParts.join("|").trimmed();
                
                if (graphPart.isEmpty()) {
                    item->setText(0, "●");
                } else {
                    QString cleanGraph = graphPart;
                    cleanGraph.replace("*", "●");
                    if (cleanGraph.length() > 10) {
                        cleanGraph = cleanGraph.left(8) + "…";
                    }
                    item->setText(0, cleanGraph);
                }
                item->setToolTip(0, graphPart.isEmpty() ? "Commit" : graphPart);
                
                // Commit数据
                item->setText(1, commitParts[1].trimmed()); // Message
                item->setText(2, commitParts[2].trimmed()); // Author
                item->setText(3, commitParts[3].trimmed()); // Date
                item->setText(4, commitParts[0].trimmed()); // Short Hash
                item->setData(4, Qt::UserRole, commitParts[4].trimmed()); // Full Hash
                
                // 设置工具提示
                item->setToolTip(1, commitParts[1].trimmed());
                item->setToolTip(4, commitParts[4].trimmed());
                
                loadedCount++;
            }
        }
    }
    
    m_currentOffset += loadedCount;
    m_isLoadingMore = false;
    
    qInfo() << QString("INFO: [GitLogDialog] Loaded %1 commits (total offset: %2)")
               .arg(loadedCount).arg(m_currentOffset);
    
    // 如果是首次加载且有结果，选中第一项
    if (!append && m_commitTree->topLevelItemCount() > 0) {
        m_commitTree->setCurrentItem(m_commitTree->topLevelItem(0));
        
        // 延迟检查是否需要加载更多内容（如果没有滚动条）
        QTimer::singleShot(500, this, [this]() {
            checkIfNeedMoreCommits();
        });
    }
}

void GitLogDialog::loadBranches()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    // 首先获取当前分支
    QStringList currentBranchArgs;
    currentBranchArgs << "branch" << "--show-current";
    
    process.start("git", currentBranchArgs);
    QString currentBranch;
    if (process.waitForFinished(5000)) {
        currentBranch = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    }
    
    // 然后获取所有分支
    QStringList args;
    args << "branch" << "-a" << "--format=%(refname:short)";
    
    process.start("git", args);
    if (!process.waitForFinished(5000)) {
        qWarning() << "WARNING: [GitLogDialog] Failed to load branches:" << process.errorString();
        return;
    }
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList branches = output.split('\n', Qt::SkipEmptyParts);
    
    m_branchCombo->clear();
    
    // 如果有当前分支，设为默认选项
    if (!currentBranch.isEmpty()) {
        m_branchCombo->addItem(QString("● %1 (current)").arg(currentBranch), currentBranch);
    }
    
    // 添加所有分支选项
    m_branchCombo->addItem(tr("All Branches"), "HEAD");
    
    for (const QString &branch : branches) {
        QString cleanBranch = branch.trimmed();
        if (!cleanBranch.isEmpty() && !cleanBranch.startsWith("origin/HEAD") && cleanBranch != currentBranch) {
            m_branchCombo->addItem(cleanBranch, cleanBranch);
        }
    }
    
    qDebug() << "[GitLogDialog] Loaded" << branches.size() << "branches, current branch:" << currentBranch;
}

// === 槽函数实现 ===

void GitLogDialog::onCommitSelectionChanged()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) {
        return;
    }
    
    loadCommitDetails(commitHash);
    loadCommitFiles(commitHash);
}

void GitLogDialog::onRefreshClicked()
{
    qInfo() << "INFO: [GitLogDialog] Refreshing commit history";
    loadBranches();
    loadCommitHistory(false);
}

void GitLogDialog::onBranchChanged()
{
    qInfo() << "INFO: [GitLogDialog] Branch changed to:" << m_branchCombo->currentText();
    loadCommitHistory(false);
}

void GitLogDialog::onSearchTextChanged()
{
    m_currentSearchText = m_searchEdit->text().trimmed();
    
    if (m_currentSearchText.isEmpty()) {
        // 清空搜索，恢复正常显示
        finishProgressiveSearch();
        filterCommits("");
        return;
    }
    
    // 延迟开始搜索
    m_searchTimer->start();
}

void GitLogDialog::onScrollValueChanged(int value)
{
    // 检查是否接近底部
    int maximum = m_commitScrollBar->maximum();
    if (maximum > 0 && value >= maximum - PRELOAD_THRESHOLD) {
        qDebug() << "[GitLogDialog] Scroll near bottom, triggering load more. Value:" << value << "Maximum:" << maximum;
        m_loadTimer->start(); // 延迟加载
    }
}

void GitLogDialog::onFileSelectionChanged()
{
    QString commitHash = getCurrentSelectedCommitHash();
    QString filePath = getCurrentSelectedFilePath();
    
    if (!commitHash.isEmpty() && !filePath.isEmpty()) {
        loadFileDiff(commitHash, filePath);
    } else {
        // 如果没有选中文件，清空预览区域
        m_diffView->setPlainText(tr("Select a file to view changes..."));
    }
}

void GitLogDialog::onFileDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    if (item) {
        showFileDiff();
    }
}

// === 辅助方法实现 ===

QString GitLogDialog::getCurrentSelectedCommitHash() const
{
    auto selectedItems = m_commitTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return QString();
    }
    
    return selectedItems.first()->data(4, Qt::UserRole).toString();
}

QString GitLogDialog::getCurrentSelectedFilePath() const
{
    auto selectedItems = m_changedFilesTree->selectedItems();
    if (selectedItems.isEmpty()) {
        return QString();
    }
    
    return selectedItems.first()->data(1, Qt::UserRole).toString();
}

void GitLogDialog::loadMoreCommitsIfNeeded()
{
    if (!m_isLoadingMore) {
        qInfo() << "INFO: [GitLogDialog] Loading more commits due to scroll position";
        loadCommitHistory(true);
    }
}

void GitLogDialog::filterCommits(const QString &searchText)
{
    if (searchText.isEmpty()) {
        // 显示所有项目，清除高亮
        for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
            item->setHidden(false);
            clearItemHighlight(item);
        }
        return;
    }
    
    // 过滤当前已加载的项目
    int visibleCount = 0;
    for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
        bool matches = false;
        
        // 检查消息、作者、哈希
        for (int col = 1; col <= 4; ++col) {
            if (item->text(col).contains(searchText, Qt::CaseInsensitive)) {
                matches = true;
                break;
            }
        }
        
        item->setHidden(!matches);
        if (matches) {
            visibleCount++;
            // 高亮匹配的项目
            highlightItemMatches(item, searchText);
        } else {
            // 清除不匹配项目的高亮
            clearItemHighlight(item);
        }
    }
    
    // 更新搜索状态
    if (m_isSearching) {
        m_searchTotalFound = visibleCount;
        updateSearchStatus();
    }
}

void GitLogDialog::highlightItemMatches(QTreeWidgetItem *item, const QString &searchText)
{
    if (!item || searchText.isEmpty()) {
        return;
    }
    
    // 为匹配的项目设置背景色
    QColor highlightColor(255, 255, 0, 80); // 淡黄色背景
    
    for (int col = 1; col <= 4; ++col) {
        QString text = item->text(col);
        if (text.contains(searchText, Qt::CaseInsensitive)) {
            item->setBackground(col, QBrush(highlightColor));
            
            // 设置工具提示显示匹配信息
            QString tooltip = item->toolTip(col);
            if (!tooltip.contains("Match:")) {
                tooltip += QString("\nMatch: '%1'").arg(searchText);
                item->setToolTip(col, tooltip);
            }
        }
    }
}

void GitLogDialog::clearItemHighlight(QTreeWidgetItem *item)
{
    if (!item) {
        return;
    }
    
    // 清除所有列的背景色
    for (int col = 0; col < item->columnCount(); ++col) {
        item->setBackground(col, QBrush());
        
        // 清除匹配相关的工具提示
        QString tooltip = item->toolTip(col);
        if (tooltip.contains("Match:")) {
            int matchIndex = tooltip.indexOf("\nMatch:");
            if (matchIndex >= 0) {
                tooltip = tooltip.left(matchIndex);
                item->setToolTip(col, tooltip);
            }
        }
    }
}

void GitLogDialog::startProgressiveSearch(const QString &searchText)
{
    if (searchText.isEmpty()) {
        return;
    }
    
    qInfo() << "INFO: [GitLogDialog] Starting progressive search for:" << searchText;
    
    m_isSearching = true;
    m_searchLoadingMore = false;
    m_searchTotalFound = 0;
    
    // 设置鼠标为等待状态
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // 显示搜索状态
    m_searchStatusLabel->show();
    updateSearchStatus();
    
    // 先过滤当前已加载的commits
    filterCommits(searchText);
    
    // 如果当前结果较少，开始加载更多commits进行搜索
    if (m_searchTotalFound < 20 && m_commitTree->topLevelItemCount() == m_currentOffset) {
        continueProgressiveSearch();
    } else {
        // 结果足够或没有更多commits，完成搜索
        finishProgressiveSearch();
    }
}

void GitLogDialog::continueProgressiveSearch()
{
    if (!m_isSearching || m_searchLoadingMore) {
        return;
    }
    
    m_searchLoadingMore = true;
    updateSearchStatus();
    
    qDebug() << "[GitLogDialog] Loading more commits for search, current found:" << m_searchTotalFound;
    
    // 记录加载前的commit数量
    int previousCommitCount = m_commitTree->topLevelItemCount();
    
    // 加载更多commits
    loadCommitHistory(true);
    
    // 加载完成后继续搜索
    QTimer::singleShot(100, this, [this, previousCommitCount]() {
        m_searchLoadingMore = false;
        
        if (m_isSearching) {
            // 检查是否真的加载了新的commits
            int currentCommitCount = m_commitTree->topLevelItemCount();
            bool hasNewCommits = currentCommitCount > previousCommitCount;
            
            // 重新过滤所有commits
            filterCommits(m_currentSearchText);
            
            // 检查是否需要继续加载
            if (hasNewCommits && m_searchTotalFound < 50) {
                // 有新commits且结果还不够，继续搜索
                continueProgressiveSearch();
            } else {
                // 没有新commits或结果已足够，完成搜索
                finishProgressiveSearch();
            }
        }
    });
}

void GitLogDialog::finishProgressiveSearch()
{
    if (!m_isSearching) {
        return;
    }
    
    qInfo() << "INFO: [GitLogDialog] Progressive search completed, found:" << m_searchTotalFound << "commits";
    
    m_isSearching = false;
    m_searchLoadingMore = false;
    
    // 恢复鼠标状态
    QApplication::restoreOverrideCursor();
    
    // 更新最终状态
    updateSearchStatus();
    
    // 3秒后隐藏状态标签
    QTimer::singleShot(3000, this, [this]() {
        if (!m_isSearching) {
            m_searchStatusLabel->hide();
        }
    });
}

void GitLogDialog::updateSearchStatus()
{
    if (!m_isSearching) {
        if (m_searchTotalFound > 0) {
            m_searchStatusLabel->setText(tr("Search completed: %1 commits found").arg(m_searchTotalFound));
        } else if (!m_currentSearchText.isEmpty()) {
            // 搜索完成但没有找到结果
            m_searchStatusLabel->setText(tr("Search completed: No commits found for '%1'").arg(m_currentSearchText));
        } else {
            m_searchStatusLabel->hide();
        }
        return;
    }
    
    QString statusText;
    if (m_searchLoadingMore) {
        statusText = tr("Searching... (loading more commits, found %1 so far)").arg(m_searchTotalFound);
    } else {
        statusText = tr("Searching... (found %1 commits)").arg(m_searchTotalFound);
    }
    
    m_searchStatusLabel->setText(statusText);
}

void GitLogDialog::loadCommitDetails(const QString &commitHash)
{
    // 检查缓存
    if (m_commitDetailsCache.contains(commitHash)) {
        m_commitDetails->setPlainText(m_commitDetailsCache[commitHash]);
        return;
    }
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show"
         << "--format=fuller"
         << "--no-patch"
         << commitHash;
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_commitDetailsCache[commitHash] = output;
        m_commitDetails->setPlainText(output);
    } else {
        m_commitDetails->setPlainText(tr("Failed to load commit details"));
    }
}

void GitLogDialog::loadCommitFiles(const QString &commitHash)
{
    // 清空文件差异预览区域，因为要加载新的文件列表
    m_diffView->setPlainText(tr("Select a file to view changes..."));
    
    // 检查缓存
    if (m_commitFilesCache.contains(commitHash)) {
        populateFilesList(m_commitFilesCache[commitHash]);
        // 即使从缓存加载，也要加载统计信息（如果启用了的话）
        if (m_enableChangeStats) {
            loadFileChangeStats(commitHash);
        }
        return;
    }
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show"
         << "--name-status"
         << "--format="
         << commitHash;
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        m_commitFilesCache[commitHash] = lines;
        populateFilesList(lines);
        
        // 异步加载文件改动统计（如果启用了的话）
        if (m_enableChangeStats) {
            loadFileChangeStats(commitHash);
        }
    } else {
        m_changedFilesTree->clear();
    }
}

void GitLogDialog::populateFilesList(const QStringList &fileLines)
{
    m_changedFilesTree->clear();
    
    for (const QString &line : fileLines) {
        if (line.trimmed().isEmpty()) continue;
        
        QStringList parts = line.split('\t');
        if (parts.size() >= 2) {
            auto *item = new QTreeWidgetItem(m_changedFilesTree);
            
            QString status = parts[0];
            QString filePath = parts[1];
            
            // Status列
            item->setText(0, status);
            item->setIcon(0, getFileStatusIcon(status));
            
            // File列
            item->setText(1, QFileInfo(filePath).fileName());
            item->setData(1, Qt::UserRole, filePath);
            item->setToolTip(1, filePath);
            
            // Changes列 - 根据设置决定是否显示Loading
            if (m_enableChangeStats) {
                item->setText(2, tr("Loading..."));
                item->setData(2, Qt::UserRole, "loading");
            } else {
                item->setText(2, tr("Disabled"));
                item->setData(2, Qt::UserRole, "disabled");
                item->setForeground(2, QBrush(QColor(128, 128, 128)));
                item->setToolTip(2, tr("Change statistics disabled. Enable in Settings."));
            }
        }
    }
}

void GitLogDialog::loadFileChangeStats(const QString &commitHash)
{
    // 添加调试信息
    qDebug() << "[GitLogDialog] Starting loadFileChangeStats for commit:" << commitHash.left(8);
    qDebug() << "[GitLogDialog] Current file tree item count:" << m_changedFilesTree->topLevelItemCount();
    
    // 设置一个备用定时器，确保Loading状态最终会被清除
    QTimer::singleShot(8000, this, [this, commitHash]() {
        qWarning() << "WARNING: [GitLogDialog] Backup timer triggered for commit:" << commitHash.left(8);
        clearLoadingStats();
    });
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show"
         << "--numstat"
         << "--format="
         << commitHash;
    
    qDebug() << "[GitLogDialog] Loading file change stats with args:" << args;
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        if (process.exitCode() == 0) {
            QString output = QString::fromUtf8(process.readAllStandardOutput());
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);
            
            qDebug() << "[GitLogDialog] Git numstat output lines count:" << lines.size();
            for (const QString &line : lines) {
                qDebug() << "[GitLogDialog] numstat line:" << line;
            }
            
            // 检查是否有有效的统计数据
            if (lines.isEmpty()) {
                qWarning() << "WARNING: [GitLogDialog] No numstat data received for commit:" << commitHash.left(8);
                clearLoadingStats();
                return;
            }
            
            // 解析numstat输出并更新文件列表
            updateFileChangeStats(lines);
            
            // 同时更新commit汇总统计
            updateCommitSummaryStats(lines);
        } else {
            QString errorOutput = QString::fromUtf8(process.readAllStandardError());
            qWarning() << "WARNING: [GitLogDialog] Git command failed with exit code:" << process.exitCode()
                       << "Error:" << errorOutput;
            clearLoadingStats();
        }
    } else {
        qWarning() << "WARNING: [GitLogDialog] Failed to load file change stats:" << process.errorString();
        // 如果加载失败，清除"Loading..."文本
        clearLoadingStats();
    }
}

void GitLogDialog::updateFileChangeStats(const QStringList &statLines)
{
    qDebug() << "[GitLogDialog] Starting updateFileChangeStats with" << statLines.size() << "lines";
    
    // 创建文件路径到统计信息的映射
    QHash<QString, QPair<int, int>> fileStats; // filePath -> (additions, deletions)
    
    for (const QString &line : statLines) {
        if (line.trimmed().isEmpty()) continue;
        
        QStringList parts = line.split('\t');
        if (parts.size() >= 3) {
            QString additionsStr = parts[0];
            QString deletionsStr = parts[1];
            QString filePath = parts[2];
            
            // 处理二进制文件（显示为"-"）
            int additions = (additionsStr == "-") ? 0 : additionsStr.toInt();
            int deletions = (deletionsStr == "-") ? 0 : deletionsStr.toInt();
            
            fileStats[filePath] = qMakePair(additions, deletions);
            qDebug() << "[GitLogDialog] Parsed stats for" << filePath << ":" << additions << "additions," << deletions << "deletions";
        }
    }
    
    qDebug() << "[GitLogDialog] Parsed" << fileStats.size() << "file stats";
    
    // 更新树形控件中的统计信息
    int updatedCount = 0;
    int totalItems = m_changedFilesTree->topLevelItemCount();
    
    for (int i = 0; i < totalItems; ++i) {
        QTreeWidgetItem *item = m_changedFilesTree->topLevelItem(i);
        QString filePath = item->data(1, Qt::UserRole).toString();
        
        qDebug() << "[GitLogDialog] Processing item" << i << "with file path:" << filePath;
        
        if (fileStats.contains(filePath)) {
            QPair<int, int> stats = fileStats[filePath];
            int additions = stats.first;
            int deletions = stats.second;
            
            // 格式化显示统计信息，类似GitHub风格
            QString statsText = formatChangeStats(additions, deletions);
            item->setText(2, statsText);
            item->setData(2, Qt::UserRole, "completed"); // 标记为已完成
            
            // 设置颜色样式
            setChangeStatsColor(item, additions, deletions);
            
            updatedCount++;
            qDebug() << "[GitLogDialog] Updated stats for" << filePath << ":" << statsText;
        } else {
            // 没有找到统计信息，可能是重命名或其他特殊情况
            qWarning() << "WARNING: [GitLogDialog] No stats found for file:" << filePath;
            qDebug() << "[GitLogDialog] Available file paths in stats:";
            for (auto it = fileStats.begin(); it != fileStats.end(); ++it) {
                qDebug() << "  -" << it.key();
            }
            
            item->setText(2, "");
            item->setData(2, Qt::UserRole, "completed");
        }
    }
    
    qInfo() << QString("INFO: [GitLogDialog] Updated stats for %1 out of %2 files")
               .arg(updatedCount).arg(totalItems);
    
    // 确保清除任何剩余的Loading状态
    clearLoadingStats();
}

void GitLogDialog::updateCommitSummaryStats(const QStringList &statLines)
{
    int totalAdditions = 0;
    int totalDeletions = 0;
    int filesChanged = 0;
    
    for (const QString &line : statLines) {
        if (line.trimmed().isEmpty()) continue;
        
        QStringList parts = line.split('\t');
        if (parts.size() >= 3) {
            QString additionsStr = parts[0];
            QString deletionsStr = parts[1];
            
            // 处理二进制文件（显示为"-"）
            if (additionsStr != "-" && deletionsStr != "-") {
                totalAdditions += additionsStr.toInt();
                totalDeletions += deletionsStr.toInt();
            }
            filesChanged++;
        }
    }
    
    // 更新commit详情中的汇总统计
    QString currentCommitHash = getCurrentSelectedCommitHash();
    if (!currentCommitHash.isEmpty()) {
        // 获取当前的commit详情（纯文本）
        QString currentDetails = m_commitDetailsCache.value(currentCommitHash, "");
        
        // 创建汇总统计信息
        QString summaryStats = formatCommitSummaryStats(filesChanged, totalAdditions, totalDeletions);
        
        // 组合HTML内容
        QString htmlContent = summaryStats + 
                             "<hr style='border: 1px solid #ccc; margin: 10px 0;'>" +
                             "<pre style='font-family: Consolas, monospace; font-size: 9pt; margin: 0;'>" +
                             currentDetails.toHtmlEscaped() + "</pre>";
        
        m_commitDetails->setHtml(htmlContent);
        
        qInfo() << QString("INFO: [GitLogDialog] Commit summary: %1 files, +%2 -%3")
                   .arg(filesChanged).arg(totalAdditions).arg(totalDeletions);
    }
}

QString GitLogDialog::formatCommitSummaryStats(int filesChanged, int additions, int deletions) const
{
    QString result = "<div style='font-family: Arial, sans-serif; font-size: 10pt; margin-bottom: 8px;'>";
    result += "<b>📊 Commit Summary:</b><br>";
    result += QString("Files changed: <b>%1</b><br>").arg(filesChanged);
    
    if (additions > 0 || deletions > 0) {
        result += "Changes: ";
        if (additions > 0) {
            result += QString("<span style='color: #28a745; font-weight: bold;'>+%1</span>").arg(additions);
        }
        if (deletions > 0) {
            if (additions > 0) {
                result += " ";
            }
            result += QString("<span style='color: #dc3545; font-weight: bold;'>-%1</span>").arg(deletions);
        }
        result += "<br>";
    } else {
        result += "No line changes<br>";
    }
    
    result += "</div>";
    return result;
}

QString GitLogDialog::formatChangeStats(int additions, int deletions) const
{
    if (additions == 0 && deletions == 0) {
        return tr("No changes");
    }
    
    QString result;
    if (additions > 0) {
        result += QString("+%1").arg(additions);
    }
    if (deletions > 0) {
        if (!result.isEmpty()) {
            result += " ";
        }
        result += QString("-%1").arg(deletions);
    }
    
    return result;
}

void GitLogDialog::setChangeStatsColor(QTreeWidgetItem *item, int additions, int deletions) const
{
    if (!item) return;
    
    // 根据改动类型设置颜色
    QColor textColor;
    if (additions > 0 && deletions == 0) {
        // 只有新增：绿色
        textColor = QColor(0, 128, 0);
    } else if (additions == 0 && deletions > 0) {
        // 只有删除：红色
        textColor = QColor(128, 0, 0);
    } else if (additions > 0 && deletions > 0) {
        // 既有新增又有删除：橙色
        textColor = QColor(255, 140, 0);
    } else {
        // 无改动：灰色
        textColor = QColor(128, 128, 128);
    }
    
    item->setForeground(2, QBrush(textColor));
    
    // 设置工具提示
    QString tooltip;
    if (additions > 0 || deletions > 0) {
        tooltip = tr("Lines added: %1, Lines deleted: %2").arg(additions).arg(deletions);
    } else {
        tooltip = tr("No line changes");
    }
    item->setToolTip(2, tooltip);
}

void GitLogDialog::clearLoadingStats()
{
    int clearedCount = 0;
    int totalItems = m_changedFilesTree->topLevelItemCount();
    
    qDebug() << "[GitLogDialog] clearLoadingStats: checking" << totalItems << "items";
    
    for (int i = 0; i < totalItems; ++i) {
        QTreeWidgetItem *item = m_changedFilesTree->topLevelItem(i);
        QString status = item->data(2, Qt::UserRole).toString();
        QString text = item->text(2);
        
        qDebug() << "[GitLogDialog] Item" << i << "- status:" << status << "text:" << text;
        
        if (status == "loading" || text == tr("Loading...")) {
            item->setText(2, "");
            item->setData(2, Qt::UserRole, "cleared");
            clearedCount++;
            qDebug() << "[GitLogDialog] Cleared loading status for item" << i;
        }
    }
    
    qInfo() << QString("INFO: [GitLogDialog] Cleared loading status for %1 out of %2 files")
               .arg(clearedCount).arg(totalItems);
}

void GitLogDialog::loadFileDiff(const QString &commitHash, const QString &filePath)
{
    QString cacheKey = QString("%1:%2").arg(commitHash, filePath);
    
    // 检查缓存
    if (m_fileDiffCache.contains(cacheKey)) {
        m_diffView->setPlainText(m_fileDiffCache[cacheKey]);
        return;
    }
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show"
         << commitHash
         << "--"
         << filePath;
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_fileDiffCache[cacheKey] = output;
        m_diffView->setPlainText(output);
    } else {
        m_diffView->setPlainText(tr("Failed to load file diff"));
    }
}

QIcon GitLogDialog::getFileStatusIcon(const QString &status) const
{
    if (status == "A") return QIcon::fromTheme("list-add");
    if (status == "M") return QIcon::fromTheme("document-edit");
    if (status == "D") return QIcon::fromTheme("list-remove");
    if (status == "R") return QIcon::fromTheme("edit-rename");
    if (status == "C") return QIcon::fromTheme("edit-copy");
    return QIcon::fromTheme("document-properties");
}

QString GitLogDialog::getFileStatusText(const QString &status) const
{
    if (status == "A") return tr("Added");
    if (status == "M") return tr("Modified");
    if (status == "D") return tr("Deleted");
    if (status == "R") return tr("Renamed");
    if (status == "C") return tr("Copied");
    return tr("Unknown");
}

// === GitDiffSyntaxHighlighter实现 ===

GitDiffSyntaxHighlighter::GitDiffSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // 设置格式
    m_addedLineFormat.setForeground(QColor(0, 128, 0));
    m_addedLineFormat.setBackground(QColor(230, 255, 230));
    
    m_removedLineFormat.setForeground(QColor(128, 0, 0));
    m_removedLineFormat.setBackground(QColor(255, 230, 230));
    
    m_lineNumberFormat.setForeground(QColor(0, 0, 128));
    m_lineNumberFormat.setFontWeight(QFont::Bold);
    
    m_filePathFormat.setForeground(QColor(128, 0, 128));
    m_filePathFormat.setFontWeight(QFont::Bold);
    
    m_contextFormat.setForeground(QColor(64, 64, 64));
}

void GitDiffSyntaxHighlighter::highlightBlock(const QString &text)
{
    if (text.startsWith('+') && !text.startsWith("+++")) {
        setFormat(0, text.length(), m_addedLineFormat);
    } else if (text.startsWith('-') && !text.startsWith("---")) {
        setFormat(0, text.length(), m_removedLineFormat);
    } else if (text.startsWith("@@")) {
        setFormat(0, text.length(), m_lineNumberFormat);
    } else if (text.startsWith("+++") || text.startsWith("---")) {
        setFormat(0, text.length(), m_filePathFormat);
    } else if (text.startsWith(" ")) {
        setFormat(0, text.length(), m_contextFormat);
    }
}

void GitLogDialog::setupContextMenus()
{
    setupCommitContextMenu();
    setupFileContextMenu();
}

void GitLogDialog::setupCommitContextMenu()
{
    m_commitContextMenu = new QMenu(this);
    
    // === 基础操作 ===
    m_checkoutCommitAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("vcs-normal"), tr("Checkout Commit"));
    m_createBranchAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("vcs-branch"), tr("Create Branch Here"));
    m_createTagAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("vcs-tag"), tr("Create Tag Here"));
    
    m_commitContextMenu->addSeparator();
    
    // === Reset操作子菜单 ===
    m_resetMenu = m_commitContextMenu->addMenu(
        QIcon::fromTheme("edit-undo"), tr("Reset to Here"));
    m_softResetAction = m_resetMenu->addAction(tr("Soft Reset"));
    m_mixedResetAction = m_resetMenu->addAction(tr("Mixed Reset"));
    m_hardResetAction = m_resetMenu->addAction(tr("Hard Reset"));
    
    // 设置工具提示
    m_softResetAction->setToolTip(tr("Keep working directory and staging area"));
    m_mixedResetAction->setToolTip(tr("Keep working directory, reset staging area"));
    m_hardResetAction->setToolTip(tr("Reset working directory and staging area"));
    
    // === 其他操作 ===
    m_revertCommitAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("edit-undo"), tr("Revert Commit"));
    m_cherryPickAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("vcs-merge"), tr("Cherry-pick Commit"));
    
    m_commitContextMenu->addSeparator();
    
    // === 查看操作 ===
    m_showDetailsAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("document-properties"), tr("Show Commit Details"));
    m_compareWorkingTreeAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("document-compare"), tr("Compare with Working Tree"));
    
    m_commitContextMenu->addSeparator();
    
    // === 复制操作 ===
    m_copyHashAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("edit-copy"), tr("Copy Commit Hash"));
    m_copyShortHashAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("edit-copy"), tr("Copy Short Hash"));
    m_copyMessageAction = m_commitContextMenu->addAction(
        QIcon::fromTheme("edit-copy"), tr("Copy Commit Message"));
    
    // === 连接信号 ===
    connect(m_checkoutCommitAction, &QAction::triggered, this, &GitLogDialog::checkoutCommit);
    connect(m_createBranchAction, &QAction::triggered, this, &GitLogDialog::createBranchFromCommit);
    connect(m_createTagAction, &QAction::triggered, this, &GitLogDialog::createTagFromCommit);
    connect(m_softResetAction, &QAction::triggered, this, &GitLogDialog::softResetToCommit);
    connect(m_mixedResetAction, &QAction::triggered, this, &GitLogDialog::mixedResetToCommit);
    connect(m_hardResetAction, &QAction::triggered, this, &GitLogDialog::hardResetToCommit);
    connect(m_revertCommitAction, &QAction::triggered, this, &GitLogDialog::revertCommit);
    connect(m_cherryPickAction, &QAction::triggered, this, &GitLogDialog::cherryPickCommit);
    connect(m_showDetailsAction, &QAction::triggered, this, &GitLogDialog::showCommitDetails);
    connect(m_compareWorkingTreeAction, &QAction::triggered, this, &GitLogDialog::compareWithWorkingTree);
    connect(m_copyHashAction, &QAction::triggered, this, &GitLogDialog::copyCommitHash);
    connect(m_copyShortHashAction, &QAction::triggered, this, &GitLogDialog::copyShortHash);
    connect(m_copyMessageAction, &QAction::triggered, this, &GitLogDialog::copyCommitMessage);
}

void GitLogDialog::setupFileContextMenu()
{
    m_fileContextMenu = new QMenu(this);
    
    // === 文件查看操作 ===
    m_viewFileAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("document-open"), tr("View File at This Commit"));
    m_showFileDiffAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("document-properties"), tr("Show File Diff"));
    m_showFileHistoryAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("view-list-details"), tr("Show File History"));
    m_showFileBlameAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("view-list-tree"), tr("Show File Blame"));
    
    m_fileContextMenu->addSeparator();
    
    // === 文件管理操作 ===
    m_openFileAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("document-open"), tr("Open File"));
    m_showFolderAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("folder-open"), tr("Show in Folder"));
    
    m_fileContextMenu->addSeparator();
    
    // === 复制操作 ===
    m_copyFilePathAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("edit-copy"), tr("Copy File Path"));
    m_copyFileNameAction = m_fileContextMenu->addAction(
        QIcon::fromTheme("edit-copy"), tr("Copy File Name"));
    
    // === 连接信号 ===
    connect(m_viewFileAction, &QAction::triggered, this, &GitLogDialog::viewFileAtCommit);
    connect(m_showFileDiffAction, &QAction::triggered, this, &GitLogDialog::showFileDiff);
    connect(m_showFileHistoryAction, &QAction::triggered, this, &GitLogDialog::showFileHistory);
    connect(m_showFileBlameAction, &QAction::triggered, this, &GitLogDialog::showFileBlame);
    connect(m_openFileAction, &QAction::triggered, this, &GitLogDialog::openFile);
    connect(m_showFolderAction, &QAction::triggered, this, &GitLogDialog::showInFolder);
    connect(m_copyFilePathAction, &QAction::triggered, this, &GitLogDialog::copyFilePath);
    connect(m_copyFileNameAction, &QAction::triggered, this, &GitLogDialog::copyFileName);
}

// === 右键菜单显示 ===

void GitLogDialog::showCommitContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_commitTree->itemAt(pos);
    if (!item) {
        return;
    }
    
    // 确保选中了正确的项目
    m_commitTree->setCurrentItem(item);
    
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) {
        return;
    }
    
    // 更新菜单项文本显示commit信息
    QString shortHash = commitHash.left(8);
    m_checkoutCommitAction->setText(tr("Checkout Commit (%1)").arg(shortHash));
    m_createBranchAction->setText(tr("Create Branch from %1").arg(shortHash));
    m_createTagAction->setText(tr("Create Tag at %1").arg(shortHash));
    m_revertCommitAction->setText(tr("Revert %1").arg(shortHash));
    m_cherryPickAction->setText(tr("Cherry-pick %1").arg(shortHash));
    
    m_commitContextMenu->exec(m_commitTree->mapToGlobal(pos));
}

void GitLogDialog::showFileContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_changedFilesTree->itemAt(pos);
    if (!item) {
        return;
    }
    
    // 确保选中了正确的项目
    m_changedFilesTree->setCurrentItem(item);
    
    QString filePath = getCurrentSelectedFilePath();
    if (filePath.isEmpty()) {
        return;
    }
    
    // 更新菜单项文本显示文件信息
    QString fileName = QFileInfo(filePath).fileName();
    m_viewFileAction->setText(tr("View %1 at This Commit").arg(fileName));
    m_showFileDiffAction->setText(tr("Show Diff for %1").arg(fileName));
    m_showFileHistoryAction->setText(tr("Show History of %1").arg(fileName));
    m_showFileBlameAction->setText(tr("Show Blame for %1").arg(fileName));
    
    m_fileContextMenu->exec(m_changedFilesTree->mapToGlobal(pos));
}

// === Commit操作实现 ===

void GitLogDialog::checkoutCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    int ret = QMessageBox::warning(this, tr("Checkout Commit"),
        tr("This will checkout commit %1 and put you in 'detached HEAD' state.\n\n"
           "Do you want to continue?").arg(commitHash.left(8)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        executeGitOperation(tr("Checkout Commit"), {"checkout", commitHash});
    }
}

void GitLogDialog::createBranchFromCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    bool ok;
    QString branchName = QInputDialog::getText(this, tr("Create Branch"),
        tr("Enter branch name:"), QLineEdit::Normal, "", &ok);
    
    if (ok && !branchName.isEmpty()) {
        executeGitOperation(tr("Create Branch"), {"checkout", "-b", branchName, commitHash});
    }
}

void GitLogDialog::createTagFromCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    bool ok;
    QString tagName = QInputDialog::getText(this, tr("Create Tag"),
        tr("Enter tag name:"), QLineEdit::Normal, "", &ok);
    
    if (ok && !tagName.isEmpty()) {
        executeGitOperation(tr("Create Tag"), {"tag", tagName, commitHash});
    }
}

void GitLogDialog::resetToCommit()
{
    // 这个方法由子菜单的具体reset方法调用
}

void GitLogDialog::softResetToCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    executeGitOperation(tr("Soft Reset"), {"reset", "--soft", commitHash}, true);
}

void GitLogDialog::mixedResetToCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    executeGitOperation(tr("Mixed Reset"), {"reset", "--mixed", commitHash}, true);
}

void GitLogDialog::hardResetToCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    int ret = QMessageBox::warning(this, tr("Hard Reset"),
        tr("This will permanently discard all local changes and reset to commit %1.\n\n"
           "This action cannot be undone. Are you sure?").arg(commitHash.left(8)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        executeGitOperation(tr("Hard Reset"), {"reset", "--hard", commitHash});
    }
}

void GitLogDialog::revertCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    executeGitOperation(tr("Revert Commit"), {"revert", "--no-edit", commitHash});
}

void GitLogDialog::cherryPickCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    executeGitOperation(tr("Cherry-pick Commit"), {"cherry-pick", commitHash});
}

void GitLogDialog::showCommitDetails()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    // 使用GitDialogManager显示详细的commit对话框
    // 这里可以创建一个专门的commit详情对话框，或者复用现有的
    QMessageBox::information(this, tr("Commit Details"),
        tr("Detailed commit dialog will be implemented in future version.\n"
           "Current commit: %1").arg(commitHash));
}

void GitLogDialog::compareWithWorkingTree()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) return;
    
    // 使用GitDialogManager显示diff对话框
    GitDialogManager::instance()->showDiffDialog(m_repositoryPath, "", this);
}

void GitLogDialog::copyCommitHash()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (!commitHash.isEmpty()) {
        QApplication::clipboard()->setText(commitHash);
        qDebug() << "[GitLogDialog] Copied full commit hash to clipboard:" << commitHash;
    }
}

void GitLogDialog::copyShortHash()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (!commitHash.isEmpty()) {
        QString shortHash = commitHash.left(8);
        QApplication::clipboard()->setText(shortHash);
        qDebug() << "[GitLogDialog] Copied short commit hash to clipboard:" << shortHash;
    }
}

void GitLogDialog::copyCommitMessage()
{
    QString commitHash = getCurrentSelectedCommitHash();
    if (commitHash.isEmpty()) {
        return;
    }
    
    // 获取完整的commit message
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "log" << "--format=%B" << "-n" << "1" << commitHash;
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        QString fullMessage = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (!fullMessage.isEmpty()) {
            QApplication::clipboard()->setText(fullMessage);
            qDebug() << "[GitLogDialog] Copied full commit message to clipboard:" << fullMessage.left(50) + "...";
        } else {
            // 备用方案：使用显示的第一行消息
            auto selectedItems = m_commitTree->selectedItems();
            if (!selectedItems.isEmpty()) {
                QString message = selectedItems.first()->text(1);
                QApplication::clipboard()->setText(message);
                qDebug() << "[GitLogDialog] Copied commit message (fallback) to clipboard:" << message;
            }
        }
    } else {
        // 备用方案：使用显示的第一行消息
        auto selectedItems = m_commitTree->selectedItems();
        if (!selectedItems.isEmpty()) {
            QString message = selectedItems.first()->text(1);
            QApplication::clipboard()->setText(message);
            qDebug() << "[GitLogDialog] Copied commit message (fallback) to clipboard:" << message;
        }
    }
}

// === 文件操作实现 ===

void GitLogDialog::viewFileAtCommit()
{
    QString commitHash = getCurrentSelectedCommitHash();
    QString filePath = getCurrentSelectedFilePath();
    if (commitHash.isEmpty() || filePath.isEmpty()) return;
    
    // 使用新的GitFilePreviewDialog
    GitDialogManager::instance()->showFilePreviewAtCommit(m_repositoryPath, filePath, commitHash, this);
    
    qInfo() << QString("INFO: [GitLogDialog] Opened file preview for %1 at commit %2")
               .arg(filePath, commitHash.left(8));
}

void GitLogDialog::showFileDiff()
{
    QString commitHash = getCurrentSelectedCommitHash();
    QString filePath = getCurrentSelectedFilePath();
    if (commitHash.isEmpty() || filePath.isEmpty()) {
        qWarning() << "WARNING: [GitLogDialog] Cannot show file diff: missing commit hash or file path";
        return;
    }
    
    // 创建专门的文件差异查看对话框
    QDialog *diffDialog = new QDialog(this);
    diffDialog->setWindowTitle(tr("File Diff - %1 at %2")
        .arg(QFileInfo(filePath).fileName(), commitHash.left(8)));
    diffDialog->resize(1000, 700);
    diffDialog->setAttribute(Qt::WA_DeleteOnClose);
    
    auto *layout = new QVBoxLayout(diffDialog);
    
    // 添加文件信息标签
    auto *infoLabel = new QLabel(tr("File: %1\nCommit: %2").arg(filePath, commitHash));
    infoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border: 1px solid #ccc; }");
    layout->addWidget(infoLabel);
    
    // 创建差异显示区域
    auto *diffTextEdit = new LineNumberTextEdit;
    diffTextEdit->setReadOnly(true);
    diffTextEdit->setFont(QFont("Consolas", 9));
    diffTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(diffTextEdit);
    
    // 添加语法高亮
    auto *highlighter = new GitDiffSyntaxHighlighter(diffTextEdit->document());
    
    // 获取文件差异
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show" << commitHash << "--" << filePath;
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        QString diffContent = QString::fromUtf8(process.readAllStandardOutput());
        if (diffContent.isEmpty()) {
            diffContent = tr("No differences found for this file in this commit.");
        }
        diffTextEdit->setPlainText(diffContent);
    } else {
        diffTextEdit->setPlainText(tr("Failed to load file diff: %1").arg(process.errorString()));
    }
    
    // 添加按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    
    auto *viewFileButton = new QPushButton(tr("View File at Commit"));
    connect(viewFileButton, &QPushButton::clicked, this, &GitLogDialog::viewFileAtCommit);
    buttonLayout->addWidget(viewFileButton);
    
    auto *closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, diffDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    
    layout->addLayout(buttonLayout);
    
    diffDialog->show();
    
    qInfo() << QString("INFO: [GitLogDialog] Showing file diff for %1 at commit %2")
               .arg(filePath, commitHash.left(8));
}

void GitLogDialog::showFileHistory()
{
    QString filePath = getCurrentSelectedFilePath();
    if (filePath.isEmpty()) return;
    
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePath);
    GitDialogManager::instance()->showLogDialog(m_repositoryPath, absolutePath, this);
}

void GitLogDialog::showFileBlame()
{
    QString filePath = getCurrentSelectedFilePath();
    if (filePath.isEmpty()) return;
    
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePath);
    GitDialogManager::instance()->showBlameDialog(m_repositoryPath, absolutePath, this);
}

void GitLogDialog::openFile()
{
    QString filePath = getCurrentSelectedFilePath();
    if (filePath.isEmpty()) return;
    
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePath);
    GitDialogManager::instance()->openFile(absolutePath, this);
}

void GitLogDialog::showInFolder()
{
    QString filePath = getCurrentSelectedFilePath();
    if (filePath.isEmpty()) return;
    
    QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePath);
    GitDialogManager::instance()->showFileInFolder(absolutePath, this);
}

void GitLogDialog::copyFilePath()
{
    QString filePath = getCurrentSelectedFilePath();
    if (!filePath.isEmpty()) {
        QString absolutePath = QDir(m_repositoryPath).absoluteFilePath(filePath);
        QApplication::clipboard()->setText(absolutePath);
        qDebug() << "[GitLogDialog] Copied file path to clipboard:" << absolutePath;
    }
}

void GitLogDialog::copyFileName()
{
    QString filePath = getCurrentSelectedFilePath();
    if (!filePath.isEmpty()) {
        QString fileName = QFileInfo(filePath).fileName();
        QApplication::clipboard()->setText(fileName);
        qDebug() << "[GitLogDialog] Copied file name to clipboard:" << fileName;
    }
}

// === 辅助方法 ===

void GitLogDialog::executeGitOperation(const QString &operation, const QStringList &args, bool needsConfirmation)
{
    if (needsConfirmation) {
        int ret = QMessageBox::question(this, tr("Confirm Operation"),
            tr("Are you sure you want to perform: %1?").arg(operation),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        
        if (ret != QMessageBox::Yes) {
            return;
        }
    }
    
    // 使用GitOperationDialog执行Git命令
    auto *dialog = new GitOperationDialog(operation, this);
    dialog->executeCommand(m_repositoryPath, args);
    dialog->show();
    
    // 操作完成后刷新界面
    connect(dialog, &GitOperationDialog::finished, this, &GitLogDialog::refreshAfterOperation);
    
    qInfo() << QString("INFO: [GitLogDialog] Executing Git operation: %1 with args: %2")
               .arg(operation, args.join(" "));
}

void GitLogDialog::refreshAfterOperation()
{
    // 延迟刷新，确保Git操作已完成
    QTimer::singleShot(500, this, [this]() {
        onRefreshClicked();
    });
}

void GitLogDialog::checkIfNeedMoreCommits()
{
    // 检查是否有滚动条，如果没有且还有更多commit可以加载，则自动加载
    int maximum = m_commitScrollBar->maximum();
    if (maximum == 0 && m_commitTree->topLevelItemCount() == COMMITS_PER_LOAD) {
        qInfo() << "INFO: [GitLogDialog] No scrollbar detected, loading more commits automatically";
        loadMoreCommitsIfNeeded();
    }
}

void GitLogDialog::keyPressEvent(QKeyEvent *event)
{
    // 空格键预览文件
    if (event->key() == Qt::Key_Space) {
        QString filePath = getCurrentSelectedFilePath();
        if (!filePath.isEmpty()) {
            if (m_currentPreviewDialog) {
                // 如果已经有预览对话框打开，关闭它
                m_currentPreviewDialog->close();
                m_currentPreviewDialog = nullptr;
            } else {
                // 打开新的预览对话框
                previewSelectedFile();
            }
        }
        event->accept(); // 标记事件已处理
        return;
    }
    
    QDialog::keyPressEvent(event);
}

bool GitLogDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 捕获文件列表的键盘事件
    if (watched == m_changedFilesTree && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        
        // 空格键预览文件
        if (keyEvent->key() == Qt::Key_Space) {
            QString filePath = getCurrentSelectedFilePath();
            if (!filePath.isEmpty()) {
                if (m_currentPreviewDialog) {
                    // 如果已经有预览对话框打开，关闭它
                    m_currentPreviewDialog->close();
                    m_currentPreviewDialog = nullptr;
                } else {
                    // 打开新的预览对话框
                    previewSelectedFile();
                }
                return true; // 事件已处理，不再传播
            }
        }
    }
    
    return QDialog::eventFilter(watched, event);
}

void GitLogDialog::previewSelectedFile()
{
    QString commitHash = getCurrentSelectedCommitHash();
    QString filePath = getCurrentSelectedFilePath();
    
    if (commitHash.isEmpty() || filePath.isEmpty()) {
        QMessageBox::information(this, tr("No File Selected"), 
                                 tr("Please select a file to preview."));
        return;
    }
    
    // 关闭之前的预览对话框
    if (m_currentPreviewDialog) {
        m_currentPreviewDialog->close();
        m_currentPreviewDialog = nullptr;
    }
    
    // 创建新的预览对话框（commit模式）
    m_currentPreviewDialog = GitDialogManager::instance()->showFilePreviewAtCommit(
        m_repositoryPath, filePath, commitHash, this);
    
    // 连接对话框关闭信号，以便清理引用
    connect(m_currentPreviewDialog, &QDialog::finished, this, [this]() {
        m_currentPreviewDialog = nullptr;
    });
    
    qInfo() << "INFO: [GitLogDialog] Opened file preview for:" << filePath 
            << "at commit:" << commitHash.left(8);
}

void GitLogDialog::onSettingsClicked()
{
    QMenu settingsMenu(this);
    
    // 改动统计选项
    QAction *changeStatsAction = settingsMenu.addAction(tr("Enable Change Statistics"));
    changeStatsAction->setCheckable(true);
    changeStatsAction->setChecked(m_enableChangeStats);
    changeStatsAction->setToolTip(tr("Show/hide file change statistics (+/-) in the file list"));
    
    connect(changeStatsAction, &QAction::triggered, this, [this](bool enabled) {
        m_enableChangeStats = enabled;
        qInfo() << "INFO: [GitLogDialog] Change statistics" << (enabled ? "enabled" : "disabled");
        
        // 如果启用了统计，重新加载当前commit的统计信息
        if (enabled) {
            QString currentCommit = getCurrentSelectedCommitHash();
            if (!currentCommit.isEmpty()) {
                // 重新填充文件列表以显示Loading状态
                if (m_commitFilesCache.contains(currentCommit)) {
                    populateFilesList(m_commitFilesCache[currentCommit]);
                    loadFileChangeStats(currentCommit);
                }
            }
        } else {
            // 如果禁用了统计，更新当前显示
            QString currentCommit = getCurrentSelectedCommitHash();
            if (!currentCommit.isEmpty() && m_commitFilesCache.contains(currentCommit)) {
                populateFilesList(m_commitFilesCache[currentCommit]);
            }
        }
    });
    
    settingsMenu.addSeparator();
    
    // 其他设置选项可以在这里添加
    QAction *aboutAction = settingsMenu.addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::information(this, tr("About Git Log Dialog"),
            tr("Git Log Dialog with GitHub-style change statistics\n\n"
               "Features:\n"
               "• File change statistics (+/-)\n"
               "• Commit summary statistics\n"
               "• Right-click context menus\n"
               "• Space key file preview\n"
               "• Infinite scroll loading\n"
               "• Search and filtering"));
    });
    
    settingsMenu.exec(m_settingsButton->mapToGlobal(QPoint(0, m_settingsButton->height())));
} 