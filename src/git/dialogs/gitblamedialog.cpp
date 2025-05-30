#include "gitblamedialog.h"
#include "gitcommandexecutor.h"

#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMessageBox>
#include <QTextCursor>
#include <QTextBlock>
#include <QScrollBar>
#include <QRegularExpression>
#include <QHelpEvent>
#include <QSplitter>
#include <QGroupBox>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QUrl>
#include <QTimer>

GitBlameDialog::GitBlameDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent), m_repositoryPath(repositoryPath), m_filePath(filePath), m_fileName(QFileInfo(filePath).fileName()), m_currentSelectedLine(-1), m_contextMenu(nullptr), m_showCommitDetailsAction(nullptr), m_filePathLabel(nullptr), m_blameTextEdit(nullptr), m_refreshButton(nullptr), m_closeButton(nullptr), m_progressBar(nullptr), m_statusLabel(nullptr)
{
    setWindowTitle(tr("Git Blame - %1").arg(m_fileName));
    setMinimumSize(1200, 800);
    // Enable maximize button and better default size
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1400, 900);
    setAttribute(Qt::WA_DeleteOnClose);

    setupUI();
    setupContextMenu();
    loadBlameData();

    qDebug() << "[GitBlameDialog] Initialized with enhanced layout for file:" << filePath;
}

void GitBlameDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // === 头部信息区域 ===
    auto *headerGroup = new QGroupBox(tr("File Information"), this);
    auto *headerLayout = new QVBoxLayout(headerGroup);

    m_filePathLabel = new QLabel(this);
    m_filePathLabel->setText(tr("File: %1").arg(m_filePath));
    m_filePathLabel->setStyleSheet("font-weight: bold; color: #2196F3; font-size: 12px;");
    headerLayout->addWidget(m_filePathLabel);

    m_statusLabel = new QLabel(tr("Loading blame information..."), this);
    m_statusLabel->setStyleSheet("color: #666; font-size: 11px;");
    headerLayout->addWidget(m_statusLabel);

    // 添加使用提示
    auto *hintLabel = new QLabel(tr("💡 Tip: Click on commit hash to view details, double-click anywhere to show commit info"), this);
    hintLabel->setStyleSheet("color: #888; font-size: 10px; font-style: italic;");
    hintLabel->setWordWrap(true);
    headerLayout->addWidget(hintLabel);

    mainLayout->addWidget(headerGroup);

    // === 进度条 ===
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // === Blame显示区域 ===
    auto *blameGroup = new QGroupBox(tr("Blame Information"), this);
    auto *blameLayout = new QVBoxLayout(blameGroup);

    m_blameTextEdit = new QTextBrowser(this);
    m_blameTextEdit->setReadOnly(true);
    m_blameTextEdit->setFont(QFont("Courier", 10));
    m_blameTextEdit->setLineWrapMode(QTextEdit::NoWrap);

    // 禁用默认的链接打开行为，防止点击链接时清空内容
    m_blameTextEdit->setOpenLinks(false);

    // 启用鼠标跟踪以便检测鼠标悬停
    m_blameTextEdit->setMouseTracking(true);
    setMouseTracking(true);

    // 连接超链接点击信号
    connect(m_blameTextEdit, &QTextBrowser::anchorClicked, this, &GitBlameDialog::onHashLinkClicked);

    blameLayout->addWidget(m_blameTextEdit);
    mainLayout->addWidget(blameGroup);

    // === 按钮区域 ===
    auto *buttonLayout = new QHBoxLayout();

    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Refresh blame information"));
    buttonLayout->addWidget(m_refreshButton);

    buttonLayout->addStretch();

    m_closeButton = new QPushButton(tr("Close"), this);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    // === 信号连接 ===
    connect(m_refreshButton, &QPushButton::clicked, this, &GitBlameDialog::onRefreshClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    qDebug() << "[GitBlameDialog] UI setup completed";
}

