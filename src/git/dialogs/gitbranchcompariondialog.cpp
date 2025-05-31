#include "gitbranchcompariondialog.h"
#include "widgets/linenumbertextedit.h"

#include <QApplication>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>
#include <QFont>
#include <QColor>
#include <QTextCharFormat>
#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QIcon>
#include <QFileInfo>

/**
 * @brief Git差异语法高亮器
 */
class GitDiffHighlighter : public QSyntaxHighlighter
{
public:
    explicit GitDiffHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        // 设置添加行格式（绿色）
        m_addedFormat.setForeground(QColor(0, 128, 0));
        m_addedFormat.setBackground(QColor(220, 255, 220));

        // 设置删除行格式（红色）
        m_removedFormat.setForeground(QColor(128, 0, 0));
        m_removedFormat.setBackground(QColor(255, 220, 220));

        // 设置文件头格式（蓝色）
        m_headerFormat.setForeground(QColor(0, 0, 128));
        m_headerFormat.setFontWeight(QFont::Bold);

        // 设置行号格式（灰色）
        m_lineNumberFormat.setForeground(QColor(128, 128, 128));
    }

protected:
    void highlightBlock(const QString &text) override
    {
        if (text.startsWith('+') && !text.startsWith("+++")) {
            setFormat(0, text.length(), m_addedFormat);
        } else if (text.startsWith('-') && !text.startsWith("---")) {
            setFormat(0, text.length(), m_removedFormat);
        } else if (text.startsWith("@@") || text.startsWith("+++") || text.startsWith("---") || text.startsWith("diff ")) {
            setFormat(0, text.length(), m_headerFormat);
        } else if (text.startsWith("\\")) {
            setFormat(0, text.length(), m_lineNumberFormat);
        }
    }

private:
    QTextCharFormat m_addedFormat;
    QTextCharFormat m_removedFormat;
    QTextCharFormat m_headerFormat;
    QTextCharFormat m_lineNumberFormat;
};

GitBranchComparisonDialog::GitBranchComparisonDialog(const QString &repositoryPath,
                                                     const QString &baseBranch,
                                                     const QString &compareBranch,
                                                     QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_baseBranch(baseBranch), m_compareBranch(compareBranch)
{
    setupUI();
    loadComparison();

    qDebug() << "[GitBranchComparisonDialog] Initialized comparison:" << baseBranch << "vs" << compareBranch;
}

GitBranchComparisonDialog::~GitBranchComparisonDialog() = default;

void GitBranchComparisonDialog::setupUI()
{
    setWindowTitle(tr("Branch Comparison: %1 ↔ %2").arg(m_baseBranch, m_compareBranch));
    setModal(false);
    setMinimumSize(1000, 700);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1400, 900);
    setAttribute(Qt::WA_DeleteOnClose);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);

    // 头部信息区域
    m_headerLayout = new QHBoxLayout;

    m_comparisonLabel = new QLabel;
    m_comparisonLabel->setText(tr("Comparing <b>%1</b> with <b>%2</b>").arg(m_baseBranch, m_compareBranch));
    m_comparisonLabel->setStyleSheet("QLabel { color: #2196F3; font-size: 14px; }");

    m_swapButton = new QPushButton(tr("⇄ Swap"));
    m_swapButton->setToolTip(tr("Swap base and compare branches"));
    m_swapButton->setMaximumWidth(80);

    m_refreshButton = new QPushButton(tr("🔄 Refresh"));
    m_refreshButton->setToolTip(tr("Refresh comparison"));
    m_refreshButton->setMaximumWidth(100);

    m_headerLayout->addWidget(m_comparisonLabel);
    m_headerLayout->addStretch();
    m_headerLayout->addWidget(m_swapButton);
    m_headerLayout->addWidget(m_refreshButton);

    m_mainLayout->addLayout(m_headerLayout);

    // 主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    // 左侧分割器（提交列表和文件列表）
    m_leftSplitter = new QSplitter(Qt::Vertical);

    setupCommitList();
    setupFileList();
    setupDiffView();

    // 设置分割器比例
    m_leftSplitter->setSizes({ 350, 250 });
    m_mainSplitter->addWidget(m_leftSplitter);
    m_mainSplitter->addWidget(m_diffView);
    m_mainSplitter->setSizes({ 600, 800 });

    m_mainLayout->addWidget(m_mainSplitter);

    // 连接信号
    connect(m_swapButton, &QPushButton::clicked, this, &GitBranchComparisonDialog::onSwapBranchesClicked);
    connect(m_refreshButton, &QPushButton::clicked, this, &GitBranchComparisonDialog::onRefreshClicked);
    connect(m_commitList, &QTreeWidget::itemSelectionChanged, this, &GitBranchComparisonDialog::onCommitSelectionChanged);
    connect(m_fileList, &QTreeWidget::itemSelectionChanged, this, &GitBranchComparisonDialog::onFileSelectionChanged);
}

