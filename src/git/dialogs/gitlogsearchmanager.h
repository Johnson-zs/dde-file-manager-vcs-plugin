#ifndef GITLOGSEARCHMANAGER_H
#define GITLOGSEARCHMANAGER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>

/**
 * @brief Git日志搜索管理器
 * 
 * 专门负责Git日志的搜索和过滤功能：
 * - 实时搜索提交记录（仅遍历前1000条，最多高亮100条，极大提升性能）
 * - 渐进式搜索（加载更多结果）
 * - 搜索结果高亮显示
 * - 搜索状态管理
 */
class GitLogSearchManager : public QObject
{
    Q_OBJECT

public:
    explicit GitLogSearchManager(QTreeWidget *commitTree, QLabel *statusLabel, QObject *parent = nullptr);
    ~GitLogSearchManager() = default;

    // === 搜索接口 ===
    void startSearch(const QString &searchText);
    void stopSearch();
    void clearSearch();
    bool isSearching() const { return m_isSearching; }

    // === 状态获取 ===
    QString currentSearchText() const { return m_currentSearchText; }
    int searchResultsCount() const { return m_searchTotalFound; }
    bool hasActiveSearch() const { return !m_currentSearchText.isEmpty(); }

    // === 搜索配置 ===
    void setSearchDelay(int milliseconds) { m_searchDelay = milliseconds; }
    void setMinSearchLength(int length) { m_minSearchLength = length; }
    void setMaxResults(int maxResults) { m_maxSearchResults = maxResults; }

Q_SIGNALS:
    // 搜索状态信号
    void searchStarted(const QString &searchText);
    void searchProgress(const QString &searchText, int currentResults);
    void searchCompleted(const QString &searchText, int totalResults);
    void searchCleared();
    
    // 需要更多数据的信号
    void moreDataNeeded();

public Q_SLOTS:
    void onNewCommitsLoaded();  // 当新的提交数据加载完成时调用

private Q_SLOTS:
    void performSearch();
    void onSearchTimeout();

private:
    void filterCurrentCommits();
    void highlightSearchResults();
    void clearHighlights();
    void updateSearchStatus();
    void highlightItemMatches(QTreeWidgetItem *item, const QString &searchText);
    void clearItemHighlight(QTreeWidgetItem *item);
    bool itemMatchesSearch(QTreeWidgetItem *item, const QString &searchText) const;

    // UI组件引用
    QTreeWidget *m_commitTree;
    QLabel *m_statusLabel;

    // 搜索状态
    QString m_currentSearchText;
    bool m_isSearching;
    bool m_isLoadingMore;
    int m_searchTotalFound;

    // 定时器
    QTimer *m_searchTimer;
    QTimer *m_progressTimer;

    // 配置
    int m_searchDelay;         // 搜索延迟（毫秒）
    int m_minSearchLength;     // 最小搜索长度
    int m_maxSearchResults;    // 最大搜索结果数

    // 常量
    static const int DEFAULT_SEARCH_DELAY = 500;
    static const int DEFAULT_MIN_SEARCH_LENGTH = 2;
    static const int DEFAULT_MAX_SEARCH_RESULTS = 100;
};

#endif // GITLOGSEARCHMANAGER_H 