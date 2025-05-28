#ifndef GITCOMMANDEXECUTOR_H
#define GITCOMMANDEXECUTOR_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

/**
 * @brief Git命令执行器 - 统一Git命令执行和路径处理
 * 
 * 提供线程安全的Git命令执行，自动路径转换，错误处理和超时管理
 */
class GitCommandExecutor : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Git命令执行结果
     */
    enum class Result {
        Success,        ///< 命令执行成功
        Timeout,        ///< 命令执行超时
        CommandError,   ///< Git命令返回错误
        ParseError,     ///< 输出解析错误
        PathError,      ///< 路径相关错误
        ProcessError    ///< 进程启动错误
    };

    /**
     * @brief Git命令结构体
     */
    struct GitCommand {
        QString command;           ///< 命令名称（用于日志）
        QStringList arguments;     ///< Git命令参数
        QString workingDirectory;  ///< 工作目录
        int timeout = 10000;       ///< 超时时间（毫秒）
    };

    explicit GitCommandExecutor(QObject *parent = nullptr);
    ~GitCommandExecutor();

    /**
     * @brief 执行Git命令
     * @param cmd Git命令结构
     * @param output 命令输出（成功时）
     * @param error 错误输出
     * @return 执行结果
     */
    Result executeCommand(const GitCommand &cmd, QString &output, QString &error);

    /**
     * @brief 异步执行Git命令
     * @param cmd Git命令结构
     */
    void executeCommandAsync(const GitCommand &cmd);

    /**
     * @brief 解析文件所属的Git仓库路径
     * @param filePath 文件绝对路径
     * @return 仓库根目录路径，失败返回空字符串
     */
    QString resolveRepositoryPath(const QString &filePath);

    /**
     * @brief 将绝对路径转换为相对于仓库根目录的路径
     * @param repoPath 仓库根目录路径
     * @param filePath 文件绝对路径
     * @return 相对路径，失败返回空字符串
     */
    QString makeRelativePath(const QString &repoPath, const QString &filePath);

    /**
     * @brief 验证路径是否在Git仓库中
     * @param filePath 文件路径
     * @return 是否有效
     */
    bool isValidRepositoryPath(const QString &filePath);

    /**
     * @brief 取消当前执行的命令
     */
    void cancelCurrentCommand();

Q_SIGNALS:
    /**
     * @brief 命令执行完成信号
     * @param command 命令名称
     * @param result 执行结果
     * @param output 输出内容
     * @param error 错误内容
     */
    void commandFinished(const QString &command, GitCommandExecutor::Result result, 
                        const QString &output, const QString &error);

    /**
     * @brief 命令输出更新信号（异步执行时）
     * @param output 新的输出内容
     * @param isError 是否为错误输出
     */
    void outputReady(const QString &output, bool isError);

private Q_SLOTS:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onProcessTimeout();

private:
    void setupProcessEnvironment(QProcess *process);
    Result processToResult(int exitCode, QProcess::ExitStatus exitStatus, QProcess::ProcessError processError);
    
    QProcess *m_currentProcess;
    QTimer *m_timeoutTimer;
    GitCommand m_currentCommand;
    bool m_isExecuting;
};

#endif // GITCOMMANDEXECUTOR_H 