void GitBranchComparisonDialog::setupCommitList()
{
    // 创建标签页容器
    m_leftTabWidget = new QTabWidget;

    // 提交差异标签页
    m_commitList = new QTreeWidget;
    QStringList commitHeaders;
    commitHeaders << tr("Direction") << tr("Subject") << tr("Author") << tr("Date") << tr("Hash");
    m_commitList->setHeaderLabels(commitHeaders);
    m_commitList->setRootIsDecorated(false);
    m_commitList->setAlternatingRowColors(true);
    m_commitList->setSortingEnabled(false);
    m_commitList->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置列宽
    m_commitList->setColumnWidth(0, 80);   // Direction
    m_commitList->setColumnWidth(1, 300);   // Subject
    m_commitList->setColumnWidth(2, 120);   // Author
    m_commitList->setColumnWidth(3, 100);   // Date
    m_commitList->setColumnWidth(4, 80);   // Hash

    m_leftTabWidget->addTab(m_commitList, tr("📝 Commits"));
    m_leftSplitter->addWidget(m_leftTabWidget);
}

void GitBranchComparisonDialog::setupFileList()
{
    // 文件变更标签页
    m_fileList = new QTreeWidget;
    QStringList fileHeaders;
    fileHeaders << tr("Status") << tr("File Path");
    m_fileList->setHeaderLabels(fileHeaders);
    m_fileList->setRootIsDecorated(false);
    m_fileList->setAlternatingRowColors(true);
    m_fileList->setSortingEnabled(false);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);

    // 设置列宽
    m_fileList->setColumnWidth(0, 80);   // Status
    m_fileList->header()->setStretchLastSection(true);   // File Path

    m_leftTabWidget->addTab(m_fileList, tr("📁 Files"));
}

void GitBranchComparisonDialog::setupDiffView()
{
    m_diffView = new LineNumberTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 10));
    m_diffView->setPlainText(tr("Select a commit or file to view differences..."));

    // 应用语法高亮
    auto *highlighter = new GitDiffHighlighter(m_diffView->document());
    Q_UNUSED(highlighter)
}

void GitBranchComparisonDialog::loadComparison()
{
    qDebug() << "[GitBranchComparisonDialog] Loading comparison data";

    loadCommitDifferences();
    loadFileDifferences();

    // 更新统计信息
    QString statsText = tr("Comparing <b>%1</b> with <b>%2</b> • %3 commits, %4 files changed")
                                .arg(m_baseBranch, m_compareBranch)
                                .arg(m_commits.size())
                                .arg(m_files.size());
    m_comparisonLabel->setText(statsText);
}

void GitBranchComparisonDialog::loadCommitDifferences()
{
    m_commits.clear();
    m_commitList->clear();

    // 获取提交差异：baseBranch...compareBranch
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    QStringList args;
    args << "log"
         << "--left-right"
         << "--oneline"
         << "--pretty=format:%m|%H|%h|%s|%an|%ad"
         << "--date=short" << QString("%1...%2").arg(m_baseBranch, m_compareBranch);

    process.start("git", args);

    if (process.waitForFinished(10000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            QStringList parts = line.split('|');
            if (parts.size() >= 6) {
                CommitInfo commit;
                commit.direction = parts[0].trimmed() == "<" ? "behind" : "ahead";
                commit.hash = parts[1];
                commit.shortHash = parts[2];
                commit.subject = parts[3];
                commit.author = parts[4];
                commit.date = parts[5];

                m_commits.append(commit);

                // 添加到列表
                auto *item = new QTreeWidgetItem(m_commitList);

                // 设置方向图标和颜色
                if (commit.direction == "ahead") {
                    item->setText(0, "→ Ahead");
                    item->setForeground(0, QColor(0, 128, 0));   // 绿色
                } else {
                    item->setText(0, "← Behind");
                    item->setForeground(0, QColor(128, 0, 0));   // 红色
                }

                item->setText(1, commit.subject);
                item->setText(2, commit.author);
                item->setText(3, commit.date);
                item->setText(4, commit.shortHash);

                // 存储完整哈希用于后续查询
                item->setData(0, Qt::UserRole, commit.hash);
            }
        }

        qDebug() << "[GitBranchComparisonDialog] Loaded" << m_commits.size() << "commit differences";
    } else {
        qWarning() << "[GitBranchComparisonDialog] Failed to load commit differences";
    }
}