void GitBlameDialog::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    m_showCommitDetailsAction = new QAction(tr("Show Commit Details"), this);
    m_showCommitDetailsAction->setShortcut(QKeySequence(Qt::Key_Return));
    m_showCommitDetailsAction->setEnabled(false);
    connect(m_showCommitDetailsAction, &QAction::triggered, this, &GitBlameDialog::onShowCommitDetailsTriggered);

    m_contextMenu->addAction(m_showCommitDetailsAction);
    m_contextMenu->addSeparator();

    auto *refreshAction = new QAction(tr("Refresh"), this);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &GitBlameDialog::onRefreshClicked);
    m_contextMenu->addAction(refreshAction);

    // 将动作添加到对话框，以便快捷键生效
    addAction(m_showCommitDetailsAction);
    addAction(refreshAction);
}

void GitBlameDialog::contextMenuEvent(QContextMenuEvent *event)
{
    const QPoint pos = event->pos();
    const QPoint textPos = m_blameTextEdit->mapFromParent(pos);

    if (m_blameTextEdit->rect().contains(textPos)) {
        const int lineNumber = getLineNumberFromPosition(textPos);
        m_currentSelectedLine = lineNumber;

        if (lineNumber >= 0 && lineNumber < m_blameData.size()) {
            const QString hash = m_blameData[lineNumber].hash;
            m_showCommitDetailsAction->setText(tr("Show Commit Details (%1)").arg(hash.left(8)));
            m_showCommitDetailsAction->setEnabled(!hash.isEmpty());
        } else {
            m_showCommitDetailsAction->setText(tr("Show Commit Details"));
            m_showCommitDetailsAction->setEnabled(false);
        }

        m_contextMenu->exec(event->globalPos());
    }
}

void GitBlameDialog::onShowCommitDetailsTriggered()
{
    if (m_currentSelectedLine >= 0 && m_currentSelectedLine < m_blameData.size()) {
        const QString hash = m_blameData[m_currentSelectedLine].hash;
        if (!hash.isEmpty()) {
            showCommitDetails(hash);
        }
    }
}

void GitBlameDialog::loadBlameData()
{
    m_blameData.clear();
    m_blameTextEdit->clear();
    m_statusLabel->setText(tr("Loading blame information..."));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);   // 不确定进度

    // 计算相对路径
    GitCommandExecutor executor;
    const QString relativePath = executor.makeRelativePath(m_repositoryPath, m_filePath);
    if (relativePath.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to calculate relative path for file."));
        m_progressBar->setVisible(false);
        return;
    }

    qDebug() << "[GitBlameDialog] Loading blame for relative path:" << relativePath;

    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // 使用 --line-porcelain 格式获取详细的blame信息
    QStringList args;
    args << "blame"
         << "--line-porcelain" << relativePath;

    process.start("git", args);
    if (!process.waitForFinished(30000)) {   // 30秒超时
        QMessageBox::critical(this, tr("Error"),
                              tr("Git blame command timed out or failed: %1").arg(process.errorString()));
        m_progressBar->setVisible(false);
        return;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QString error = QString::fromUtf8(process.readAllStandardError());

    if (process.exitCode() != 0) {
        QMessageBox::critical(this, tr("Error"),
                              tr("Git blame failed:\n%1").arg(error));
        m_progressBar->setVisible(false);
        return;
    }

    if (output.isEmpty()) {
        QMessageBox::information(this, tr("No Data"), tr("No blame information available for this file."));
        m_progressBar->setVisible(false);
        return;
    }

    // 解析blame输出
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    m_progressBar->setRange(0, lines.size());

    int currentIndex = 0;
    int lineNumber = 1;

    while (currentIndex < lines.size()) {
        m_progressBar->setValue(currentIndex);
        QApplication::processEvents();

        BlameLineInfo info = parseBlameLineInfo(lines, currentIndex);
        if (!info.hash.isEmpty()) {
            info.lineNumber = lineNumber++;
            m_blameData.append(info);
        }
    }

    m_progressBar->setVisible(false);

    if (m_blameData.isEmpty()) {
        m_statusLabel->setText(tr("No blame information found."));
        return;
    }

    formatBlameDisplay();

    // 默认选中第一行
    if (!m_blameData.isEmpty()) {
        m_currentSelectedLine = 0;
    }

    m_statusLabel->setText(tr("Blame information loaded successfully. %1 lines processed.").arg(m_blameData.size()));

    qDebug() << "[GitBlameDialog] Blame data loaded successfully," << m_blameData.size() << "lines";
}

