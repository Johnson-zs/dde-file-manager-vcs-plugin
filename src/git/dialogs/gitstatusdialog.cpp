#include "gitstatusdialog.h"
#include "gitdiffdialog.h"
#include "../utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

GitStatusDialog::GitStatusDialog(const QString &repositoryPath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_branchLabel(nullptr)
    , m_statusSummary(nullptr)
    , m_stagedFilesList(nullptr)
    , m_unstagedFilesList(nullptr)
    , m_untrackedFilesList(nullptr)
    , m_refreshButton(nullptr)
{
    setWindowTitle(tr("Git Repository Status"));
    setMinimumSize(800, 600);
    setupUI();
    loadRepositoryStatus();
}

void GitStatusDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // === 仓库信息区域 ===
    auto *infoGroup = new QGroupBox(tr("Repository Information"), this);
    auto *infoLayout = new QVBoxLayout(infoGroup);

    m_branchLabel = new QLabel(this);
    m_branchLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    infoLayout->addWidget(m_branchLabel);

    m_statusSummary = new QLabel(this);
    m_statusSummary->setWordWrap(true);
    infoLayout->addWidget(m_statusSummary);

    mainLayout->addWidget(infoGroup);

    // === 文件状态区域 ===
    auto *filesGroup = new QGroupBox(tr("File Status"), this);
    auto *filesLayout = new QGridLayout(filesGroup);

    // 已暂存文件
    auto *stagedLabel = new QLabel(tr("Staged Files:"), this);
    stagedLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    m_stagedFilesList = new QListWidget(this);
    m_stagedFilesList->setToolTip(tr("Files ready to be committed\nDouble-click to view diff"));
    filesLayout->addWidget(stagedLabel, 0, 0);
    filesLayout->addWidget(m_stagedFilesList, 1, 0);

    // 未暂存文件
    auto *unstagedLabel = new QLabel(tr("Modified Files:"), this);
    unstagedLabel->setStyleSheet("font-weight: bold; color: #FF9800;");
    m_unstagedFilesList = new QListWidget(this);
    m_unstagedFilesList->setToolTip(tr("Modified files not yet staged\nDouble-click to view diff"));
    filesLayout->addWidget(unstagedLabel, 0, 1);
    filesLayout->addWidget(m_unstagedFilesList, 1, 1);

    // 未跟踪文件
    auto *untrackedLabel = new QLabel(tr("Untracked Files:"), this);
    untrackedLabel->setStyleSheet("font-weight: bold; color: #9E9E9E;");
    m_untrackedFilesList = new QListWidget(this);
    m_untrackedFilesList->setToolTip(tr("New files not yet added to Git\nDouble-click to open file"));
    filesLayout->addWidget(untrackedLabel, 0, 2);
    filesLayout->addWidget(m_untrackedFilesList, 1, 2);

    mainLayout->addWidget(filesGroup);

    // === 按钮区域 ===
    auto *buttonLayout = new QHBoxLayout();
    
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Refresh repository status"));
    buttonLayout->addWidget(m_refreshButton);
    
    buttonLayout->addStretch();
    
    auto *closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);

    // === 信号连接 ===
    connect(m_refreshButton, &QPushButton::clicked, this, &GitStatusDialog::onRefreshClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    
    connect(m_stagedFilesList, &QListWidget::itemDoubleClicked, 
            this, &GitStatusDialog::onFileDoubleClicked);
    connect(m_unstagedFilesList, &QListWidget::itemDoubleClicked, 
            this, &GitStatusDialog::onFileDoubleClicked);
    connect(m_untrackedFilesList, &QListWidget::itemDoubleClicked, 
            this, &GitStatusDialog::onFileDoubleClicked);
}

void GitStatusDialog::loadRepositoryStatus()
{
    if (m_repositoryPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid repository path"));
        return;
    }

    // 清空现有内容
    m_stagedFilesList->clear();
    m_unstagedFilesList->clear();
    m_untrackedFilesList->clear();

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // === 获取当前分支信息 ===
    process.start("git", QStringList() << "branch" << "--show-current");
    if (process.waitForFinished(3000)) {
        QString branch = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (branch.isEmpty()) {
            branch = tr("(detached HEAD)");
        }
        m_branchLabel->setText(tr("Current Branch: %1").arg(branch));
    } else {
        m_branchLabel->setText(tr("Current Branch: Unknown"));
        qWarning() << "Failed to get current branch:" << process.errorString();
    }

    // === 获取文件状态 ===
    process.start("git", QStringList() << "status" << "--porcelain");
    if (!process.waitForFinished(5000)) {
        QMessageBox::critical(this, tr("Error"), 
                             tr("Failed to get repository status: %1").arg(process.errorString()));
        return;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);

    int stagedCount = 0;
    int modifiedCount = 0;
    int untrackedCount = 0;

    for (const QString &line : lines) {
        if (line.length() < 3) continue;

        const QString status = line.left(2);
        const QString filePath = line.mid(3);

        if (status.at(0) != ' ' && status.at(0) != '?') {
            // 已暂存的文件
            auto *item = new QListWidgetItem(filePath);
            item->setToolTip(tr("Status: %1\nDouble-click to view diff").arg(getStatusDescription(status)));
            item->setIcon(getStatusIcon(status));
            item->setData(Qt::UserRole, status); // 存储状态信息
            m_stagedFilesList->addItem(item);
            stagedCount++;
        } else if (status.at(1) != ' ') {
            // 已修改但未暂存的文件
            auto *item = new QListWidgetItem(filePath);
            item->setToolTip(tr("Status: %1\nDouble-click to view diff").arg(getStatusDescription(status)));
            item->setIcon(getStatusIcon(status));
            item->setData(Qt::UserRole, status); // 存储状态信息
            m_unstagedFilesList->addItem(item);
            modifiedCount++;
        }

        if (status == "??") {
            // 未跟踪的文件
            auto *item = new QListWidgetItem(filePath);
            item->setToolTip(tr("Untracked file\nDouble-click to open file"));
            item->setIcon(QIcon::fromTheme("document-new"));
            item->setData(Qt::UserRole, status); // 存储状态信息
            m_untrackedFilesList->addItem(item);
            untrackedCount++;
        }
    }

    updateStatusSummary();
}

