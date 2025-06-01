#include "markdownrenderer.h"
#include "linenumbertextedit.h"

#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileInfo>
#include <QDebug>
#include <QTextDocument>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

/**
 * @brief Markdown 语法高亮器
 */
class MarkdownSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit MarkdownSyntaxHighlighter(QTextDocument *parent = nullptr)
        : QSyntaxHighlighter(parent)
    {
        setupFormats();
        setupRules();
    }

protected:
    void highlightBlock(const QString &text) override
    {
        for (const auto &rule : m_highlightingRules) {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    void setupFormats()
    {
        // 标题格式
        m_headerFormat.setForeground(QColor(0, 0, 255));
        m_headerFormat.setFontWeight(QFont::Bold);
        m_headerFormat.setFontPointSize(14);

        // 粗体格式
        m_boldFormat.setForeground(QColor(0, 0, 0));
        m_boldFormat.setFontWeight(QFont::Bold);

        // 斜体格式
        m_italicFormat.setForeground(QColor(0, 0, 0));
        m_italicFormat.setFontItalic(true);

        // 代码格式
        m_codeFormat.setForeground(QColor(139, 69, 19));
        m_codeFormat.setBackground(QColor(245, 245, 245));
        m_codeFormat.setFontFamilies(QStringList{"Consolas", "Monaco", "monospace"});

        // 链接格式
        m_linkFormat.setForeground(QColor(0, 0, 255));
        m_linkFormat.setFontUnderline(true);

        // 列表格式
        m_listFormat.setForeground(QColor(128, 128, 128));
    }

    void setupRules()
    {
        HighlightingRule rule;

        // 标题 (# ## ### #### ##### ######)
        rule.pattern = QRegularExpression("^#{1,6}\\s+.*$");
        rule.format = m_headerFormat;
        m_highlightingRules.append(rule);

        // 粗体 (**text** 或 __text__)
        rule.pattern = QRegularExpression("(\\*\\*|__)(.*?)(\\*\\*|__)");
        rule.format = m_boldFormat;
        m_highlightingRules.append(rule);

        // 斜体 (*text* 或 _text_)
        rule.pattern = QRegularExpression("(\\*|_)(.*?)(\\*|_)");
        rule.format = m_italicFormat;
        m_highlightingRules.append(rule);

        // 行内代码 (`code`)
        rule.pattern = QRegularExpression("`([^`]+)`");
        rule.format = m_codeFormat;
        m_highlightingRules.append(rule);

        // 代码块 (```code```)
        rule.pattern = QRegularExpression("```[\\s\\S]*?```");
        rule.format = m_codeFormat;
        m_highlightingRules.append(rule);

        // 链接 [text](url)
        rule.pattern = QRegularExpression("\\[([^\\]]+)\\]\\(([^\\)]+)\\)");
        rule.format = m_linkFormat;
        m_highlightingRules.append(rule);

        // 列表项 (- * +)
        rule.pattern = QRegularExpression("^\\s*[-\\*\\+]\\s+");
        rule.format = m_listFormat;
        m_highlightingRules.append(rule);

        // 有序列表 (1. 2. 3.)
        rule.pattern = QRegularExpression("^\\s*\\d+\\.\\s+");
        rule.format = m_listFormat;
        m_highlightingRules.append(rule);
    }

    QVector<HighlightingRule> m_highlightingRules;

    QTextCharFormat m_headerFormat;
    QTextCharFormat m_boldFormat;
    QTextCharFormat m_italicFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_listFormat;
};

// ============================================================================
// MarkdownRenderer Implementation
// ============================================================================

MarkdownRenderer::MarkdownRenderer()
    : m_showRenderedView(true)
    , m_stackedWidget(nullptr)
    , m_htmlBrowser(nullptr)
    , m_sourceEditor(nullptr)
{
    qInfo() << "INFO: [MarkdownRenderer] Markdown renderer initialized";
}

bool MarkdownRenderer::canRender(const QString &filePath) const
{
    return isMarkdownFile(filePath);
}

QWidget* MarkdownRenderer::createWidget(QWidget *parent)
{
    if (m_stackedWidget) {
        qWarning() << "WARNING: [MarkdownRenderer] Widget already created";
        return m_stackedWidget;
    }

    // 创建堆叠窗口
    m_stackedWidget = new QStackedWidget(parent);

    // 创建 HTML 浏览器用于渲染视图
    m_htmlBrowser = new QTextBrowser(m_stackedWidget);
    m_htmlBrowser->setOpenExternalLinks(true);
    m_htmlBrowser->setStyleSheet(
        "QTextBrowser {"
        "    background-color: #ffffff;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    padding: 12px;"
        "    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "    font-size: 14px;"
        "    line-height: 1.6;"
        "}"
    );

    // 创建源码编辑器
    m_sourceEditor = new LineNumberTextEdit(m_stackedWidget);
    m_sourceEditor->setReadOnly(true);
    m_sourceEditor->setFont(QFont("Consolas", 10));
    m_sourceEditor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_sourceEditor->setStyleSheet(
        "LineNumberTextEdit {"
        "    background-color: #ffffff;"
        "    border: 1px solid #dee2e6;"
        "    border-radius: 4px;"
        "    selection-background-color: #007acc;"
        "    selection-color: white;"
        "}"
    );

    // 设置语法高亮
    setupSyntaxHighlighter();

    // 添加到堆叠窗口
    m_stackedWidget->addWidget(m_htmlBrowser);
    m_stackedWidget->addWidget(m_sourceEditor);

    // 默认显示渲染视图
    m_stackedWidget->setCurrentWidget(m_showRenderedView ? static_cast<QWidget*>(m_htmlBrowser) : static_cast<QWidget*>(m_sourceEditor));

    qDebug() << "[MarkdownRenderer] Widget created successfully";
    return m_stackedWidget;
}

void MarkdownRenderer::setContent(const QString &content)
{
    m_content = content;

    if (!m_stackedWidget) {
        qWarning() << "WARNING: [MarkdownRenderer] Widget not created yet";
        return;
    }

    // 使用 Qt6 原生 Markdown 支持设置 HTML 浏览器内容
    if (m_htmlBrowser) {
        QTextDocument *document = m_htmlBrowser->document();
        
        // 使用 Qt6 的原生 Markdown 解析，支持 GitHub 风格
        document->setMarkdown(content, QTextDocument::MarkdownDialectGitHub);
        
        qDebug() << "[MarkdownRenderer] Content set using Qt6 native Markdown support";
    }

    // 设置源码编辑器内容
    if (m_sourceEditor) {
        m_sourceEditor->setPlainText(content);
        qDebug() << "[MarkdownRenderer] Source content set";
    }
}

QString MarkdownRenderer::getRendererType() const
{
    return "Markdown";
}

bool MarkdownRenderer::supportsViewToggle() const
{
    return true;
}

void MarkdownRenderer::toggleViewMode()
{
    if (!m_stackedWidget) {
        qWarning() << "WARNING: [MarkdownRenderer] Widget not created";
        return;
    }

    m_showRenderedView = !m_showRenderedView;
    m_stackedWidget->setCurrentWidget(m_showRenderedView ? static_cast<QWidget*>(m_htmlBrowser) : static_cast<QWidget*>(m_sourceEditor));

    qDebug() << "[MarkdownRenderer] View mode toggled to:" 
             << (m_showRenderedView ? "Rendered" : "Source");
}

QString MarkdownRenderer::getCurrentViewModeDescription() const
{
    return m_showRenderedView ? "Show Source" : "Show Rendered";
}

bool MarkdownRenderer::isMarkdownFile(const QString &filePath) const
{
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();
    QString fileName = fileInfo.fileName().toLower();

    return (suffix == "md" || suffix == "markdown" || suffix == "mdown" ||
            fileName == "readme" || fileName == "readme.md" || fileName == "readme.txt");
}

void MarkdownRenderer::setupSyntaxHighlighter()
{
    if (m_sourceEditor && m_sourceEditor->document()) {
        m_syntaxHighlighter = std::make_unique<MarkdownSyntaxHighlighter>(m_sourceEditor->document());
        qDebug() << "[MarkdownRenderer] Syntax highlighter setup completed";
    }
}

#include "markdownrenderer.moc" 