BlameLineInfo GitBlameDialog::parseBlameLineInfo(const QStringList &blameLines, int &currentIndex)
{
    BlameLineInfo info;

    if (currentIndex >= blameLines.size()) {
        return info;
    }

    // 第一行包含哈希和行号信息
    const QString firstLine = blameLines[currentIndex];
    const QStringList parts = firstLine.split(' ', Qt::SkipEmptyParts);

    if (parts.size() >= 3) {
        info.hash = parts[0];
        // parts[1] 是原始行号，parts[2] 是最终行号
    }

    currentIndex++;

    // 解析后续的元数据行
    while (currentIndex < blameLines.size()) {
        const QString line = blameLines[currentIndex];

        if (line.startsWith("author ")) {
            info.author = line.mid(7);   // 去掉 "author " 前缀
        } else if (line.startsWith("author-time ")) {
            bool ok;
            qint64 timestamp = line.mid(12).toLongLong(&ok);
            if (ok) {
                info.timestamp = QDateTime::fromSecsSinceEpoch(timestamp);
            }
        } else if (line.startsWith("summary ")) {
            info.fullCommitMessage = line.mid(8);   // 去掉 "summary " 前缀
        } else if (line.startsWith('\t')) {
            // 这是实际的代码行，以tab开头
            info.lineContent = line.mid(1);   // 去掉开头的tab
            currentIndex++;
            break;
        }

        currentIndex++;
    }

    return info;
}

void GitBlameDialog::formatBlameDisplay()
{
    QStringList displayLines;

    // 初始化作者颜色映射
    QHash<QString, QColor> authorColors;
    QList<QColor> colorPalette = {
        QColor(255, 239, 219),   // 浅橙色
        QColor(219, 255, 239),   // 浅绿色
        QColor(239, 219, 255),   // 浅紫色
        QColor(255, 219, 239),   // 浅粉色
        QColor(219, 239, 255),   // 浅蓝色
        QColor(255, 255, 219),   // 浅黄色
        QColor(239, 255, 219),   // 浅青色
        QColor(255, 219, 219),   // 浅红色
    };

    QSet<QString> authors;
    for (const auto &info : m_blameData) {
        authors.insert(info.author);
    }

    int colorIndex = 0;
    for (const QString &author : authors) {
        authorColors[author] = colorPalette[colorIndex % colorPalette.size()];
        colorIndex++;
    }

    for (int i = 0; i < m_blameData.size(); ++i) {
        const BlameLineInfo &info = m_blameData[i];

        // 格式化显示：行号 | 哈希(超链接) | 作者 | 时间 | 代码
        QString hashDisplay = info.hash.left(HASH_DISPLAY_LENGTH);
        QString authorDisplay = info.author.left(AUTHOR_DISPLAY_LENGTH);
        if (authorDisplay.length() < AUTHOR_DISPLAY_LENGTH) {
            authorDisplay = authorDisplay.leftJustified(AUTHOR_DISPLAY_LENGTH, ' ');
        }

        QString timeDisplay = info.timestamp.toString("MM-dd hh:mm");
        if (timeDisplay.length() < TIME_DISPLAY_LENGTH) {
            timeDisplay = timeDisplay.leftJustified(TIME_DISPLAY_LENGTH, ' ');
        }

        // 创建超链接格式的哈希
        QString hashLink = QString("<a href=\"%1\" style=\"color: #0066cc; text-decoration: underline;\">%2</a>")
                                   .arg(info.hash)   // 完整哈希作为链接
                                   .arg(hashDisplay);   // 显示的短哈希

        // 获取作者背景色
        QColor bgColor = authorColors.value(info.author, QColor(240, 240, 240));

        // 如果是选中行，使用高亮颜色
        if (i == m_currentSelectedLine) {
            bgColor = QColor(255, 255, 0, 120);   // 半透明黄色高亮
        }

        QString bgColorStr = QString("rgb(%1, %2, %3)")
                                     .arg(bgColor.red())
                                     .arg(bgColor.green())
                                     .arg(bgColor.blue());

        QString line = QString("<div style=\"background-color: %6; font-family: 'Courier', monospace; padding: 2px; margin: 0; white-space: pre;\">%1 | %2 | %3 | %4 | %5</div>")
                               .arg(info.lineNumber, 4)
                               .arg(hashLink)
                               .arg(authorDisplay.toHtmlEscaped())
                               .arg(timeDisplay)
                               .arg(info.lineContent.toHtmlEscaped())
                               .arg(bgColorStr);

        displayLines.append(line);
    }

    // 使用HTML格式设置内容，不包装额外的容器
    QString htmlContent = displayLines.join("");
    m_blameTextEdit->setHtml(htmlContent);
}

