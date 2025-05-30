#include "gitblamedialog.h"
#include "utils.h"
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

// 静态成员初始化
QList<QColor> GitBlameSyntaxHighlighter::s_authorColorPalette = {
    QColor(255, 239, 219),  // 浅橙色
    QColor(219, 255, 239),  // 浅绿色  
    QColor(239, 219, 255),  // 浅紫色
    QColor(255, 219, 239),  // 浅粉色
    QColor(219, 239, 255),  // 浅蓝色
    QColor(255, 255, 219),  // 浅黄色
    QColor(239, 255, 219),  // 浅青色
    QColor(255, 219, 219),  // 浅红色
};

int GitBlameSyntaxHighlighter::s_nextColorIndex = 0;

GitBlameSyntaxHighlighter::GitBlameSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , m_highlightLine(-1)
{
    // 初始化格式
    m_lineNumberFormat.setForeground(QColor(128, 128, 128));
    m_lineNumberFormat.setFontWeight(QFont::Normal);
    
    m_hashFormat.setForeground(QColor(0, 102, 204));
    m_hashFormat.setFontWeight(QFont::Bold);
    m_hashFormat.setFontUnderline(true);
    
    m_authorFormat.setForeground(QColor(51, 51, 51));
    m_authorFormat.setFontWeight(QFont::Normal);
    
    m_timeFormat.setForeground(QColor(102, 102, 102));
    m_timeFormat.setFontWeight(QFont::Normal);
    
    m_codeFormat.setForeground(QColor(0, 0, 0));
    m_codeFormat.setFontFamilies(QStringList{"Courier", "monospace"});
    
    m_highlightFormat.setBackground(QColor(255, 255, 0, 80)); // 半透明黄色
}

void GitBlameSyntaxHighlighter::setBlameData(const QVector<BlameLineInfo> &blameData)
{
    m_blameData = blameData;
    initializeAuthorColors();
    rehighlight();
}

void GitBlameSyntaxHighlighter::setHighlightLine(int lineNumber)
{
    m_highlightLine = lineNumber;
    rehighlight();
}

void GitBlameSyntaxHighlighter::initializeAuthorColors()
{
    m_authorColors.clear();
    QSet<QString> authors;
    
    for (const auto &info : m_blameData) {
        authors.insert(info.author);
    }
    
    int colorIndex = 0;
    for (const QString &author : authors) {
        m_authorColors[author] = s_authorColorPalette[colorIndex % s_authorColorPalette.size()];
        colorIndex++;
    }
}

QColor GitBlameSyntaxHighlighter::getAuthorColor(const QString &author)
{
    return m_authorColors.value(author, QColor(240, 240, 240));
}

void GitBlameSyntaxHighlighter::highlightBlock(const QString &text)
{
    const int blockNumber = currentBlock().blockNumber();
    
    if (blockNumber < m_blameData.size()) {
        const BlameLineInfo &info = m_blameData[blockNumber];
        
        // 设置整行背景色（基于作者）
        QTextCharFormat blockFormat;
        blockFormat.setBackground(getAuthorColor(info.author));
        setFormat(0, text.length(), blockFormat);
        
        // 如果是高亮行，添加高亮效果
        if (blockNumber == m_highlightLine) {
            QTextCharFormat highlightFormat = blockFormat;
            highlightFormat.setBackground(m_highlightFormat.background());
            setFormat(0, text.length(), highlightFormat);
        }
        
        // 解析行内容格式：行号 | 哈希 | 作者 | 时间 | 代码
        QRegularExpression pattern(R"(^\s*(\d+)\s*\|\s*([a-f0-9]+)\s*\|\s*(.+?)\s*\|\s*(.+?)\s*\|\s*(.*)$)");
        QRegularExpressionMatch match = pattern.match(text);
        
        if (match.hasMatch()) {
            int pos = 0;
            
            // 行号
            QString lineNum = match.captured(1);
            int lineNumEnd = text.indexOf('|');
            if (lineNumEnd > 0) {
                setFormat(pos, lineNumEnd, m_lineNumberFormat);
                pos = lineNumEnd + 1;
            }
            
            // 哈希
            QString hash = match.captured(2);
            int hashEnd = text.indexOf('|', pos);
            if (hashEnd > pos) {
                setFormat(pos, hashEnd - pos, m_hashFormat);
                pos = hashEnd + 1;
            }
            
            // 作者
            QString author = match.captured(3);
            int authorEnd = text.indexOf('|', pos);
            if (authorEnd > pos) {
                setFormat(pos, authorEnd - pos, m_authorFormat);
                pos = authorEnd + 1;
            }
            
            // 时间
            QString time = match.captured(4);
            int timeEnd = text.indexOf('|', pos);
            if (timeEnd > pos) {
                setFormat(pos, timeEnd - pos, m_timeFormat);
                pos = timeEnd + 1;
            }
            
            // 代码部分
            if (pos < text.length()) {
                setFormat(pos, text.length() - pos, m_codeFormat);
            }
        }
    }
}

