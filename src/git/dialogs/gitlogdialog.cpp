#include "gitlogdialog.h"

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

GitLogDialog::GitLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent),
      m_repositoryPath(repositoryPath),
      m_filePath(filePath),
      m_mainSplitter(nullptr),
      m_rightSplitter(nullptr),
      m_commitTree(nullptr),
      m_branchCombo(nullptr),
      m_searchEdit(nullptr),
      m_refreshButton(nullptr),
      m_loadMoreButton(nullptr),
      m_commitDetails(nullptr),
      m_diffView(nullptr),
      m_currentOffset(0)
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