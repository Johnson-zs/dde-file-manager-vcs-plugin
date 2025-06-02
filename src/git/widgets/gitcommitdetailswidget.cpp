#include "gitcommitdetailswidget.h"

#include <QFont>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QTextBrowser>

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

    // 设置样式
    m_detailsTextEdit->setStyleSheet(
        "QTextEdit {"
        "    background-color: #f8f9fa;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 8px;"
        "}");
}

void GitCommitDetailsWidget::setCommitDetails(const QString &details)
{
    m_currentDetailsText = details;
    m_detailsTextEdit->setPlainText(details);
    
    qDebug() << "[GitCommitDetailsWidget] Set commit details (plain text mode)";
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
    
    // 组合HTML内容
    QString htmlContent = summaryStats + 
                         "<hr style='border: 1px solid #ccc; margin: 10px 0;'>" + 
                         "<pre style='font-family: Consolas, monospace; font-size: 9pt; margin: 0;'>" + 
                         m_currentDetailsText.toHtmlEscaped() + 
                         "</pre>";
    
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
    
    // 发出信号让父组件处理
    Q_EMIT linkClicked(link.toString());
    
    // 默认行为：用系统默认应用打开链接
    if (link.scheme() == "http" || link.scheme() == "https") {
        QDesktopServices::openUrl(link);
    }
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