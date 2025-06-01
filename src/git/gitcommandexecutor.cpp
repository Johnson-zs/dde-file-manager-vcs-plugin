#include "gitcommandexecutor.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>

GitCommandExecutor::GitCommandExecutor(QObject *parent)
    : QObject(parent)
    , m_currentProcess(nullptr)
    , m_timeoutTimer(new QTimer(this))
    , m_isExecuting(false)
{
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, &GitCommandExecutor::onProcessTimeout);
}

GitCommandExecutor::~GitCommandExecutor()
{
    cancelCurrentCommand();
}

GitCommandExecutor::Result GitCommandExecutor::executeCommand(const GitCommand &cmd, QString &output, QString &error)
{
    // 参数验证
    if (cmd.arguments.isEmpty()) {
        error = QObject::tr("No Git command arguments provided");
        qWarning() << "ERROR: [GitCommandExecutor::executeCommand] Empty arguments";
        return Result::ParseError;
    }

    if (!QDir(cmd.workingDirectory).exists()) {
        error = QObject::tr("Working directory does not exist: %1").arg(cmd.workingDirectory);
        qWarning() << "ERROR: [GitCommandExecutor::executeCommand] Invalid working directory:" << cmd.workingDirectory;
        return Result::PathError;
    }

    // 创建进程
    QProcess process;
    process.setWorkingDirectory(cmd.workingDirectory);
    setupProcessEnvironment(&process);

    // 记录执行日志
    qInfo() << "INFO: [GitCommandExecutor::executeCommand] Executing git" << cmd.arguments.join(' ') 
            << "in directory:" << cmd.workingDirectory;

    // 启动Git命令
    process.start("git", cmd.arguments);
    
    if (!process.waitForStarted(3000)) {
        error = QObject::tr("Failed to start git process: %1").arg(process.errorString());
        qWarning() << "ERROR: [GitCommandExecutor::executeCommand] Failed to start git process:" << process.errorString();
        return Result::ProcessError;
    }

    // 等待命令完成
    bool finished = process.waitForFinished(cmd.timeout);
    
    if (!finished) {
        process.kill();
        process.waitForFinished(3000); // 等待进程清理
        error = QObject::tr("Git command timed out after %1ms").arg(cmd.timeout);
        qWarning() << "WARNING: [GitCommandExecutor::executeCommand] Command timed out:" << cmd.command;
        return Result::Timeout;
    }

    // 读取输出
    output = QString::fromUtf8(process.readAllStandardOutput());
    error = QString::fromUtf8(process.readAllStandardError());

    // 确定结果
    Result result = processToResult(process.exitCode(), process.exitStatus(), process.error());
    
    if (result == Result::Success) {
        qInfo() << "INFO: [GitCommandExecutor::executeCommand] Command completed successfully:" << cmd.command;
    } else {
        qWarning() << "WARNING: [GitCommandExecutor::executeCommand] Command failed:" << cmd.command 
                  << "Exit code:" << process.exitCode() << "Error:" << error;
    }

    return result;
}

void GitCommandExecutor::executeCommandAsync(const GitCommand &cmd)
{
    if (m_isExecuting) {
        qWarning() << "WARNING: [GitCommandExecutor::executeCommandAsync] Already executing command, cancelling previous";
        cancelCurrentCommand();
    }

    m_currentCommand = cmd;
    m_isExecuting = true;

    // 创建新进程
    m_currentProcess = new QProcess(this);
    m_currentProcess->setWorkingDirectory(cmd.workingDirectory);
    setupProcessEnvironment(m_currentProcess);

    // 连接信号
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitCommandExecutor::onProcessFinished);
    connect(m_currentProcess, &QProcess::errorOccurred,
            this, &GitCommandExecutor::onProcessError);
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, [this]() {
        if (m_currentProcess) {
            QString output = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
            if (!output.isEmpty()) {
                emit outputReady(output, false);
            }
        }
    });
    connect(m_currentProcess, &QProcess::readyReadStandardError, [this]() {
        if (m_currentProcess) {
            QString error = QString::fromUtf8(m_currentProcess->readAllStandardError());
            if (!error.isEmpty()) {
                emit outputReady(error, true);
            }
        }
    });

    // 启动超时定时器
    m_timeoutTimer->start(cmd.timeout);

    // 记录日志并启动进程
    qInfo() << "INFO: [GitCommandExecutor::executeCommandAsync] Starting async execution of git" 
            << cmd.arguments.join(' ') << "in" << cmd.workingDirectory;
    
    m_currentProcess->start("git", cmd.arguments);
}

