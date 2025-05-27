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

#include "utils.h"

// ============================================================================
// GitLogDialog Implementation
// ============================================================================

GitLogDialog::GitLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_filePath(filePath), m_mainSplitter(nullptr), m_rightSplitter(nullptr), m_commitTree(nullptr), m_branchCombo(nullptr), m_searchEdit(nullptr), m_refreshButton(nullptr), m_loadMoreButton(nullptr), m_commitDetails(nullptr), m_diffView(nullptr), m_currentOffset(0)
{
    setupUI();
    loadBranches();
    loadCommitHistory();
}

void GitLogDialog::setupUI()
{
    setWindowTitle(m_filePath.isEmpty() ? tr("Git Log - Repository") : tr("Git Log - %1").arg(QFileInfo(m_filePath).fileName()));
    setModal(false);
    resize(1200, 800);

    auto *mainLayout = new QVBoxLayout(this);

    // 顶部工具栏
    auto *toolbarLayout = new QHBoxLayout;

    toolbarLayout->addWidget(new QLabel(tr("Branch:")));
    m_branchCombo = new QComboBox;
    m_branchCombo->setMinimumWidth(150);
    toolbarLayout->addWidget(m_branchCombo);

    toolbarLayout->addWidget(new QLabel(tr("Search:")));
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Search commits..."));
    toolbarLayout->addWidget(m_searchEdit);

    m_refreshButton = new QPushButton(tr("Refresh"));
    toolbarLayout->addWidget(m_refreshButton);

    m_loadMoreButton = new QPushButton(tr("Load More"));
    toolbarLayout->addWidget(m_loadMoreButton);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧：提交列表
    setupCommitList();
    m_mainSplitter->addWidget(m_commitTree);

    // 右侧分割器
    m_rightSplitter = new QSplitter(Qt::Vertical);

    // 右上：提交详情
    setupCommitDetails();
    m_rightSplitter->addWidget(m_commitDetails);

    // 右下：差异视图
    m_diffView = new QTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 10));
    m_diffView->setPlainText(tr("Select a commit to view changes..."));
    m_rightSplitter->addWidget(m_diffView);

    m_rightSplitter->setSizes({ 300, 400 });
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setSizes({ 400, 800 });

    mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_commitTree, &QTreeWidget::itemSelectionChanged, this, &GitLogDialog::onCommitSelectionChanged);
    connect(m_refreshButton, &QPushButton::clicked, this, &GitLogDialog::onRefreshClicked);
    connect(m_branchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GitLogDialog::onBranchChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &GitLogDialog::onSearchTextChanged);
    connect(m_loadMoreButton, &QPushButton::clicked, this, &GitLogDialog::onLoadMoreClicked);
}

void GitLogDialog::setupCommitList()
{
    m_commitTree = new QTreeWidget;
    QStringList headers;
    headers << tr("Graph") << tr("Subject") << tr("Author") << tr("Date") << tr("Hash");
    m_commitTree->setHeaderLabels(headers);
    m_commitTree->setRootIsDecorated(false);
    m_commitTree->setAlternatingRowColors(true);
    m_commitTree->setSortingEnabled(false);

    // 设置列宽
    m_commitTree->setColumnWidth(0, 60);   // Graph
    m_commitTree->setColumnWidth(1, 300);   // Subject
    m_commitTree->setColumnWidth(2, 120);   // Author
    m_commitTree->setColumnWidth(3, 120);   // Date
    m_commitTree->setColumnWidth(4, 80);   // Hash
}

void GitLogDialog::setupCommitDetails()
{
    m_commitDetails = new QTextEdit;
    m_commitDetails->setReadOnly(true);
    m_commitDetails->setMaximumHeight(200);
    m_commitDetails->setPlainText(tr("Select a commit to view details..."));
}

