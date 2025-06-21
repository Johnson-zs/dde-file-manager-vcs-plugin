#pragma once

#include "git-types.h"
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <tuple>

/**
 * @brief Git状态解析器
 * 
 * 提供Git状态解析、文件名编码处理和相关工具函数
 */
class GitStatusParser
{
public:
    // === Git状态解析 ===
    
    /**
     * @brief 解析git status --porcelain输出
     * @param gitStatusOutput git status命令的输出
     * @return 解析后的状态映射
     */
    static GitStatusMap parseGitStatus(const QString &gitStatusOutput);
    
    /**
     * @brief 解析单个文件状态行
     * @param statusLine Git状态行
     * @return 文件状态
     */
    static ItemVersion parseFileStatus(const QString &statusLine);
    
    /**
     * @brief 解析Git状态码到枚举
     * @param indexStatus 索引状态字符
     * @param workingStatus 工作区状态字符
     * @return Git文件状态枚举
     */
    static ItemVersion parseFileStatusFromChars(char indexStatus, char workingStatus);
    
    /**
     * @brief 执行git status命令并解析结果
     * @param repositoryPath 仓库路径
     * @return 解析后的状态映射
     */
    static GitStatusMap getRepositoryStatus(const QString &repositoryPath);
    
    // === 文件名编码处理 ===
    
    /**
     * @brief 处理Git引用的文件名（包含转义序列）
     * @param quotedFilename 带引号的文件名
     * @return 解码后的文件名
     */
    static QString unquoteGitFilename(const QString &quotedFilename);
    
    /**
     * @brief 处理八进制转义序列
     * @param text 包含八进制转义序列的文本
     * @return 解码后的文本
     */
    static QString processOctalEscapes(const QString &text);
    
    // === 日志解析 ===
    
    /**
     * @brief 解析Git日志输出
     * @param output Git日志输出
     * @return 日志条目列表
     */
    static QStringList parseGitLog(const QString &output);
    
    // === 状态描述 ===
    
    /**
     * @brief 获取状态描述
     * @param status 文件状态
     * @return 状态描述字符串
     */
    static QString getStatusDescription(ItemVersion status);
    
    /**
     * @brief 获取状态描述（兼容旧格式）
     * @param statusCode 两字符的Git状态码
     * @return 状态描述
     */
    static QString getStatusDescription(const QString &statusCode);
    
private:
    GitStatusParser() = default;
    
    // 静态正则表达式
    static QRegularExpression s_octalRegex;
}; 