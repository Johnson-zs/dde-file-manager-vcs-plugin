#include "gitlogdialog.h"
#include "gitoperationdialog.h"
#include "gitdialogs.h"

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
      m_searchTimer(nullptr)
{
    qInfo() << "INFO: [GitLogDialog] Initializing GitKraken-style log dialog for repository:" << repositoryPath;
    
    setupUI();
    setupContextMenus();
    setupInfiniteScroll();
    
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
    m_diffView = new QTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 9));
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
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
            this, [this]() { filterCommits(m_currentSearchText); });
    
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
        
        // 解析git log输出
        QStringList parts = line.split('|');
        if (parts.size() >= 5) {
            auto *item = new QTreeWidgetItem(m_commitTree);
            
            // Graph列 - 简化的图形显示
            QString graphPart = parts[0];
            item->setText(0, "●"); // 简单的圆点表示commit
            
            // Message列
            item->setText(1, parts[1].trimmed());
            
            // Author列
            item->setText(2, parts[2].trimmed());
            
            // Date列
            item->setText(3, parts[3].trimmed());
            
            // Hash列 - 显示短哈希
            QString fullHash = parts[4].trimmed();
            item->setText(4, fullHash.left(8));
            item->setData(4, Qt::UserRole, fullHash); // 存储完整哈希
            
            // 设置工具提示
            item->setToolTip(1, parts[1].trimmed());
            item->setToolTip(4, fullHash);
            
            loadedCount++;
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
    m_searchTimer->start(); // 延迟搜索
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
        // 显示所有项目
        for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
            m_commitTree->topLevelItem(i)->setHidden(false);
        }
        return;
    }
    
    // 过滤项目
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
    }
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
            
            // Changes列 - 稍后通过diff统计获取
            item->setText(2, "");
        }
    }
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
    
    // 创建临时文件查看器
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show" << QString("%1:%2").arg(commitHash, filePath);
    
    process.start("git", args);
    if (process.waitForFinished(5000)) {
        QString content = QString::fromUtf8(process.readAllStandardOutput());
        
        // 创建简单的文件查看对话框
        QDialog *viewDialog = new QDialog(this);
        viewDialog->setWindowTitle(tr("View File - %1 at %2")
            .arg(QFileInfo(filePath).fileName(), commitHash.left(8)));
        viewDialog->resize(800, 600);
        viewDialog->setAttribute(Qt::WA_DeleteOnClose);
        
        auto *layout = new QVBoxLayout(viewDialog);
        auto *textEdit = new QTextEdit;
        textEdit->setReadOnly(true);
        textEdit->setFont(QFont("Consolas", 9));
        textEdit->setPlainText(content);
        layout->addWidget(textEdit);
        
        auto *buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        auto *closeButton = new QPushButton(tr("Close"));
        connect(closeButton, &QPushButton::clicked, viewDialog, &QDialog::accept);
        buttonLayout->addWidget(closeButton);
        layout->addLayout(buttonLayout);
        
        viewDialog->show();
    } else {
        QMessageBox::warning(this, tr("Error"), 
            tr("Failed to load file content: %1").arg(process.errorString()));
    }
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
    auto *diffTextEdit = new QTextEdit;
    diffTextEdit->setReadOnly(true);
    diffTextEdit->setFont(QFont("Consolas", 9));
    diffTextEdit->setLineWrapMode(QTextEdit::NoWrap);
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