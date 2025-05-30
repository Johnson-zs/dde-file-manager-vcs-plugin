#include "gitdiffdialog.h"
#include "../utils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QFileInfo>
#include <QDir>
#include <QProcess>

GitDiffDialog::GitDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_filePath(filePath)
{
    setupUI();
    loadFileDiff();
}

void GitDiffDialog::setupUI()
{
    setWindowTitle(tr("Git Diff - %1").arg(QFileInfo(m_filePath).fileName()));
    setModal(false);
    resize(900, 600);

    auto *layout = new QVBoxLayout(this);

    // 文件信息标签
    m_fileInfoLabel = new QLabel;
    m_fileInfoLabel->setWordWrap(true);
    m_fileInfoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border-radius: 4px; }");
    layout->addWidget(m_fileInfoLabel);

    // 工具栏
    auto *toolbarLayout = new QHBoxLayout;
    m_refreshButton = new QPushButton(tr("Refresh"));
    connect(m_refreshButton, &QPushButton::clicked, this, &GitDiffDialog::onRefreshClicked);
    toolbarLayout->addWidget(m_refreshButton);
    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);

    // 差异视图
    m_diffView = new QTextEdit;
    m_diffView->setReadOnly(true);
    m_diffView->setFont(QFont("Consolas", 10));
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
    layout->addWidget(m_diffView);

    // 关闭按钮
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto *closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
}

void GitDiffDialog::loadFileDiff()
{
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // 计算相对路径
    QDir repoDir(m_repositoryPath);
    QString relativePath = repoDir.relativeFilePath(m_filePath);

    QStringList args { "diff", "HEAD", "--", relativePath };
    process.start("git", args);

    if (process.waitForFinished(5000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QString error = QString::fromUtf8(process.readAllStandardError());

        if (process.exitCode() == 0) {
            if (output.isEmpty()) {
                m_diffView->setPlainText(tr("No changes found in this file."));
            } else {
                m_diffView->setPlainText(output);
                applySyntaxHighlighting();
            }

            // 更新文件信息
            QString statusText = Utils::getFileStatusDescription(m_filePath);
            m_fileInfoLabel->setText(tr("File: %1\nStatus: %2\nRepository: %3")
                                             .arg(relativePath, statusText, m_repositoryPath));
        } else {
            m_diffView->setPlainText(tr("Error loading diff:\n%1").arg(error));
        }
    } else {
        m_diffView->setPlainText(tr("Failed to execute git diff command."));
    }
}

void GitDiffDialog::applySyntaxHighlighting()
{
    // 简单的差异语法高亮
    QTextDocument *doc = m_diffView->document();
    QTextCursor cursor(doc);

    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat addedFormat;
    addedFormat.setBackground(QColor(220, 255, 220));

    QTextCharFormat removedFormat;
    removedFormat.setBackground(QColor(255, 220, 220));

    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(128, 128, 128));
    headerFormat.setFontWeight(QFont::Bold);

    while (!cursor.atEnd()) {
        cursor.select(QTextCursor::LineUnderCursor);
        QString line = cursor.selectedText();

        if (line.startsWith('+') && !line.startsWith("+++")) {
            cursor.setCharFormat(addedFormat);
        } else if (line.startsWith('-') && !line.startsWith("---")) {
            cursor.setCharFormat(removedFormat);
        } else if (line.startsWith("@@") || line.startsWith("diff ") || line.startsWith("index ") || line.startsWith("+++") || line.startsWith("---")) {
            cursor.setCharFormat(headerFormat);
        }

        cursor.movePosition(QTextCursor::NextBlock);
    }

    cursor.endEditBlock();
}

void GitDiffDialog::onRefreshClicked()
{
    loadFileDiff();
} 