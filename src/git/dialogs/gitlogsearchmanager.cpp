#include "gitlogsearchmanager.h"

#include <QApplication>
#include <QColor>
#include <QBrush>
#include <QDebug>

GitLogSearchManager::GitLogSearchManager(QTreeWidget *commitTree, QLabel *statusLabel, QObject *parent)
    : QObject(parent)
    , m_commitTree(commitTree)
    , m_statusLabel(statusLabel)
    , m_isSearching(false)
    , m_isLoadingMore(false)
    , m_searchTotalFound(0)
    , m_searchDelay(DEFAULT_SEARCH_DELAY)
    , m_minSearchLength(DEFAULT_MIN_SEARCH_LENGTH)
    , m_maxSearchResults(DEFAULT_MAX_SEARCH_RESULTS)
{
    // 创建搜索定时器
    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(m_searchDelay);
    
    // 创建进度定时器
    m_progressTimer = new QTimer(this);
    m_progressTimer->setSingleShot(false);
    m_progressTimer->setInterval(1000);  // 每秒更新一次进度

    // 连接信号
    connect(m_searchTimer, &QTimer::timeout, this, &GitLogSearchManager::performSearch);
    connect(m_progressTimer, &QTimer::timeout, this, &GitLogSearchManager::onSearchTimeout);

    qDebug() << "[GitLogSearchManager] Initialized search manager";
}

void GitLogSearchManager::startSearch(const QString &searchText)
{
    m_currentSearchText = searchText.trimmed();

    // 停止之前的搜索
    stopSearch();

    // 检查搜索条件
    if (m_currentSearchText.isEmpty()) {
        clearSearch();
        return;
    }

    if (m_currentSearchText.length() < m_minSearchLength) {
        m_statusLabel->setText(tr("Search term too short (minimum %1 characters)").arg(m_minSearchLength));
        m_statusLabel->show();
        return;
    }

    // 启动延迟搜索
    m_searchTimer->start();

    qDebug() << "[GitLogSearchManager] Starting search for:" << m_currentSearchText;
}

void GitLogSearchManager::stopSearch()
{
    m_searchTimer->stop();
    m_progressTimer->stop();
    
    if (m_isSearching) {
        m_isSearching = false;
        QApplication::restoreOverrideCursor();
        updateSearchStatus();
        Q_EMIT searchCompleted(m_currentSearchText, m_searchTotalFound);
    }
    
    m_isLoadingMore = false;
}

void GitLogSearchManager::clearSearch()
{
    stopSearch();
    
    m_currentSearchText.clear();
    m_searchTotalFound = 0;
    
    // 显示所有项目并清除高亮
    clearHighlights();
    
    // 隐藏状态标签
    m_statusLabel->hide();
    
    Q_EMIT searchCleared();
    
    qDebug() << "[GitLogSearchManager] Search cleared";
}

void GitLogSearchManager::performSearch()
{
    if (m_currentSearchText.isEmpty()) {
        return;
    }

    m_isSearching = true;
    m_searchTotalFound = 0;
    
    // 设置等待光标
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // 启动进度定时器
    m_progressTimer->start();
    
    Q_EMIT searchStarted(m_currentSearchText);
    
    // 过滤当前已加载的提交
    filterCurrentCommits();
    
    // 不再自动加载更多数据，避免死循环和卡死
    stopSearch();
}

void GitLogSearchManager::onNewCommitsLoaded()
{
    if (m_isSearching && m_isLoadingMore) {
        // 新数据加载完成，继续搜索
        filterCurrentCommits();
        
        // 检查是否需要继续加载更多数据
        if (m_searchTotalFound < m_maxSearchResults) {
            // 继续保持加载状态，等待下一批数据
            updateSearchStatus();
        } else {
            // 找到足够的结果，停止搜索
            m_isLoadingMore = false;
            stopSearch();
        }
    }
}

void GitLogSearchManager::onSearchTimeout()
{
    // 更新搜索进度状态
    updateSearchStatus();
}

