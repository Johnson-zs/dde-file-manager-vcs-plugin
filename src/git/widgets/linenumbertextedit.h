#ifndef LINENUMBERTEXTEDIT_H
#define LINENUMBERTEXTEDIT_H

#include <QPlainTextEdit>
#include <QWidget>

class QPaintEvent;
class QResizeEvent;

/**
 * @brief 带行号显示的文本编辑器
 * 
 * 基于QPlainTextEdit实现，在左侧显示行号，支持：
 * - 自动行号更新
 * - 行号区域自适应宽度
 * - 语法高亮兼容
 * - 滚动同步
 */
class LineNumberTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LineNumberTextEdit(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *m_lineNumberArea;
};

/**
 * @brief 行号显示区域
 */
class LineNumberArea : public QWidget
{
    Q_OBJECT

public:
    explicit LineNumberArea(LineNumberTextEdit *editor);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    LineNumberTextEdit *m_codeEditor;
};

#endif // LINENUMBERTEXTEDIT_H 