void GitBlameDialog::onRefreshClicked()
{
    qDebug() << "[GitBlameDialog] Refreshing blame data";
    loadBlameData();
}

void GitBlameDialog::keyPressEvent(QKeyEvent *event)
{
    // 如果还没有选中任何行，默认选中第一行
    if (m_currentSelectedLine < 0 && !m_blameData.isEmpty()) {
        m_currentSelectedLine = 0;
    }

    switch (event->key()) {
    case Qt::Key_Up:
        if (m_currentSelectedLine > 0) {
            m_currentSelectedLine--;
            highlightSelectedLine();
        }
        break;

    case Qt::Key_Down:
        if (m_currentSelectedLine < m_blameData.size() - 1) {
            m_currentSelectedLine++;
            highlightSelectedLine();
        }
        break;

    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_currentSelectedLine >= 0 && m_currentSelectedLine < m_blameData.size()) {
            const QString hash = m_blameData[m_currentSelectedLine].hash;
            if (!hash.isEmpty()) {
                showCommitDetails(hash);
            }
        }
        break;

    default:
        QDialog::keyPressEvent(event);
        break;
    }
}

void GitBlameDialog::highlightSelectedLine()
{
    if (m_currentSelectedLine >= 0 && m_currentSelectedLine < m_blameData.size()) {
        // 重新格式化显示，带选中行高亮
        formatBlameDisplay();

        // 滚动到选中行
        QTextCursor cursor = m_blameTextEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        for (int i = 0; i < m_currentSelectedLine; i++) {
            cursor.movePosition(QTextCursor::Down);
        }
        m_blameTextEdit->setTextCursor(cursor);
        m_blameTextEdit->ensureCursorVisible();
    }
}

void GitBlameDialog::showCommitDetails(const QString &hash)
{
    if (hash.isEmpty()) {
        return;
    }

    qDebug() << "[GitBlameDialog] Showing commit details for:" << hash;

    // 保存当前的显示状态 - 使用更可靠的方法
    QString currentHtml = m_blameTextEdit->toHtml();
    QTextDocument *doc = m_blameTextEdit->document();

    // 确保有备份内容
    if (currentHtml.isEmpty() && !m_blameData.isEmpty()) {
        qDebug() << "[GitBlameDialog] Current HTML is empty, regenerating from blame data";
        formatBlameDisplay();
        currentHtml = m_blameTextEdit->toHtml();
    }

    qDebug() << "[GitBlameDialog] Backup HTML length:" << currentHtml.length();

    showCommitDetailsDialog(hash);

    // 检查内容是否被意外清空，如果是则恢复
    QString afterHtml = m_blameTextEdit->toHtml();
    qDebug() << "[GitBlameDialog] After dialog HTML length:" << afterHtml.length();

    if (afterHtml.isEmpty() || afterHtml.length() < currentHtml.length() / 2) {
        qDebug() << "[GitBlameDialog] Content appears to be cleared, restoring...";
        m_blameTextEdit->setHtml(currentHtml);
        qInfo() << "INFO: [GitBlameDialog::showCommitDetails] Restored blame content after commit details dialog";
    }
}