void GitBranchComparisonDialog::loadFileDifferences()
{
    m_files.clear();
    m_fileList->clear();

    // 获取文件差异
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    QStringList args;
    args << "diff"
         << "--name-status" << QString("%1...%2").arg(m_baseBranch, m_compareBranch);

    process.start("git", args);

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);

        for (const QString &line : lines) {
            QStringList parts = line.split('\t');
            if (parts.size() >= 2) {
                FileInfo file;
                file.status = parts[0];
                file.path = parts[1];

                // 处理重命名文件
                if (file.status.startsWith('R') && parts.size() >= 3) {
                    file.oldPath = file.path;
                    file.path = parts[2];
                }

                m_files.append(file);

                // 添加到列表
                auto *item = new QTreeWidgetItem(m_fileList);

                // 设置状态图标和颜色
                QString statusText;
                QColor statusColor;

                switch (file.status.at(0).toLatin1()) {
                case 'A':
                    statusText = "➕ Added";
                    statusColor = QColor(0, 128, 0);
                    break;
                case 'M':
                    statusText = "📝 Modified";
                    statusColor = QColor(0, 0, 128);
                    break;
                case 'D':
                    statusText = "➖ Deleted";
                    statusColor = QColor(128, 0, 0);
                    break;
                case 'R':
                    statusText = "📋 Renamed";
                    statusColor = QColor(128, 128, 0);
                    break;
                case 'C':
                    statusText = "📄 Copied";
                    statusColor = QColor(128, 0, 128);
                    break;
                default:
                    statusText = file.status;
                    statusColor = QColor(64, 64, 64);
                    break;
                }

                item->setText(0, statusText);
                item->setForeground(0, statusColor);

                // 显示文件路径
                QString displayPath = file.path;
                if (!file.oldPath.isEmpty()) {
                    displayPath = QString("%1 → %2").arg(file.oldPath, file.path);
                }
                item->setText(1, displayPath);

                // 存储文件路径用于后续查询
                item->setData(0, Qt::UserRole, file.path);
            }
        }

        qDebug() << "[GitBranchComparisonDialog] Loaded" << m_files.size() << "file differences";
    } else {
        qWarning() << "[GitBranchComparisonDialog] Failed to load file differences";
    }
}

void GitBranchComparisonDialog::onCommitSelectionChanged()
{
    auto selectedItems = m_commitList->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QString commitHash = selectedItems.first()->data(0, Qt::UserRole).toString();
    showCommitDiff(commitHash);
}

void GitBranchComparisonDialog::onFileSelectionChanged()
{
    auto selectedItems = m_fileList->selectedItems();
    if (selectedItems.isEmpty()) {
        return;
    }

    QString filePath = selectedItems.first()->data(0, Qt::UserRole).toString();
    showFileDiff(filePath);
}

void GitBranchComparisonDialog::showCommitDiff(const QString &commitHash)
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    QStringList args;
    args << "show"
         << "--pretty=fuller" << commitHash;

    process.start("git", args);

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        m_diffView->setPlainText(output);
        qDebug() << "[GitBranchComparisonDialog] Showing commit diff for:" << commitHash;
    } else {
        m_diffView->setPlainText(tr("Failed to load commit diff for: %1").arg(commitHash));
        qWarning() << "[GitBranchComparisonDialog] Failed to load commit diff for:" << commitHash;
    }
}

void GitBranchComparisonDialog::showFileDiff(const QString &filePath)
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    QStringList args;
    args << "diff" << QString("%1...%2").arg(m_baseBranch, m_compareBranch) << "--" << filePath;

    process.start("git", args);

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        if (output.isEmpty()) {
            m_diffView->setPlainText(tr("No differences found for file: %1").arg(filePath));
        } else {
            m_diffView->setPlainText(output);
        }
        qDebug() << "[GitBranchComparisonDialog] Showing file diff for:" << filePath;
    } else {
        m_diffView->setPlainText(tr("Failed to load file diff for: %1").arg(filePath));
        qWarning() << "[GitBranchComparisonDialog] Failed to load file diff for:" << filePath;
    }
}

void GitBranchComparisonDialog::onRefreshClicked()
{
    qDebug() << "[GitBranchComparisonDialog] Refreshing comparison";
    loadComparison();
}

void GitBranchComparisonDialog::onSwapBranchesClicked()
{
    qDebug() << "[GitBranchComparisonDialog] Swapping branches";

    // 交换分支
    QString temp = m_baseBranch;
    m_baseBranch = m_compareBranch;
    m_compareBranch = temp;

    // 更新窗口标题和标签
    setWindowTitle(tr("Branch Comparison: %1 ↔ %2").arg(m_baseBranch, m_compareBranch));

    // 重新加载数据
    loadComparison();
}
