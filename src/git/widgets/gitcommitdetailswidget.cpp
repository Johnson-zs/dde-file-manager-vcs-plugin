#include "gitcommitdetailswidget.h"

#include <QFont>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QTextBrowser>
#include <QScrollBar>
#include <QRegularExpression>
#include <QTimer>
#include <QStringList>

GitCommitDetailsWidget::GitCommitDetailsWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_detailsTextEdit(nullptr)
    , m_placeholderText(tr("Select a commit to view details..."))
{
    setupUI();
    setupStyling();
    setPlaceholder(m_placeholderText);

    qDebug() << "[GitCommitDetailsWidget] Initialized commit details widget";
}

void GitCommitDetailsWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // 使用QTextBrowser以支持链接点击
    m_detailsTextEdit = new QTextBrowser;
    m_detailsTextEdit->setReadOnly(true);
    m_detailsTextEdit->setMaximumHeight(300);
    m_detailsTextEdit->setAcceptRichText(true);
    m_detailsTextEdit->setOpenExternalLinks(false);  // 我们手动处理链接点击
    
    // === 修复链接点击问题：禁用QTextBrowser的默认导航行为 ===
    m_detailsTextEdit->setSource(QUrl());  // 清空source，防止导航
    m_detailsTextEdit->setSearchPaths(QStringList());  // 清空搜索路径
    
    // === 修复1：优化滚动条设置，避免内容遮挡 ===
    // 设置水平滚动条策略：仅在需要时显示，并且不占用内容空间
    m_detailsTextEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_detailsTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 设置文本换行模式，减少水平滚动条的出现
    m_detailsTextEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    
    // 设置边距，为滚动条预留空间
    m_detailsTextEdit->setContentsMargins(2, 2, 2, 2);

    // 连接链接点击信号
    connect(m_detailsTextEdit, &QTextBrowser::anchorClicked,
            this, &GitCommitDetailsWidget::onLinkClicked);

    m_mainLayout->addWidget(m_detailsTextEdit);
}

void GitCommitDetailsWidget::setupStyling()
{
    // 设置字体
    QFont detailsFont("Consolas", 9);
    m_detailsTextEdit->setFont(detailsFont);

    // === 修复2：改进样式，为滚动条预留空间 ===
    m_detailsTextEdit->setStyleSheet(
        "QTextBrowser {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "    padding-bottom: 20px;"  // 为水平滚动条预留底部空间
        "}"
        "QTextBrowser QScrollBar:horizontal {"
        "    height: 12px;"  // 设置水平滚动条高度
        "    background-color: #f1f3f4;"
        "    border-radius: 6px;"
        "    margin: 2px;"
        "}"
        "QTextBrowser QScrollBar::handle:horizontal {"
        "    background-color: #c1c1c1;"
        "    border-radius: 5px;"
        "    min-width: 20px;"
        "}"
        "QTextBrowser QScrollBar::handle:horizontal:hover {"
        "    background-color: #a8a8a8;"
        "}"
        "QTextBrowser QScrollBar:vertical {"
        "    width: 12px;"
        "    background-color: #f1f3f4;"
        "    border-radius: 6px;"
        "    margin: 2px;"
        "}"
        "QTextBrowser QScrollBar::handle:vertical {"
        "    background-color: #c1c1c1;"
        "    border-radius: 5px;"
        "    min-height: 20px;"
        "}"
        "QTextBrowser QScrollBar::handle:vertical:hover {"
        "    background-color: #a8a8a8;"
        "}");
}

void GitCommitDetailsWidget::setCommitDetails(const QString &details)
{
    m_currentDetailsText = details;
    
    // === 修复3：自动将URL转换为超链接 ===
    QString htmlContent = convertTextToHtmlWithLinks(details);
    m_detailsTextEdit->setHtml(htmlContent);
    
    qDebug() << "[GitCommitDetailsWidget] Set commit details with auto-link conversion";
}

void GitCommitDetailsWidget::setCommitDetailsHtml(const QString &htmlContent)
{
    m_detailsTextEdit->setHtml(htmlContent);
    
    qDebug() << "[GitCommitDetailsWidget] Set commit details (HTML mode)";
}

void GitCommitDetailsWidget::setCommitSummaryStats(int filesChanged, int additions, int deletions)
{
    // 创建汇总统计信息
    QString summaryStats = formatCommitSummaryStats(filesChanged, additions, deletions);
    
    // === 修复4：为详情文本添加链接转换 ===
    QString detailsHtml = convertTextToHtmlWithLinks(m_currentDetailsText);
    
    // 组合HTML内容
    QString htmlContent = summaryStats + 
                         "<hr style='border: 1px solid #ccc; margin: 10px 0;'>" + 
                         detailsHtml;
    
    m_detailsTextEdit->setHtml(htmlContent);
    
    qInfo() << QString("INFO: [GitCommitDetailsWidget] Updated commit summary: %1 files, +%2 -%3")
                       .arg(filesChanged)
                       .arg(additions)
                       .arg(deletions);
}

