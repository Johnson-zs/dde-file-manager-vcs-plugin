#ifndef GITOPERATIONDIALOG_H
#define GITOPERATIONDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QWidget>
#include <QScrollArea>

#include "gitcommandexecutor.h"

/**
 * @brief Git操作进度对话框 - 重构版本
 *
 * 使用GitCommandExecutor提供统一的Git操作界面
 * 支持实时输出、取消操作、重试机制
 */
class GitOperationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GitOperationDialog(const QString &operation, QWidget *parent = nullptr);
    ~GitOperationDialog();

    /**
     * @brief 执行Git命令
     * @param repoPath 仓库路径
     * @param arguments Git命令参数
     * @param timeout 超时时间（毫秒），默认30秒
     */
    void executeCommand(const QString &repoPath, const QStringList &arguments, int timeout = 30000);

    /**
     * @brief 设置操作描述
     * @param description 操作描述文本
     */
    void setOperationDescription(const QString &description);

    /**
     * @brief 获取命令执行结果
     * @return 执行结果枚举
     */
    GitCommandExecutor::Result getExecutionResult() const;

private Q_SLOTS:
    void onCommandFinished(const QString &command, GitCommandExecutor::Result result,
                           const QString &output, const QString &error);
    void onOutputReady(const QString &output, bool isError);
    void onCancelClicked();
    void onRetryClicked();
    void onDetailsToggled(bool visible);

private:
    void setupUI();
    void setupProgressSection();
    void setupOutputSection();
    void setupButtonSection();
    void updateUIState(bool isExecuting);
    void showResult(GitCommandExecutor::Result result, const QString &output, const QString &error);

    QString m_operation;
    QString m_currentDescription;
    QStringList m_lastArguments;
    QString m_lastRepoPath;
    GitCommandExecutor::Result m_executionResult;

    // UI组件
    QLabel *m_statusLabel;
    QLabel *m_descriptionLabel;
    QProgressBar *m_progressBar;
    QTextEdit *m_outputText;
    QScrollArea *m_outputScrollArea;
    QPushButton *m_cancelButton;
    QPushButton *m_retryButton;
    QPushButton *m_closeButton;
    QPushButton *m_detailsButton;
    QWidget *m_outputWidget;
    QWidget *m_buttonWidget;

    // 核心组件
    GitCommandExecutor *m_executor;
    bool m_isExecuting;
    bool m_showDetails;
};

#endif // GITOPERATIONDIALOG_H 