#include "gitcommitdialog.h"
#include "gitoperationdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>

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