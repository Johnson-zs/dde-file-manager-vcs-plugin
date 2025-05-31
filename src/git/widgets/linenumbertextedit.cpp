#include "linenumbertextedit.h"

#include <QPainter>
#include <QTextBlock>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QTextDocument>
#include <QDebug>

LineNumberTextEdit::LineNumberTextEdit(QWidget *parent)
    : QPlainTextEdit(parent)
{
    m_lineNumberArea = new LineNumberArea(this);

    // 连接信号槽
    connect(document(), &QTextDocument::blockCountChanged,
            this, &LineNumberTextEdit::updateLineNumberAreaWidth);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this](int) { updateLineNumberArea(QRect(), 0); });
    connect(this, &QPlainTextEdit::textChanged,
            this, [this]() { updateLineNumberArea(QRect(), 0); });
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &LineNumberTextEdit::highlightCurrentLine);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &LineNumberTextEdit::updateLineNumberArea);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    qDebug() << "[LineNumberTextEdit] Initialized with line number support";
}

int LineNumberTextEdit::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, document()->blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
#else
    int space = 3 + fontMetrics().width(QLatin1Char('9')) * digits;
#endif
    return space;
}

void LineNumberTextEdit::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void LineNumberTextEdit::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void LineNumberTextEdit::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void LineNumberTextEdit::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void LineNumberTextEdit::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(m_lineNumberArea);
    painter.fillRect(event->rect(), QColor(240, 240, 240));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(QColor(120, 120, 120));
            painter.drawText(0, top, m_lineNumberArea->width() - 3, fontMetrics().height(),
                           Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

// LineNumberArea implementation
LineNumberArea::LineNumberArea(LineNumberTextEdit *editor) : QWidget(editor), m_codeEditor(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(m_codeEditor->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    m_codeEditor->lineNumberAreaPaintEvent(event);
} 