GitBlameDialog::GitBlameDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_repositoryPath(repositoryPath)
    , m_filePath(filePath)
    , m_filePathLabel(nullptr)
    , m_blameTextEdit(nullptr)
    , m_highlighter(nullptr)
    , m_refreshButton(nullptr)
    , m_closeButton(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
{
    m_fileName = QFileInfo(filePath).fileName();
    setWindowTitle(tr("Git Blame - %1").arg(m_fileName));
    setMinimumSize(1200, 800);
    // Enable maximize button and better default size
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint);
    resize(1400, 900);
    setAttribute(Qt::WA_DeleteOnClose);
    
    setupUI();
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
    
    mainLayout->addWidget(headerGroup);

    // === 进度条 ===
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);

    // === Blame显示区域 ===
    auto *blameGroup = new QGroupBox(tr("Blame Information"), this);
    auto *blameLayout = new QVBoxLayout(blameGroup);
    
    m_blameTextEdit = new QTextEdit(this);
    m_blameTextEdit->setReadOnly(true);
    m_blameTextEdit->setFont(QFont("Courier", 10));
    m_blameTextEdit->setLineWrapMode(QTextEdit::NoWrap);
    
    // 应用语法高亮
    m_highlighter = new GitBlameSyntaxHighlighter(m_blameTextEdit->document());
    
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

void GitBlameDialog::loadBlameData()
{
    m_blameData.clear();
    m_blameTextEdit->clear();
    m_statusLabel->setText(tr("Loading blame information..."));
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // 不确定进度
    
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
    args << "blame" << "--line-porcelain" << relativePath;
    
    process.start("git", args);
    if (!process.waitForFinished(30000)) { // 30秒超时
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
    m_highlighter->setBlameData(m_blameData);
    
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
            info.author = line.mid(7); // 去掉 "author " 前缀
        } else if (line.startsWith("author-time ")) {
            bool ok;
            qint64 timestamp = line.mid(12).toLongLong(&ok);
            if (ok) {
                info.timestamp = QDateTime::fromSecsSinceEpoch(timestamp);
            }
        } else if (line.startsWith("summary ")) {
            info.fullCommitMessage = line.mid(8); // 去掉 "summary " 前缀
        } else if (line.startsWith('\t')) {
            // 这是实际的代码行，以tab开头
            info.lineContent = line.mid(1); // 去掉开头的tab
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
    
    for (int i = 0; i < m_blameData.size(); ++i) {
        const BlameLineInfo &info = m_blameData[i];
        
        // 格式化显示：行号 | 哈希 | 作者 | 时间 | 代码
        QString hashDisplay = info.hash.left(HASH_DISPLAY_LENGTH);
        QString authorDisplay = info.author.left(AUTHOR_DISPLAY_LENGTH);
        if (authorDisplay.length() < AUTHOR_DISPLAY_LENGTH) {
            authorDisplay = authorDisplay.leftJustified(AUTHOR_DISPLAY_LENGTH, ' ');
        }
        
        QString timeDisplay = info.timestamp.toString("MM-dd hh:mm");
        if (timeDisplay.length() < TIME_DISPLAY_LENGTH) {
            timeDisplay = timeDisplay.leftJustified(TIME_DISPLAY_LENGTH, ' ');
        }
        
        QString line = QString("%1 | %2 | %3 | %4 | %5")
                       .arg(info.lineNumber, 4)
                       .arg(hashDisplay)
                       .arg(authorDisplay)
                       .arg(timeDisplay)
                       .arg(info.lineContent);
        
        displayLines.append(line);
    }
    
    m_blameTextEdit->setPlainText(displayLines.join('\n'));
}

void GitBlameDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPoint pos = event->pos();
        const QPoint textPos = m_blameTextEdit->mapFromParent(pos);
        
        if (m_blameTextEdit->rect().contains(textPos)) {
            const int lineNumber = getLineNumberFromPosition(textPos);
            if (lineNumber >= 0 && lineNumber < m_blameData.size()) {
                const QString hash = m_blameData[lineNumber].hash;
                showCommitDetails(hash);
            }
        }
    }
    
    QDialog::mousePressEvent(event);
}

bool GitBlameDialog::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent*>(event);
        const QPoint pos = helpEvent->pos();
        const QPoint textPos = m_blameTextEdit->mapFromParent(pos);
        
        if (m_blameTextEdit->rect().contains(textPos)) {
            const int lineNumber = getLineNumberFromPosition(textPos);
            if (lineNumber >= 0 && lineNumber < m_blameData.size()) {
                const BlameLineInfo &info = m_blameData[lineNumber];
                const QString tooltip = tr("Commit: %1\nAuthor: %2\nDate: %3\nMessage: %4")
                                       .arg(info.hash)
                                       .arg(info.author)
                                       .arg(info.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                                       .arg(info.fullCommitMessage);
                
                QToolTip::showText(helpEvent->globalPos(), tooltip);
                return true;
            }
        }
        
        QToolTip::hideText();
        event->ignore();
        return true;
    }
    
    return QDialog::event(event);
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

void GitBlameDialog::showCommitDetails(const QString &hash)
{
    if (hash.isEmpty()) {
        return;
    }
    
    qDebug() << "[GitBlameDialog] Showing commit details for:" << hash;
    
    QProcess process;
    process.setWorkingDirectory(m_repositoryPath);
    
    QStringList args;
    args << "show" << "--stat" << hash;
    
    process.start("git", args);
    if (process.waitForFinished(10000)) {
        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        
        if (!output.isEmpty()) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Commit Details - %1").arg(hash.left(8)));
            msgBox.setText(tr("Commit information:"));
            msgBox.setDetailedText(output);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
        } else {
            QMessageBox::information(this, tr("No Information"), 
                                   tr("No detailed information available for commit %1").arg(hash));
        }
    } else {
        QMessageBox::warning(this, tr("Error"), 
                           tr("Failed to get commit details: %1").arg(process.errorString()));
    }
}

void GitBlameDialog::onRefreshClicked()
{
    qDebug() << "[GitBlameDialog] Refreshing blame data";
    loadBlameData();
} 