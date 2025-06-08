#ifndef GITCOMMITDETAILSWIDGET_H
#define GITCOMMITDETAILSWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QTextBrowser>
#include <QUrl>
#include <QString>

/**
 * @brief 可重用的提交详情展示组件
 * 
 * 这个组件被设计为可以在多个地方复用，包括：
 * - GitLogDialog的右侧详情面板
 * - 其他需要显示提交详情的界面
 * 
 * 主要功能：
 * - 显示提交的详细信息（作者、时间、消息等）
 * - 显示提交统计信息（文件数、添加/删除行数）
 * - 支持HTML格式的富文本显示
 * - 支持链接点击处理
 */
class GitCommitDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GitCommitDetailsWidget(QWidget *parent = nullptr);
    ~GitCommitDetailsWidget() = default;

    /**
     * @brief 设置提交详情（纯文本格式）
     * @param details git show --format=fuller输出的详情文本
     */
    void setCommitDetails(const QString &details);

    /**
     * @brief 设置提交详情（HTML格式）
     * @param htmlContent 格式化的HTML内容
     */
    void setCommitDetailsHtml(const QString &htmlContent);

    /**
     * @brief 添加提交统计信息
     * @param filesChanged 变更的文件数
     * @param additions 新增行数
     * @param deletions 删除行数
     */
    void setCommitSummaryStats(int filesChanged, int additions, int deletions);

    /**
     * @brief 清空显示内容
     */
    void clear();

    /**
     * @brief 设置占位符文本
     * @param placeholder 当没有选中提交时显示的文本
     */
    void setPlaceholder(const QString &placeholder);

    /**
     * @brief 获取当前显示的详情文本
     */
    QString getDetailsText() const;

Q_SIGNALS:
    /**
     * @brief 当用户点击提交中的链接时发出
     * @param url 点击的链接地址
     */
    void linkClicked(const QString &url);

private Q_SLOTS:
    void onLinkClicked(const QUrl &link);

private:
    void setupUI();
    void setupStyling();
    QString formatCommitSummaryStats(int filesChanged, int additions, int deletions) const;
    
    /**
     * @brief 将纯文本转换为HTML格式，并自动识别URL转换为超链接
     * @param text 输入的纯文本
     * @return 包含链接的HTML格式文本
     */
    QString convertTextToHtmlWithLinks(const QString &text) const;

    QVBoxLayout *m_mainLayout;
    QTextBrowser *m_detailsTextEdit;
    
    QString m_placeholderText;
    QString m_currentDetailsText;   // 缓存当前的纯文本内容
};

#endif // GITCOMMITDETAILSWIDGET_H 