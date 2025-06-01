#ifndef MARKDOWNRENDERER_H
#define MARKDOWNRENDERER_H

#include "filerenderer.h"
#include <QStackedWidget>
#include <QTextBrowser>
#include <QSyntaxHighlighter>
#include <memory>

// Forward declarations
class LineNumberTextEdit;

/**
 * @brief Markdown 文件渲染器
 * 
 * 专门用于渲染 Markdown 文件的渲染器实现。
 * 支持在渲染视图和源码视图之间切换。
 * 遵循单一职责原则。
 */
class MarkdownRenderer : public IFileRenderer
{
public:
    MarkdownRenderer();
    ~MarkdownRenderer() override = default;
    
    // IFileRenderer 接口实现
    bool canRender(const QString &filePath) const override;
    QWidget* createWidget(QWidget *parent = nullptr) override;
    void setContent(const QString &content) override;
    QString getRendererType() const override;
    bool supportsViewToggle() const override;
    void toggleViewMode() override;
    QString getCurrentViewModeDescription() const override;

private:
    /**
     * @brief 检查文件是否为 Markdown 类型
     * @param filePath 文件路径
     * @return 如果是 Markdown 文件返回 true，否则返回 false
     */
    bool isMarkdownFile(const QString &filePath) const;
    
    /**
     * @brief 将 Markdown 内容转换为 HTML
     * @param markdownContent Markdown 内容
     * @return 转换后的 HTML 内容
     */
    QString convertMarkdownToHtml(const QString &markdownContent) const;
    
    /**
     * @brief 设置语法高亮器
     */
    void setupSyntaxHighlighter();
    
    QString m_content;
    bool m_showRenderedView;
    
    // UI 组件（延迟创建）
    QStackedWidget *m_stackedWidget;
    QTextBrowser *m_htmlBrowser;
    LineNumberTextEdit *m_sourceEditor;
    std::unique_ptr<QSyntaxHighlighter> m_syntaxHighlighter;
};

#endif // MARKDOWNRENDERER_H 