void GitBlameDialog::showCommitDetailsDialog(const QString &hash)
{
    // 创建提交详情对话框
    QDialog *commitDialog = new QDialog(this);
    commitDialog->setWindowTitle(tr("Commit Details - %1").arg(hash.left(8)));
    commitDialog->resize(1000, 700);
    commitDialog->setAttribute(Qt::WA_DeleteOnClose);

    // 主布局
    auto *mainLayout = new QVBoxLayout(commitDialog);

    // 信息标签
    auto *infoLabel = new QLabel(tr("Loading commit details..."));
    infoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 8px; border-radius: 4px; }");
    mainLayout->addWidget(infoLabel);

    // 创建分割器
    auto *splitter = new QSplitter(Qt::Vertical, commitDialog);

    // 上部分：提交信息
    auto *commitInfoEdit = new QTextEdit;
    commitInfoEdit->setReadOnly(true);
    commitInfoEdit->setMaximumHeight(200);
    commitInfoEdit->setFont(QFont("Consolas", 9));
    splitter->addWidget(commitInfoEdit);

    // 下部分：文件差异
    auto *diffEdit = new QTextEdit;
    diffEdit->setReadOnly(true);
    diffEdit->setFont(QFont("Consolas", 9));
    diffEdit->setLineWrapMode(QTextEdit::NoWrap);
    splitter->addWidget(diffEdit);

    // 设置分割器比例
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    mainLayout->addWidget(splitter);

    // 按钮布局
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    auto *closeButton = new QPushButton(tr("Close"));
    connect(closeButton, &QPushButton::clicked, commitDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    // 显示对话框
    commitDialog->show();

    // 加载提交详情
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);

    // 获取提交的完整信息
    QStringList args;
    args << "show"
         << "--format=fuller"
         << "--no-patch" << hash;

    process.start("git", args);
    if (process.waitForFinished(10000)) {
        const QString commitInfo = QString::fromUtf8(process.readAllStandardOutput());
        if (!commitInfo.isEmpty()) {
            commitInfoEdit->setPlainText(commitInfo);
            qInfo() << "INFO: [GitBlameDialog::showCommitDetailsDialog] Loaded commit info for" << hash.left(8);
        } else {
            commitInfoEdit->setPlainText(tr("No commit information available."));
            qWarning() << "WARNING: [GitBlameDialog::showCommitDetailsDialog] Empty commit info for" << hash;
        }
    } else {
        commitInfoEdit->setPlainText(tr("Failed to load commit information: %1").arg(process.errorString()));
        qCritical() << "ERROR: [GitBlameDialog::showCommitDetailsDialog] Failed to load commit info:" << process.errorString();
    }

    // 获取提交的文件差异
    QProcess diffProcess;
    diffProcess.setWorkingDirectory(m_repositoryPath);

    QStringList diffArgs;
    diffArgs << "show"
             << "--color=never" << hash;
    if (!m_filePath.isEmpty()) {
        // 如果有指定文件，只显示该文件的差异
        QDir repoDir(m_repositoryPath);
        QString relativePath = repoDir.relativeFilePath(m_filePath);
        diffArgs << "--" << relativePath;
        infoLabel->setText(tr("Commit: %1 - File: %2").arg(hash.left(8), relativePath));
    } else {
        infoLabel->setText(tr("Commit: %1 - All changes").arg(hash.left(8)));
    }

    diffProcess.start("git", diffArgs);
    if (diffProcess.waitForFinished(15000)) {
        const QString diffOutput = QString::fromUtf8(diffProcess.readAllStandardOutput());
        if (!diffOutput.isEmpty()) {
            diffEdit->setPlainText(diffOutput);

            // 应用语法高亮
            applyDiffSyntaxHighlighting(diffEdit);
            qInfo() << "INFO: [GitBlameDialog::showCommitDetailsDialog] Loaded diff for" << hash.left(8);
        } else {
            diffEdit->setPlainText(tr("No changes found for this commit."));
            qWarning() << "WARNING: [GitBlameDialog::showCommitDetailsDialog] Empty diff for" << hash;
        }
    } else {
        diffEdit->setPlainText(tr("Failed to load commit diff: %1").arg(diffProcess.errorString()));
        qCritical() << "ERROR: [GitBlameDialog::showCommitDetailsDialog] Failed to load diff:" << diffProcess.errorString();
    }
}