QString GitCommandExecutor::resolveRepositoryPath(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString searchPath = fileInfo.isDir() ? filePath : fileInfo.absolutePath();

    // 使用Git命令查找仓库根目录
    QProcess process;
    process.setWorkingDirectory(searchPath);
    process.start("git", {"rev-parse", "--show-toplevel"});
    
    if (process.waitForFinished(3000) && process.exitCode() == 0) {
        QString repoPath = QString::fromUtf8(process.readAllStandardOutput().trimmed());
        QDir dir(repoPath);
        if (dir.exists()) {
            QString absolutePath = dir.absolutePath();
            qInfo() << "INFO: [GitCommandExecutor::resolveRepositoryPath] Found repository:" << absolutePath;
            return absolutePath;
        }
    }

    qWarning() << "WARNING: [GitCommandExecutor::resolveRepositoryPath] No repository found for:" << filePath;
    return QString();
}

QString GitCommandExecutor::makeRelativePath(const QString &repoPath, const QString &filePath)
{
    QDir repoDir(repoPath);
    QFileInfo fileInfo(filePath);
    
    if (!repoDir.exists()) {
        qWarning() << "ERROR: [GitCommandExecutor::makeRelativePath] Repository directory does not exist:" << repoPath;
        return QString();
    }

    if (!fileInfo.exists()) {
        qWarning() << "WARNING: [GitCommandExecutor::makeRelativePath] File does not exist:" << filePath;
        // 文件可能已被删除，但仍尝试计算相对路径
    }

    QString relativePath = repoDir.relativeFilePath(fileInfo.absoluteFilePath());
    
    // 验证文件在仓库内
    if (relativePath.startsWith("../")) {
        qWarning() << "ERROR: [GitCommandExecutor::makeRelativePath] File outside repository:" << filePath;
        return QString();
    }

    // 规范化路径分隔符（在Windows上）
    relativePath = QDir::fromNativeSeparators(relativePath);
    
    qInfo() << "INFO: [GitCommandExecutor::makeRelativePath] Converted" << filePath 
            << "to relative path:" << relativePath;
    
    return relativePath;
}

bool GitCommandExecutor::isValidRepositoryPath(const QString &filePath)
{
    return !resolveRepositoryPath(filePath).isEmpty();
}

void GitCommandExecutor::cancelCurrentCommand()
{
    if (m_isExecuting && m_currentProcess) {
        qInfo() << "INFO: [GitCommandExecutor::cancelCurrentCommand] Cancelling current command:" << m_currentCommand.command;
        
        // 停止超时定时器
        m_timeoutTimer->stop();
        
        // 断开所有信号连接，防止在进程终止时触发槽函数
        m_currentProcess->disconnect();
        
        // 强制终止进程
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(3000);
        
        // 安全删除进程对象
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
        m_isExecuting = false;
        
        // 发送取消信号
        emit commandFinished(m_currentCommand.command, Result::ProcessError, QString(), tr("Operation cancelled by user"));
    }
}