void GitLogDialog::loadBranches()
{
    m_branchCombo->clear();

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "branch", "-a" });

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList branches = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &branch : branches) {
            QString cleanBranch = branch.trimmed();
            if (cleanBranch.startsWith('*')) {
                cleanBranch = cleanBranch.mid(2);
                m_branchCombo->addItem(cleanBranch);
                m_branchCombo->setCurrentText(cleanBranch);
            } else {
                m_branchCombo->addItem(cleanBranch);
            }
        }
    }
}

void GitLogDialog::loadCommitHistory(bool append)
{
    if (!append) {
        m_commitTree->clear();
        m_currentOffset = 0;
    }

    // 使用 --skip 和 --max-count 实现分页
    QStringList args { "log", "--graph", "--decorate", "--all",
                       "--format=%H|%h|%s|%an|%ae|%ad|%ar", "--date=short",
                       "--max-count=" + QString::number(COMMITS_PER_PAGE),
                       "--skip=" + QString::number(m_currentOffset) };

    // 如果指定了文件，添加文件路径过滤
    if (!m_filePath.isEmpty()) {
        // 计算相对于仓库根目录的路径
        QString relativePath = QDir(m_repositoryPath).relativeFilePath(m_filePath);
        args << "--" << relativePath;
    }

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", args);

    if (process.waitForFinished(10000)) {   // 增加超时时间到10秒
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QString errorOutput = QString::fromUtf8(process.readAllStandardError());

        if (!errorOutput.isEmpty()) {
            qWarning() << "WARNING: [GitLogDialog::loadCommitHistory] Git log error:" << errorOutput;
        }

        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        int loadedCount = 0;

        qInfo() << "INFO: [GitLogDialog::loadCommitHistory] Processing" << lines.size() << "lines, offset:" << m_currentOffset;

        for (const QString &line : lines) {
            // 跳过图形字符行，只处理包含提交信息的行
            if (!line.contains('|')) {
                continue;
            }

            // 提取图形部分和提交信息部分
            QString commitInfo = line;
            QString graphPart = "●";   // 默认图形

            // 查找第一个字母数字字符的位置（提交哈希开始）
            int hashStart = -1;
            for (int i = 0; i < line.length(); ++i) {
                if (line[i].isLetterOrNumber()) {
                    hashStart = i;
                    break;
                }
            }

            if (hashStart > 0) {
                graphPart = line.left(hashStart);
                commitInfo = line.mid(hashStart);
            }

            QStringList parts = commitInfo.split('|');
            if (parts.size() >= 6) {
                auto *item = new QTreeWidgetItem(m_commitTree);
                item->setText(0, graphPart);   // Graph
                item->setText(1, parts[2]);   // Subject
                item->setText(2, parts[3]);   // Author
                item->setText(3, parts[5]);   // Date
                item->setText(4, parts[1]);   // Short hash
                item->setData(0, Qt::UserRole, parts[0]);   // Full hash

                // 设置工具提示
                item->setToolTip(1, QString("Full hash: %1\nAuthor: %2 <%3>\nDate: %4").arg(parts[0], parts[3], parts[4], parts[5]));
                loadedCount++;
            }
        }

        // 更新偏移量
        m_currentOffset += loadedCount;

        // 如果加载的提交数量少于每页数量，说明已经到底了
        if (loadedCount < COMMITS_PER_PAGE) {
            m_loadMoreButton->setEnabled(false);
            m_loadMoreButton->setText(tr("No More Commits"));
        } else {
            m_loadMoreButton->setEnabled(true);
            m_loadMoreButton->setText(tr("Load More (%1 loaded)").arg(m_currentOffset));
        }

        // 如果没有加载到提交记录且不是追加模式，显示提示信息
        if (m_commitTree->topLevelItemCount() == 0 && !append) {
            auto *item = new QTreeWidgetItem(m_commitTree);
            item->setText(1, tr("No commits found"));
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
            m_loadMoreButton->setEnabled(false);
        }

        qInfo() << "INFO: [GitLogDialog::loadCommitHistory] Loaded" << loadedCount
                << "commits, total:" << m_commitTree->topLevelItemCount();

    } else {
        qWarning() << "ERROR: [GitLogDialog::loadCommitHistory] Git log process failed or timed out";
        if (!append) {
            auto *item = new QTreeWidgetItem(m_commitTree);
            item->setText(1, tr("Failed to load commit history"));
            item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        }
        m_loadMoreButton->setEnabled(false);
    }
}