void GitBlameDialog::applyDiffSyntaxHighlighting(QTextEdit *textEdit)
{
    if (!textEdit || textEdit == m_blameTextEdit) {
        // 不要对主blame显示区域应用语法高亮
        return;
    }

    QTextDocument *doc = textEdit->document();
    if (!doc) {
        return;
    }

    QTextCursor cursor(doc);

    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);

    // 定义格式 - 参考 QtCreator 的配色方案
    QTextCharFormat addedFormat;
    addedFormat.setBackground(QColor(221, 255, 221));   // 浅绿背景
    addedFormat.setForeground(QColor(0, 128, 0));   // 深绿前景

    QTextCharFormat removedFormat;
    removedFormat.setBackground(QColor(255, 221, 221));   // 浅红背景
    removedFormat.setForeground(QColor(164, 0, 0));   // 深红前景

    QTextCharFormat contextFormat;
    contextFormat.setForeground(QColor(64, 64, 64));   // 深灰色

    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(128, 128, 128));
    headerFormat.setFontWeight(QFont::Bold);

    QTextCharFormat metaFormat;
    metaFormat.setForeground(QColor(0, 0, 255));   // 蓝色
    metaFormat.setFontWeight(QFont::Bold);

    QTextCharFormat filePathFormat;
    filePathFormat.setForeground(QColor(128, 0, 128));   // 紫色
    filePathFormat.setFontWeight(QFont::Bold);

    QTextCharFormat lineNumberFormat;
    lineNumberFormat.setForeground(QColor(135, 135, 135));
    lineNumberFormat.setBackground(QColor(245, 245, 245));

    // 逐行处理语法高亮
    while (!cursor.atEnd()) {
        cursor.select(QTextCursor::LineUnderCursor);
        QString line = cursor.selectedText();

        if (line.startsWith('+') && !line.startsWith("+++")) {
            // 添加的行
            cursor.setCharFormat(addedFormat);
        } else if (line.startsWith('-') && !line.startsWith("---")) {
            // 删除的行
            cursor.setCharFormat(removedFormat);
        } else if (line.startsWith("@@") && line.contains("@@")) {
            // 行号信息
            cursor.setCharFormat(lineNumberFormat);
        } else if (line.startsWith("+++") || line.startsWith("---")) {
            // 文件路径
            cursor.setCharFormat(filePathFormat);
        } else if (line.startsWith("commit ") || line.startsWith("Author:") || line.startsWith("AuthorDate:") || line.startsWith("Commit:") || line.startsWith("CommitDate:") || line.startsWith("Date:")) {
            // 提交元信息
            cursor.setCharFormat(metaFormat);
        } else if (line.startsWith("diff --git") || line.startsWith("index ") || line.contains(" files changed") || line.contains(" insertions") || line.contains(" deletions")) {
            // diff 头信息
            cursor.setCharFormat(headerFormat);
        } else if (line.startsWith(" ")) {
            // 上下文行（未修改的行）
            cursor.setCharFormat(contextFormat);
        }

        cursor.movePosition(QTextCursor::NextBlock);
    }

    cursor.endEditBlock();
}

int GitBlameDialog::getLineNumberFromPosition(const QPoint &pos)
{
    QTextCursor cursor = m_blameTextEdit->cursorForPosition(pos);
    return cursor.blockNumber();
}

QString GitBlameDialog::getCommitHashFromLine(int lineNumber)
{
    if (lineNumber >= 0 && lineNumber < m_blameData.size()) {
        return m_blameData[lineNumber].hash;
    }
    return QString();
}

void GitBlameDialog::onHashLinkClicked(const QUrl &url)
{
    QString hash = url.toString();
    if (!hash.isEmpty()) {
        qDebug() << "[GitBlameDialog] Hash link clicked:" << hash.left(8);

        // 确保链接点击不会触发QTextBrowser的默认导航行为
        // 通过延迟调用showCommitDetails来避免与QTextBrowser的内部处理冲突
        QTimer::singleShot(0, this, [this, hash]() {
            showCommitDetails(hash);
        });
    } else {
        qWarning() << "WARNING: [GitBlameDialog::onHashLinkClicked] Empty hash from link click";
    }
}