void GitCommandExecutor::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    // 检查进程是否已经被取消或清理
    if (!m_currentProcess || !m_isExecuting) {
        qInfo() << "INFO: [GitCommandExecutor::onProcessFinished] Process already cancelled or cleaned up";
        return;
    }

    m_timeoutTimer->stop();
    m_isExecuting = false;

    QString output, error;
    output = QString::fromUtf8(m_currentProcess->readAllStandardOutput());
    error = QString::fromUtf8(m_currentProcess->readAllStandardError());
    
    // 清理进程
    m_currentProcess->deleteLater();
    m_currentProcess = nullptr;

    Result result = processToResult(exitCode, exitStatus, QProcess::UnknownError);
    
    if (result == Result::Success) {
        qInfo() << "INFO: [GitCommandExecutor::onProcessFinished] Async command completed successfully:" << m_currentCommand.command;
    } else {
        qWarning() << "WARNING: [GitCommandExecutor::onProcessFinished] Async command failed:" << m_currentCommand.command 
                  << "Exit code:" << exitCode;
    }

    emit commandFinished(m_currentCommand.command, result, output, error);
}

void GitCommandExecutor::onProcessError(QProcess::ProcessError error)
{
    // 检查进程是否已经被取消或清理
    if (!m_currentProcess || !m_isExecuting) {
        qInfo() << "INFO: [GitCommandExecutor::onProcessError] Process already cancelled or cleaned up";
        return;
    }

    m_timeoutTimer->stop();
    m_isExecuting = false;

    QString errorString;
    Result result = Result::ProcessError;
    
    switch (error) {
    case QProcess::FailedToStart:
        errorString = QObject::tr("Failed to start git process");
        break;
    case QProcess::Crashed:
        errorString = QObject::tr("Git process crashed");
        break;
    case QProcess::Timedout:
        errorString = QObject::tr("Git process timed out");
        result = Result::Timeout;
        break;
    case QProcess::ReadError:
    case QProcess::WriteError:
        errorString = QObject::tr("Git process I/O error");
        break;
    default:
        errorString = QObject::tr("Unknown git process error");
        break;
    }

    qWarning() << "ERROR: [GitCommandExecutor::onProcessError]" << errorString << "for command:" << m_currentCommand.command;

    // 清理进程
    m_currentProcess->deleteLater();
    m_currentProcess = nullptr;

    emit commandFinished(m_currentCommand.command, result, QString(), errorString);
}

void GitCommandExecutor::onProcessTimeout()
{
    qWarning() << "WARNING: [GitCommandExecutor::onProcessTimeout] Command timed out:" << m_currentCommand.command;
    
    // 检查进程是否仍然存在且正在执行
    if (m_currentProcess && m_isExecuting) {
        // 断开信号连接，防止在强制终止时触发其他槽函数
        m_currentProcess->disconnect();
        
        // 强制终止进程
        m_currentProcess->kill();
        m_currentProcess->waitForFinished(3000);
        
        // 清理状态
        m_isExecuting = false;
        m_currentProcess->deleteLater();
        m_currentProcess = nullptr;
        
        // 发送超时信号
        emit commandFinished(m_currentCommand.command, Result::Timeout, QString(), tr("Command execution timed out"));
    }
}

void GitCommandExecutor::setupProcessEnvironment(QProcess *process)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    // 设置Git环境变量
    env.insert("GIT_TERMINAL_PROMPT", "0"); // 禁用交互式提示
    env.insert("GIT_ASKPASS", "echo");      // 避免密码提示
    env.insert("LC_ALL", "C");              // 使用英文输出便于解析
    
    process->setProcessEnvironment(env);
}

GitCommandExecutor::Result GitCommandExecutor::processToResult(int exitCode, QProcess::ExitStatus exitStatus, QProcess::ProcessError processError)
{
    if (processError != QProcess::UnknownError) {
        return Result::ProcessError;
    }
    
    if (exitStatus != QProcess::NormalExit) {
        return Result::ProcessError;
    }
    
    return (exitCode == 0) ? Result::Success : Result::CommandError;
} 