void GitLogDialog::loadCommitDiff(const QString &commitHash)
{
    QStringList args { "show", "--stat", "--format=fuller", commitHash };

    if (!m_filePath.isEmpty()) {
        args << "--" << m_filePath;
    }

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", args);

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_diffView->setPlainText(output);
    }
}

void GitLogDialog::onCommitSelectionChanged()
{
    auto *current = m_commitTree->currentItem();
    if (!current) {
        return;
    }

    QString hash = current->data(0, Qt::UserRole).toString();
    QString subject = current->text(1);
    QString author = current->text(2);
    QString date = current->text(3);

    QString details = QString("Commit: %1\nAuthor: %2\nDate: %3\nSubject: %4")
                              .arg(hash, author, date, subject);
    m_commitDetails->setPlainText(details);

    loadCommitDiff(hash);
}

void GitLogDialog::onRefreshClicked()
{
    loadCommitHistory();
}

void GitLogDialog::onBranchChanged()
{
    // TODO: 根据选择的分支过滤提交
    loadCommitHistory();
}

void GitLogDialog::onSearchTextChanged()
{
    // TODO: 实现搜索功能
}

void GitLogDialog::onLoadMoreClicked()
{
    loadCommitHistory(true);   // 追加模式加载更多提交
}

// ============================================================================
// GitOperationDialog Implementation
// ============================================================================

GitOperationDialog::GitOperationDialog(const QString &operation, QWidget *parent)
    : QDialog(parent), m_operation(operation), m_process(nullptr), m_outputTimer(new QTimer(this))
{
    setupUI();

    m_outputTimer->setInterval(100);
    connect(m_outputTimer, &QTimer::timeout, this, &GitOperationDialog::updateOutput);
}

void GitOperationDialog::setupUI()
{
    setWindowTitle(tr("Git %1").arg(m_operation));
    setModal(true);
    resize(600, 400);

    auto *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel(tr("Preparing to %1...").arg(m_operation));
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 0);   // 不确定进度
    layout->addWidget(m_progressBar);

    m_outputText = new QTextEdit;
    m_outputText->setReadOnly(true);
    m_outputText->setFont(QFont("Consolas", 9));
    layout->addWidget(m_outputText);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_closeButton = new QPushButton(tr("Close"));
    m_closeButton->setEnabled(false);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    layout->addLayout(buttonLayout);
}

void GitOperationDialog::executeCommand(const QString &workingDir, const QStringList &arguments)
{
    m_process = new QProcess(this);
    m_process->setWorkingDirectory(workingDir);

    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitOperationDialog::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &GitOperationDialog::onProcessError);

    m_statusLabel->setText(tr("Executing: git %1").arg(arguments.join(' ')));
    m_outputTimer->start();

    m_process->start("git", arguments);
}

void GitOperationDialog::updateOutput()
{
    if (m_process) {
        QString output = QString::fromUtf8(m_process->readAllStandardOutput());
        QString error = QString::fromUtf8(m_process->readAllStandardError());

        if (!output.isEmpty()) {
            m_outputText->append(output);
        }
        if (!error.isEmpty()) {
            m_outputText->append(QString("<font color='red'>%1</font>").arg(error));
        }
    }
}

void GitOperationDialog::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_outputTimer->stop();
    updateOutput();   // 获取最后的输出

    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(1);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_statusLabel->setText(tr("Operation completed successfully"));
        m_outputText->append(tr("\n--- Operation completed successfully ---"));
    } else {
        m_statusLabel->setText(tr("Operation failed (exit code: %1)").arg(exitCode));
        m_outputText->append(tr("\n--- Operation failed ---"));
    }

    m_closeButton->setEnabled(true);
}

