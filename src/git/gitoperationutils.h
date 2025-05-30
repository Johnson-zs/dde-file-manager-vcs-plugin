#ifndef GITOPERATIONUTILS_H
#define GITOPERATIONUTILS_H

#include <QString>
#include <QStringList>
#include <QObject>

/**
 * @brief Git操作结果结构
 */
struct GitOperationResult {
    bool success;
    QString output;
    QString error;
    int exitCode;
    
    GitOperationResult() : success(false), exitCode(-1) {}
    GitOperationResult(bool ok, const QString &out = QString(), const QString &err = QString(), int code = 0)
        : success(ok), output(out), error(err), exitCode(code) {}
};

/**
 * @brief Git操作工具类
 * 
 * 提供统一的Git命令执行接口，避免在各个对话框中重复实现相同的Git操作
 */
class GitOperationUtils : public QObject
{
    Q_OBJECT

public:
    explicit GitOperationUtils(QObject *parent = nullptr);

    // === 文件操作 ===
    /**
     * @brief 暂存文件
     * @param repositoryPath 仓库路径
     * @param filePath 文件路径（相对于仓库根目录）
     * @return 操作结果
     */
    static GitOperationResult stageFile(const QString &repositoryPath, const QString &filePath);

    /**
     * @brief 取消暂存文件
     * @param repositoryPath 仓库路径
     * @param filePath 文件路径（相对于仓库根目录）
     * @return 操作结果
     */
    static GitOperationResult unstageFile(const QString &repositoryPath, const QString &filePath);

    /**
     * @brief 丢弃文件更改
     * @param repositoryPath 仓库路径
     * @param filePath 文件路径（相对于仓库根目录）
     * @return 操作结果
     */
    static GitOperationResult discardFile(const QString &repositoryPath, const QString &filePath);

    /**
     * @brief 添加文件到Git跟踪
     * @param repositoryPath 仓库路径
     * @param filePath 文件路径（相对于仓库根目录）
     * @return 操作结果
     */
    static GitOperationResult addFile(const QString &repositoryPath, const QString &filePath);

    /**
     * @brief 重置文件到HEAD状态
     * @param repositoryPath 仓库路径
     * @param filePath 文件路径（相对于仓库根目录）
     * @return 操作结果
     */
    static GitOperationResult resetFile(const QString &repositoryPath, const QString &filePath);

    // === 批量操作 ===
    /**
     * @brief 批量暂存文件
     * @param repositoryPath 仓库路径
     * @param filePaths 文件路径列表
     * @return 操作结果
     */
    static GitOperationResult stageFiles(const QString &repositoryPath, const QStringList &filePaths);

    /**
     * @brief 批量取消暂存文件
     * @param repositoryPath 仓库路径
     * @param filePaths 文件路径列表
     * @return 操作结果
     */
    static GitOperationResult unstageFiles(const QString &repositoryPath, const QStringList &filePaths);

    /**
     * @brief 批量添加文件
     * @param repositoryPath 仓库路径
     * @param filePaths 文件路径列表
     * @return 操作结果
     */
    static GitOperationResult addFiles(const QString &repositoryPath, const QStringList &filePaths);

    /**
     * @brief 批量重置文件
     * @param repositoryPath 仓库路径
     * @param filePaths 文件路径列表
     * @return 操作结果
     */
    static GitOperationResult resetFiles(const QString &repositoryPath, const QStringList &filePaths);

    // === 仓库信息 ===
    /**
     * @brief 获取当前分支名
     * @param repositoryPath 仓库路径
     * @return 分支名，失败时返回空字符串
     */
    static QString getCurrentBranch(const QString &repositoryPath);

    /**
     * @brief 检查仓库是否干净（无未提交更改）
     * @param repositoryPath 仓库路径
     * @return true表示仓库干净
     */
    static bool isRepositoryClean(const QString &repositoryPath);

    // === 通用Git命令执行 ===
    /**
     * @brief 执行Git命令
     * @param repositoryPath 仓库路径
     * @param arguments Git命令参数
     * @param timeoutMs 超时时间（毫秒）
     * @return 操作结果
     */
    static GitOperationResult executeGitCommand(const QString &repositoryPath, 
                                                const QStringList &arguments, 
                                                int timeoutMs = 5000);

private:
    /**
     * @brief 执行单个文件操作的通用方法
     * @param repositoryPath 仓库路径
     * @param operation 操作名称（用于日志）
     * @param arguments Git命令参数
     * @param filePath 文件路径
     * @return 操作结果
     */
    static GitOperationResult executeSingleFileOperation(const QString &repositoryPath,
                                                         const QString &operation,
                                                         const QStringList &arguments,
                                                         const QString &filePath);

    /**
     * @brief 执行批量文件操作的通用方法
     * @param repositoryPath 仓库路径
     * @param operation 操作名称（用于日志）
     * @param baseArguments 基础Git命令参数
     * @param filePaths 文件路径列表
     * @return 操作结果
     */
    static GitOperationResult executeBatchFileOperation(const QString &repositoryPath,
                                                        const QString &operation,
                                                        const QStringList &baseArguments,
                                                        const QStringList &filePaths);
};

#endif // GITOPERATIONUTILS_H 