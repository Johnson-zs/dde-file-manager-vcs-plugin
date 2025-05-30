#include "gitcheckoutdialog.h"
#include "gitoperationdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>

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
        args << "checkout" << "-b" << target;
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