void GitStatusDialog::updateStatusSummary()
{
    const int stagedCount = m_stagedFilesList->count();
    const int modifiedCount = m_unstagedFilesList->count();
    const int untrackedCount = m_untrackedFilesList->count();

    QString summary;
    if (stagedCount == 0 && modifiedCount == 0 && untrackedCount == 0) {
        summary = tr("Working directory is clean");
    } else {
        QStringList parts;
        if (stagedCount > 0) {
            parts << tr("%1 staged").arg(stagedCount);
        }
        if (modifiedCount > 0) {
            parts << tr("%1 modified").arg(modifiedCount);
        }
        if (untrackedCount > 0) {
            parts << tr("%1 untracked").arg(untrackedCount);
        }
        summary = parts.join(", ");
    }

    m_statusSummary->setText(summary);
}

void GitStatusDialog::onRefreshClicked()
{
    loadRepositoryStatus();
}

void GitStatusDialog::onFileDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;

    const QString filePath = item->text();
    const QString fullPath = QDir(m_repositoryPath).absoluteFilePath(filePath);
    const QString status = item->data(Qt::UserRole).toString();

    qInfo() << "INFO: [GitStatusDialog::onFileDoubleClicked] File:" << filePath << "Status:" << status;

    // 根据文件状态决定操作
    if (status == "??") {
        // 未跟踪文件：直接打开文件
        if (QFileInfo::exists(fullPath)) {
            QProcess::startDetached("xdg-open", QStringList() << fullPath);
        } else {
            QMessageBox::information(this, tr("File Not Found"), 
                                    tr("The file '%1' does not exist in the working directory.").arg(filePath));
        }
    } else {
        // 已修改或已暂存文件：显示diff界面
        if (Utils::canShowFileDiff(fullPath)) {
            qInfo() << "INFO: [GitStatusDialog::onFileDoubleClicked] Opening diff dialog for:" << fullPath;
            
            auto *diffDialog = new GitDiffDialog(m_repositoryPath, fullPath, this);
            diffDialog->setAttribute(Qt::WA_DeleteOnClose);
            diffDialog->show();
        } else {
            // 如果不能显示diff，则尝试打开文件
            if (QFileInfo::exists(fullPath)) {
                QProcess::startDetached("xdg-open", QStringList() << fullPath);
            } else {
                QMessageBox::information(this, tr("File Not Found"), 
                                        tr("The file '%1' does not exist in the working directory.").arg(filePath));
            }
        }
    }
}

QString GitStatusDialog::getStatusDescription(const QString &status) const
{
    if (status.length() != 2) return tr("Unknown");

    const QChar index = status.at(0);
    const QChar workTree = status.at(1);

    QString desc;
    
    // 索引状态
    switch (index.toLatin1()) {
    case 'A': desc += tr("Added"); break;
    case 'M': desc += tr("Modified"); break;
    case 'D': desc += tr("Deleted"); break;
    case 'R': desc += tr("Renamed"); break;
    case 'C': desc += tr("Copied"); break;
    case ' ': break;
    default: desc += tr("Unknown"); break;
    }

    // 工作树状态
    if (workTree != ' ') {
        if (!desc.isEmpty()) desc += ", ";
        switch (workTree.toLatin1()) {
        case 'M': desc += tr("Modified in working tree"); break;
        case 'D': desc += tr("Deleted in working tree"); break;
        case '?': desc += tr("Untracked"); break;
        default: desc += tr("Unknown working tree status"); break;
        }
    }

    return desc.isEmpty() ? tr("Unchanged") : desc;
}

QIcon GitStatusDialog::getStatusIcon(const QString &status) const
{
    if (status.length() != 2) return QIcon();

    const QChar index = status.at(0);
    const QChar workTree = status.at(1);

    // 优先显示索引状态的图标
    switch (index.toLatin1()) {
    case 'A': return QIcon::fromTheme("list-add");
    case 'M': return QIcon::fromTheme("document-edit");
    case 'D': return QIcon::fromTheme("list-remove");
    case 'R': return QIcon::fromTheme("edit-rename");
    case 'C': return QIcon::fromTheme("edit-copy");
    default:
        // 如果索引没有状态，检查工作树状态
        switch (workTree.toLatin1()) {
        case 'M': return QIcon::fromTheme("document-edit");
        case 'D': return QIcon::fromTheme("list-remove");
        case '?': return QIcon::fromTheme("document-new");
        default: return QIcon::fromTheme("text-plain");
        }
    }
} 