void GitOperationDialog::onProcessError(QProcess::ProcessError error)
{
    m_outputTimer->stop();
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(1);

    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = tr("Failed to start git process");
        break;
    case QProcess::Crashed:
        errorMsg = tr("Git process crashed");
        break;
    case QProcess::Timedout:
        errorMsg = tr("Git process timed out");
        break;
    default:
        errorMsg = tr("Unknown error occurred");
    }

    m_statusLabel->setText(errorMsg);
    m_outputText->append(QString("<font color='red'>%1</font>").arg(errorMsg));
    m_closeButton->setEnabled(true);
}

// ============================================================================
// GitCheckoutDialog Implementation
// ============================================================================

GitCheckoutDialog::GitCheckoutDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath)
{
    setupUI();
    loadBranches();
    loadTags();
}

void GitCheckoutDialog::setupUI()
{
    setWindowTitle(tr("Git Checkout"));
    setModal(true);
    resize(500, 400);

    auto *layout = new QVBoxLayout(this);

    auto *tabWidget = new QTabWidget;

    // 分支标签页
    auto *branchWidget = new QWidget;
    auto *branchLayout = new QVBoxLayout(branchWidget);

    branchLayout->addWidget(new QLabel(tr("Select branch to checkout:")));
    m_branchList = new QListWidget;
    branchLayout->addWidget(m_branchList);

    auto *newBranchLayout = new QHBoxLayout;
    newBranchLayout->addWidget(new QLabel(tr("Create new branch:")));
    m_newBranchEdit = new QLineEdit;
    newBranchLayout->addWidget(m_newBranchEdit);
    branchLayout->addLayout(newBranchLayout);

    tabWidget->addTab(branchWidget, tr("Branches"));

    // 标签标签页
    auto *tagWidget = new QWidget;
    auto *tagLayout = new QVBoxLayout(tagWidget);

    tagLayout->addWidget(new QLabel(tr("Select tag to checkout:")));
    m_tagList = new QListWidget;
    tagLayout->addWidget(m_tagList);

    tabWidget->addTab(tagWidget, tr("Tags"));

    layout->addWidget(tabWidget);

    // 按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"));
    connect(m_cancelButton, &QPushButton::clicked, this, &GitCheckoutDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    m_checkoutButton = new QPushButton(tr("Checkout"));
    m_checkoutButton->setDefault(true);
    connect(m_checkoutButton, &QPushButton::clicked, this, &GitCheckoutDialog::onCheckoutClicked);
    buttonLayout->addWidget(m_checkoutButton);

    layout->addLayout(buttonLayout);

    // 连接信号
    connect(m_branchList, &QListWidget::itemDoubleClicked, this, &GitCheckoutDialog::onBranchDoubleClicked);
    connect(m_tagList, &QListWidget::itemDoubleClicked, this, &GitCheckoutDialog::onBranchDoubleClicked);
}

void GitCheckoutDialog::loadBranches()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "branch", "-a" });

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList branches = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &branch : branches) {
            QString cleanBranch = branch.trimmed();
            if (cleanBranch.startsWith('*')) {
                cleanBranch = cleanBranch.mid(2);
            }
            if (!cleanBranch.isEmpty()) {
                auto *item = new QListWidgetItem(cleanBranch);
                if (branch.startsWith('*')) {
                    item->setBackground(QColor(200, 255, 200));
                }
                m_branchList->addItem(item);
            }
        }
    }
}

void GitCheckoutDialog::loadTags()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "tag", "-l" });

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList tags = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &tag : tags) {
            m_tagList->addItem(tag.trimmed());
        }
    }
}

