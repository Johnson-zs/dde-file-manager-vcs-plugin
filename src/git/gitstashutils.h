#ifndef GITSTASHUTILS_H
#define GITSTASHUTILS_H

#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QRegularExpression>

/**
 * @brief Stash信息结构体
 */
struct GitStashInfo {
    int index;                    // stash@{n}中的n
    QString message;              // stash消息
    QString branch;               // 创建stash时的分支
    QDateTime timestamp;          // 创建时间
    QString shortHash;            // 短哈希
    QString author;               // 作者
    QString fullRef;              // 完整引用，如stash@{0}
    
    GitStashInfo() : index(-1) {}
    GitStashInfo(int idx, const QString &msg, const QString &br, const QDateTime &ts, 
                 const QString &hash, const QString &auth, const QString &ref = QString())
        : index(idx), message(msg), branch(br), timestamp(ts), shortHash(hash), author(auth), fullRef(ref) {}
    
    bool isValid() const { return index >= 0; }
};

/**
 * @brief Git Stash工具类
 * 
 * 提供stash相关的辅助功能，包括：
 * - 解析stash列表
 * - 格式化stash信息
 * - 检查stash状态
 * - 验证stash索引
 */
class GitStashUtils
{
public:
    /**
     * @brief 解析git stash list命令的输出
     * @param stashListOutput git stash list命令的输出
     * @return 解析后的stash信息列表
     */
    static QList<GitStashInfo> parseStashList(const QStringList &stashListOutput);
    
    /**
     * @brief 解析单行stash信息
     * @param line 单行stash信息
     * @return 解析后的stash信息
     */
    static GitStashInfo parseStashLine(const QString &line);
    
    /**
     * @brief 格式化stash信息用于显示
     * @param info stash信息
     * @return 格式化后的显示文本
     */
    static QString formatStashDisplayText(const GitStashInfo &info);
    
    /**
     * @brief 格式化stash详细信息
     * @param info stash信息
     * @return 格式化后的详细信息
     */
    static QString formatStashDetailText(const GitStashInfo &info);
    
    /**
     * @brief 验证stash索引是否有效
     * @param index stash索引
     * @param maxIndex 最大有效索引
     * @return 如果索引有效返回true
     */
    static bool isValidStashIndex(int index, int maxIndex);
    
    /**
     * @brief 检查仓库是否有stash
     * @param repositoryPath 仓库路径
     * @return 如果有stash返回true
     */
    static bool hasStashes(const QString &repositoryPath);
    
    /**
     * @brief 获取stash数量
     * @param repositoryPath 仓库路径
     * @return stash数量
     */
    static int getStashCount(const QString &repositoryPath);
    
    /**
     * @brief 生成stash引用字符串
     * @param index stash索引
     * @return stash引用字符串，如"stash@{0}"
     */
    static QString generateStashRef(int index);
    
    /**
     * @brief 从stash引用字符串中提取索引
     * @param stashRef stash引用字符串，如"stash@{0}"
     * @return 提取的索引，如果解析失败返回-1
     */
    static int extractStashIndex(const QString &stashRef);
    
    /**
     * @brief 检查stash消息是否有效
     * @param message stash消息
     * @return 如果消息有效返回true
     */
    static bool isValidStashMessage(const QString &message);
    
    /**
     * @brief 清理和格式化stash消息
     * @param message 原始消息
     * @return 清理后的消息
     */
    static QString cleanStashMessage(const QString &message);
    
    /**
     * @brief 生成默认的stash消息
     * @param branchName 当前分支名
     * @return 默认stash消息
     */
    static QString generateDefaultStashMessage(const QString &branchName = QString());
    
    /**
     * @brief 解析相对时间字符串（如"2 hours ago"）
     * @param relativeTime 相对时间字符串
     * @return 估算的绝对时间
     */
    static QDateTime parseRelativeTime(const QString &relativeTime);
    
    /**
     * @brief 格式化时间差
     * @param dateTime 时间
     * @return 格式化的时间差字符串
     */
    static QString formatTimeAgo(const QDateTime &dateTime);

private:
    // 私有构造函数，这是一个工具类
    GitStashUtils() = default;
    
    // 正则表达式模式
    static const QRegularExpression s_stashRefPattern;
    static const QRegularExpression s_branchPattern;
    static const QRegularExpression s_timePattern;
};

#endif // GITSTASHUTILS_H 