void GitCommitDetailsWidget::clear()
{
    m_currentDetailsText.clear();
    setPlaceholder(m_placeholderText);
}

void GitCommitDetailsWidget::setPlaceholder(const QString &placeholder)
{
    m_placeholderText = placeholder;
    m_detailsTextEdit->setPlainText(placeholder);
}

QString GitCommitDetailsWidget::getDetailsText() const
{
    return m_currentDetailsText;
}

void GitCommitDetailsWidget::onLinkClicked(const QUrl &link)
{
    qDebug() << "[GitCommitDetailsWidget] Link clicked:" << link.toString();
    
    // === 修复：防止链接点击清空内容 ===
    // 保存当前的HTML内容
    QString currentHtml = m_detailsTextEdit->toHtml();
    
    // 发出信号让父组件处理
    Q_EMIT linkClicked(link.toString());
    
    // 默认行为：用系统默认应用打开链接
    if (link.scheme() == "http" || link.scheme() == "https") {
        QDesktopServices::openUrl(link);
    }
    
    // === 关键修复：确保内容不会被清空 ===
    // 如果内容被意外清空，恢复之前保存的内容
    QTimer::singleShot(50, this, [this, currentHtml]() {
        if (m_detailsTextEdit && m_detailsTextEdit->toPlainText().trimmed().isEmpty()) {
            qWarning() << "WARNING: [GitCommitDetailsWidget] Content was cleared after link click, restoring...";
            m_detailsTextEdit->setHtml(currentHtml);
        }
    });
}

QString GitCommitDetailsWidget::formatCommitSummaryStats(int filesChanged, int additions, int deletions) const
{
    QString result = "<div style='font-family: Arial, sans-serif; font-size: 10pt; margin-bottom: 8px;'>";
    result += "<b>📊 Commit Summary:</b><br>";
    result += QString("Files changed: <b>%1</b><br>").arg(filesChanged);

    if (additions > 0 || deletions > 0) {
        result += "Changes: ";
        if (additions > 0) {
            result += QString("<span style='color: #28a745; font-weight: bold;'>+%1</span>").arg(additions);
        }
        if (deletions > 0) {
            if (additions > 0) {
                result += " ";
            }
            result += QString("<span style='color: #dc3545; font-weight: bold;'>-%1</span>").arg(deletions);
        }
    } else {
        result += "No line changes<br>";
    }

    result += "</div>";
    return result;
}

// === 新增方法：将纯文本转换为HTML并自动识别链接 ===
QString GitCommitDetailsWidget::convertTextToHtmlWithLinks(const QString &text) const
{
    if (text.isEmpty()) {
        return QString();
    }

    QString result = "<div style='font-family: Consolas, monospace; font-size: 9pt; white-space: pre-wrap; word-wrap: break-word;'>";
    
    // 转义HTML特殊字符，但保留换行符
    QString escapedText = text.toHtmlEscaped();
    
    // === 关键功能：使用正则表达式识别URL并转换为链接 ===
    // 匹配HTTP/HTTPS URL的正则表达式
    QRegularExpression urlRegex(
        R"((https?://[^\s<>"{}|\\^`\[\]]+))",
        QRegularExpression::CaseInsensitiveOption
    );
    
    // 替换所有匹配的URL为HTML链接
    escapedText.replace(urlRegex, R"(<a href="\1" style="color: #0066cc; text-decoration: underline;">\1</a>)");
    
    // 匹配邮箱地址
    QRegularExpression emailRegex(
        R"(([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}))",
        QRegularExpression::CaseInsensitiveOption
    );
    
    // 替换邮箱为mailto链接
    escapedText.replace(emailRegex, R"(<a href="mailto:\1" style="color: #0066cc; text-decoration: underline;">\1</a>)");
    
    // 匹配Git提交哈希（7-40个十六进制字符）
    QRegularExpression hashRegex(
        R"(\b([a-f0-9]{7,40})\b)",
        QRegularExpression::CaseInsensitiveOption
    );
    
    // 为Git哈希添加样式（但不转换为链接，因为需要更多上下文信息）
    escapedText.replace(hashRegex, R"(<span style="font-family: monospace; background-color: #f6f8fa; padding: 2px 4px; border-radius: 3px; color: #0366d6;">\1</span>)");
    
    result += escapedText;
    result += "</div>";
    
    return result;
} 