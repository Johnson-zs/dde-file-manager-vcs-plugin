#ifndef GITSTATUSPARSER_H
#define GITSTATUSPARSER_H

#include <QString>
#include <QStringList>
#include <QIcon>
#include <QObject>
#include <QRegularExpression>
#include <memory>

/**
 * @brief Git文件状态枚举
 */
enum class GitFileStatus {
    Modified,           // Modified but not staged
    Staged,             // Staged for commit
    Untracked,          // Not tracked by Git
    Deleted,            // Deleted but not staged
    StagedDeleted,      // Staged for deletion
    StagedModified,     // Staged modification
    StagedAdded,        // Staged addition
    Renamed,            // Renamed file
    Copied,             // Copied file
    Unknown             // Unknown status
};

/**
 * @brief Git文件信息结构
 */
struct GitFileInfo {
    QString filePath;
    GitFileStatus status;
    QString statusText;
    bool isStaged;
    
    GitFileInfo() : status(GitFileStatus::Unknown), isStaged(false) {}
    GitFileInfo(const QString &path, GitFileStatus stat, const QString &text = QString())
        : filePath(path), status(stat), statusText(text), isStaged(isFileStaged(stat)) {}
    
    QString fileName() const;
    QString displayPath() const { return filePath; }
    QIcon statusIcon() const;
    QString statusDisplayText() const;
    
private:
    static bool isFileStaged(GitFileStatus status);
};

/**
 * @brief Git状态解析器和工具类
 * 
 * 提供统一的Git状态解析、文件名编码处理和相关工具函数
 */
class GitStatusParser : public QObject
{
    Q_OBJECT

public:
    explicit GitStatusParser(QObject *parent = nullptr);

    // === Git状态解析 ===
    /**
     * @brief 解析git status --porcelain输出
     * @param gitStatusOutput git status命令的输出
     * @return 解析后的文件信息列表
     */
    static QList<std::shared_ptr<GitFileInfo>> parseGitStatus(const QString &gitStatusOutput);

    /**
     * @brief 执行git status命令并解析结果
     * @param repositoryPath 仓库路径
     * @return 解析后的文件信息列表
     */
    static QList<std::shared_ptr<GitFileInfo>> getRepositoryStatus(const QString &repositoryPath);

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

    // === 状态转换 ===
    /**
     * @brief 解析Git状态码到枚举
     * @param indexStatus 索引状态字符
     * @param workingStatus 工作区状态字符
     * @return Git文件状态枚举
     */
    static GitFileStatus parseFileStatus(const QString &indexStatus, const QString &workingStatus);

    /**
     * @brief 获取状态图标
     * @param status Git文件状态
     * @return 对应的图标
     */
    static QIcon getStatusIcon(GitFileStatus status);

    /**
     * @brief 获取状态显示文本
     * @param status Git文件状态
     * @return 状态的显示文本
     */
    static QString getStatusDisplayText(GitFileStatus status);

    /**
     * @brief 获取状态描述（兼容GitStatusDialog的格式）
     * @param statusCode 两字符的Git状态码
     * @return 状态描述
     */
    static QString getStatusDescription(const QString &statusCode);

    /**
     * @brief 获取状态图标（兼容GitStatusDialog的格式）
     * @param statusCode 两字符的Git状态码
     * @return 对应的图标
     */
    static QIcon getStatusIcon(const QString &statusCode);

private:
    static QRegularExpression s_octalRegex;
};

#endif // GITSTATUSPARSER_H 