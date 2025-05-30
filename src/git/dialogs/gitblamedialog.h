#ifndef GITBLAMEDIALOG_H
#define GITBLAMEDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QHash>
#include <QVector>
#include <QMouseEvent>
#include <QToolTip>

/**
 * @brief Git Blame信息结构
 */
struct BlameLineInfo {
    QString hash;                   // 提交哈希
    QString author;                 // 作者
    QDateTime timestamp;            // 提交时间
    QString lineContent;            // 代码行内容
    QString fullCommitMessage;      // 完整提交消息
    int lineNumber;                 // 行号
    bool isCommitStart;             // 是否是提交的第一行
};

/**
 * @brief Git Blame语法高亮器
 * 
 * 为blame信息提供颜色编码和格式化显示
 */
class GitBlameSyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    explicit GitBlameSyntaxHighlighter(QTextDocument *parent = nullptr);
    
    void setBlameData(const QVector<BlameLineInfo> &blameData);
    void setHighlightLine(int lineNumber);

protected:
    void highlightBlock(const QString &text) override;

private:
    void initializeAuthorColors();
    QColor getAuthorColor(const QString &author);
    
    QVector<BlameLineInfo> m_blameData;
    QHash<QString, QColor> m_authorColors;  // 作者颜色映射
    QTextCharFormat m_lineNumberFormat;
    QTextCharFormat m_hashFormat;
    QTextCharFormat m_authorFormat;
    QTextCharFormat m_timeFormat;
    QTextCharFormat m_codeFormat;
    QTextCharFormat m_highlightFormat;      // 高亮行格式
    int m_highlightLine;                    // 要高亮的行号
    
    static QList<QColor> s_authorColorPalette;
    static int s_nextColorIndex;
};

/**
 * @brief Git Blame对话框
 * 
 * 实现GitHub风格的blame界面，显示每行代码的作者、时间和提交信息
 */
class GitBlameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitBlameDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;  // 处理ToolTip事件

private slots:
    void onRefreshClicked();
    void showCommitDetails(const QString &hash);

private:
    void setupUI();
    void loadBlameData();
    void formatBlameDisplay();
    void updateProgressBar(int progress);
    BlameLineInfo parseBlameLineInfo(const QStringList &blameLines, int &currentIndex);
    int getLineNumberFromPosition(const QPoint &pos);
    QString getCommitHashFromLine(int lineNumber);
    
    QString m_repositoryPath;
    QString m_filePath;
    QString m_fileName;
    
    // UI组件
    QLabel *m_filePathLabel;
    QTextEdit *m_blameTextEdit;
    GitBlameSyntaxHighlighter *m_highlighter;
    QPushButton *m_refreshButton;
    QPushButton *m_closeButton;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // 数据
    QVector<BlameLineInfo> m_blameData;
    
    // 常量
    static constexpr int HASH_DISPLAY_LENGTH = 8;      // 显示的哈希长度
    static constexpr int AUTHOR_DISPLAY_LENGTH = 15;   // 显示的作者名长度
    static constexpr int TIME_DISPLAY_LENGTH = 10;     // 显示的时间长度
};

#endif // GITBLAMEDIALOG_H 