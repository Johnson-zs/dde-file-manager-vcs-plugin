#ifndef GITBLAMEDIALOG_H
#define GITBLAMEDIALOG_H

#include <QDialog>
#include <QTextBrowser>
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
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QContextMenuEvent>

/**
 * @brief Git Blame信息结构
 */
struct BlameLineInfo
{
    QString hash;   // 提交哈希
    QString author;   // 作者
    QDateTime timestamp;   // 提交时间
    QString lineContent;   // 代码行内容
    QString fullCommitMessage;   // 完整提交消息
    int lineNumber;   // 行号
    bool isCommitStart;   // 是否是提交的第一行
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
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onRefreshClicked();
    void showCommitDetails(const QString &hash);
    void onShowCommitDetailsTriggered();
    void onHashLinkClicked(const QUrl &link);

private:
    void setupUI();
    void loadBlameData();
    void formatBlameDisplay();
    void updateProgressBar(int progress);
    BlameLineInfo parseBlameLineInfo(const QStringList &blameLines, int &currentIndex);
    int getLineNumberFromPosition(const QPoint &pos);
    QString getCommitHashFromLine(int lineNumber);
    void showCommitDetailsDialog(const QString &hash);
    void applyDiffSyntaxHighlighting(QTextEdit *textEdit);
    void setupContextMenu();
    void highlightSelectedLine();

    QString m_repositoryPath;
    QString m_filePath;
    QString m_fileName;

    // UI组件
    QLabel *m_filePathLabel;
    QTextBrowser *m_blameTextEdit;
    QPushButton *m_refreshButton;
    QPushButton *m_closeButton;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;

    // 右键菜单
    QMenu *m_contextMenu;
    QAction *m_showCommitDetailsAction;

    // 数据
    QVector<BlameLineInfo> m_blameData;

    // 当前选中的行
    int m_currentSelectedLine;

    // 常量
    static constexpr int HASH_DISPLAY_LENGTH = 8;   // 显示的哈希长度
    static constexpr int AUTHOR_DISPLAY_LENGTH = 15;   // 显示的作者名长度
    static constexpr int TIME_DISPLAY_LENGTH = 10;   // 显示的时间长度
};

#endif   // GITBLAMEDIALOG_H