void GitLogSearchManager::filterCurrentCommits()
{
    if (m_currentSearchText.isEmpty()) {
        // 显示所有项目
        for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
            item->setHidden(false);
            clearItemHighlight(item);
        }
        return;
    }

    const int MAX_SCAN = 1000; // 最多遍历1000条
    const int MAX_HIGHLIGHT = 100; // 最多高亮100条
    int visibleCount = 0;
    int scanned = 0;
    int highlighted = 0;
    int total = m_commitTree->topLevelItemCount();

    for (int i = 0; i < total && scanned < MAX_SCAN; ++i, ++scanned) {
        QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
        bool matches = itemMatchesSearch(item, m_currentSearchText);
        item->setHidden(!matches);
        if (matches) {
            visibleCount++;
            if (highlighted < MAX_HIGHLIGHT) {
                highlightItemMatches(item, m_currentSearchText);
                highlighted++;
            } else {
                clearItemHighlight(item);
            }
        } else {
            clearItemHighlight(item);
        }
    }
    // 超出部分全部隐藏
    for (int i = scanned; i < total; ++i) {
        QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
        item->setHidden(true);
        clearItemHighlight(item);
    }

    m_searchTotalFound = visibleCount;
    updateSearchStatus();

    if (scanned == MAX_SCAN && visibleCount > MAX_HIGHLIGHT) {
        m_statusLabel->setText(tr("Too many results, only showing first %1 matches. Please refine your search.").arg(MAX_HIGHLIGHT));
        m_statusLabel->show();
    }

    qDebug() << "[GitLogSearchManager] Filtered commits, found:" << m_searchTotalFound << "matches (scanned" << scanned << ")";
}

void GitLogSearchManager::highlightSearchResults()
{
    if (m_currentSearchText.isEmpty()) {
        clearHighlights();
        return;
    }

    for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
        if (!item->isHidden()) {
            highlightItemMatches(item, m_currentSearchText);
        }
    }
}

void GitLogSearchManager::clearHighlights()
{
    for (int i = 0; i < m_commitTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_commitTree->topLevelItem(i);
        item->setHidden(false);
        clearItemHighlight(item);
    }
}

void GitLogSearchManager::updateSearchStatus()
{
    if (!m_statusLabel) {
        return;
    }

    QString statusText;
    
    if (m_isSearching) {
        if (m_isLoadingMore) {
            statusText = tr("Searching... (loading more commits, found %1 so far)").arg(m_searchTotalFound);
        } else {
            statusText = tr("Searching... (found %1 commits)").arg(m_searchTotalFound);
        }
    } else {
        if (m_searchTotalFound > 0) {
            statusText = tr("Search completed: %1 commits found").arg(m_searchTotalFound);
        } else if (!m_currentSearchText.isEmpty()) {
            statusText = tr("Search completed: No commits found for '%1'").arg(m_currentSearchText);
        } else {
            m_statusLabel->hide();
            return;
        }
    }

    m_statusLabel->setText(statusText);
    m_statusLabel->show();
}

void GitLogSearchManager::highlightItemMatches(QTreeWidgetItem *item, const QString &searchText)
{
    if (!item || searchText.isEmpty()) {
        return;
    }

    // 高亮颜色
    QColor highlightColor(255, 255, 0, 80);  // 淡黄色背景

    // 检查各列是否匹配并设置高亮
    for (int col = 1; col <= 4; ++col) {  // 跳过Graph列
        QString text = item->text(col);
        if (text.contains(searchText, Qt::CaseInsensitive)) {
            item->setBackground(col, QBrush(highlightColor));

            // 更新工具提示
            QString tooltip = item->toolTip(col);
            if (!tooltip.contains("Match:")) {
                if (!tooltip.isEmpty()) {
                    tooltip += "\n";
                }
                tooltip += QString("Match: '%1'").arg(searchText);
                item->setToolTip(col, tooltip);
            }
        }
    }
}

void GitLogSearchManager::clearItemHighlight(QTreeWidgetItem *item)
{
    if (!item) {
        return;
    }

    // 清除所有列的背景色
    for (int col = 0; col < item->columnCount(); ++col) {
        item->setBackground(col, QBrush());

        // 清除匹配相关的工具提示
        QString tooltip = item->toolTip(col);
        if (tooltip.contains("Match:")) {
            int matchIndex = tooltip.indexOf("\nMatch:");
            if (matchIndex >= 0) {
                tooltip = tooltip.left(matchIndex);
            } else if (tooltip.startsWith("Match:")) {
                tooltip.clear();
            }
            item->setToolTip(col, tooltip);
        }
    }
}

bool GitLogSearchManager::itemMatchesSearch(QTreeWidgetItem *item, const QString &searchText) const
{
    if (!item || searchText.isEmpty()) {
        return false;
    }

    // 检查消息、作者、日期、哈希列
    for (int col = 1; col <= 4; ++col) {
        if (item->text(col).contains(searchText, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
} 