void GitCheckoutDialog::onCheckoutClicked()
{
    QString target;
    bool createNewBranch = false;

    if (!m_newBranchEdit->text().isEmpty()) {
        target = m_newBranchEdit->text();
        createNewBranch = true;
    } else if (m_branchList->currentItem()) {
        target = m_branchList->currentItem()->text();
    } else if (m_tagList->currentItem()) {
        target = m_tagList->currentItem()->text();
    }

    if (target.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please select a branch/tag or enter a new branch name."));
        return;
    }

    QStringList args;
    if (createNewBranch) {
        args << "checkout"
             << "-b" << target;
    } else {
        args << "checkout" << target;
    }

    auto *opDialog = new GitOperationDialog("Checkout", this);
    opDialog->executeCommand(m_repositoryPath, args);

    if (opDialog->exec() == QDialog::Accepted) {
        accept();
    }
}

void GitCheckoutDialog::onCancelClicked()
{
    reject();
}

void GitCheckoutDialog::onBranchDoubleClicked()
{
    onCheckoutClicked();
}

// ============================================================================
// GitCommitDialog Implementation
// ============================================================================

GitCommitDialog::GitCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_files(files)
{
    setupUI();
    loadStagedFiles();
}

void GitCommitDialog::setupUI()
{
    setWindowTitle(tr("Git Commit"));
    setModal(true);
    resize(600, 500);

    auto *layout = new QVBoxLayout(this);

    // 提交信息
    layout->addWidget(new QLabel(tr("Commit message:")));
    m_messageEdit = new QTextEdit;
    m_messageEdit->setMaximumHeight(120);
    m_messageEdit->setPlaceholderText(tr("Enter commit message..."));
    layout->addWidget(m_messageEdit);

    // 文件列表
    layout->addWidget(new QLabel(tr("Files to commit:")));
    m_fileList = new QListWidget;
    layout->addWidget(m_fileList);

    // 按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"));
    connect(m_cancelButton, &QPushButton::clicked, this, &GitCommitDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    m_commitButton = new QPushButton(tr("Commit"));
    m_commitButton->setDefault(true);
    m_commitButton->setEnabled(false);
    connect(m_commitButton, &QPushButton::clicked, this, &GitCommitDialog::onCommitClicked);
    buttonLayout->addWidget(m_commitButton);

    layout->addLayout(buttonLayout);

    // 连接信号
    connect(m_messageEdit, &QTextEdit::textChanged, this, &GitCommitDialog::onMessageChanged);
}

void GitCommitDialog::loadStagedFiles()
{
    // 如果指定了文件，显示这些文件
    if (!m_files.isEmpty()) {
        for (const QString &file : m_files) {
            auto *item = new QListWidgetItem(file);
            item->setCheckState(Qt::Checked);
            m_fileList->addItem(item);
        }
        return;
    }

    // 否则获取暂存区的文件
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    process.start("git", { "diff", "--cached", "--name-only" });

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList files = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &file : files) {
            auto *item = new QListWidgetItem(file.trimmed());
            item->setCheckState(Qt::Checked);
            m_fileList->addItem(item);
        }
    }
}

QString GitCommitDialog::getCommitMessage() const
{
    return m_messageEdit->toPlainText();
}

QStringList GitCommitDialog::getSelectedFiles() const
{
    QStringList selected;
    for (int i = 0; i < m_fileList->count(); ++i) {
        auto *item = m_fileList->item(i);
        if (item->checkState() == Qt::Checked) {
            selected << item->text();
        }
    }
    return selected;
}

void GitCommitDialog::onCommitClicked()
{
    QString message = getCommitMessage();
    if (message.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("Please enter a commit message."));
        return;
    }

    QStringList args { "commit", "-m", message };

    auto *opDialog = new GitOperationDialog("Commit", this);
    opDialog->executeCommand(m_repositoryPath, args);

    if (opDialog->exec() == QDialog::Accepted) {
        accept();
    }
}

void GitCommitDialog::onCancelClicked()
{
    reject();
}

void GitCommitDialog::onMessageChanged()
{
    m_commitButton->setEnabled(!m_messageEdit->toPlainText().isEmpty());
}
