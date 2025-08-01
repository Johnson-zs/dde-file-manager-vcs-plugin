#include "gitoperationdialog.h"
#include "gitcommandexecutor.h"
#include "../widgets/characteranimationwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

GitOperationDialog::GitOperationDialog(const QString &operation, QWidget *parent)
    : QDialog(parent),
      m_operation(operation),
      m_executionResult(GitCommandExecutor::Result::Success),
      m_executor(new GitCommandExecutor(this)),
      m_isExecuting(false),
      m_showDetails(false)
{
    setupUI();

    connect(m_executor, &GitCommandExecutor::commandFinished,
            this, &GitOperationDialog::onCommandFinished);
    connect(m_executor, &GitCommandExecutor::outputReady,
            this, &GitOperationDialog::onOutputReady);
}

GitOperationDialog::~GitOperationDialog()
{
    if (m_isExecuting) {
        m_executor->cancelCurrentCommand();
    }
}

void GitOperationDialog::setupUI()
{
    setWindowTitle(tr("Git %1").arg(m_operation));
    setModal(true);
    setMinimumSize(500, 200);
    resize(600, 300);

    auto *mainLayout = new QVBoxLayout(this);

    setupProgressSection();
    setupOutputSection();
    setupButtonSection();

    mainLayout->addWidget(m_descriptionLabel);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_animationWidget);
    mainLayout->addWidget(m_outputWidget);
    mainLayout->addWidget(m_buttonWidget);

    m_outputWidget->setVisible(false);
    m_animationWidget->setVisible(false);
    adjustSize();
}

void GitOperationDialog::setupProgressSection()
{
    m_descriptionLabel = new QLabel;
    m_descriptionLabel->setWordWrap(true);

    m_statusLabel = new QLabel(tr("Preparing to execute %1 operation...").arg(m_operation));
    m_statusLabel->setStyleSheet("QLabel { color: #555; }");

    // 创建字符动画组件
    m_animationWidget = new CharacterAnimationWidget(this);
    m_animationWidget->setTextStyleSheet("QLabel { color: #2196F3; font-weight: bold; }");
}

void GitOperationDialog::setupOutputSection()
{
    m_outputText = new QTextEdit;
    m_outputText->setReadOnly(true);
    m_outputText->setFont(QFont("Consolas", 9));
    m_outputText->setMinimumHeight(200);

    m_outputWidget = new QWidget;
    auto *outputLayout = new QVBoxLayout(m_outputWidget);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->addWidget(new QLabel(tr("Command Output:")));
    outputLayout->addWidget(m_outputText);
}

void GitOperationDialog::setupButtonSection()
{
    auto *buttonLayout = new QHBoxLayout;

    m_detailsButton = new QPushButton(tr("Show Details"));
    m_detailsButton->setCheckable(true);
    connect(m_detailsButton, &QPushButton::toggled, this, &GitOperationDialog::onDetailsToggled);

    buttonLayout->addWidget(m_detailsButton);
    buttonLayout->addStretch();

    m_retryButton = new QPushButton(tr("Retry"));
    m_retryButton->setVisible(false);
    connect(m_retryButton, &QPushButton::clicked, this, &GitOperationDialog::onRetryClicked);
    buttonLayout->addWidget(m_retryButton);

    m_cancelButton = new QPushButton(tr("Cancel"));
    connect(m_cancelButton, &QPushButton::clicked, this, &GitOperationDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelButton);

    m_closeButton = new QPushButton(tr("Close"));
    m_closeButton->setDefault(true);
    m_closeButton->setVisible(false);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    m_buttonWidget = new QWidget(this);
    m_buttonWidget->setLayout(buttonLayout);
}

void GitOperationDialog::executeCommand(const QString &repoPath, const QStringList &arguments, int timeout)
{
    m_lastRepoPath = repoPath;
    m_lastArguments = arguments;

    updateUIState(true);
    m_outputText->clear();

    // 启动字符动画
    QString animationText = tr("Executing: git %1").arg(arguments.join(' '));
    m_animationWidget->setVisible(true);
    m_animationWidget->startAnimation(animationText);

    GitCommandExecutor::GitCommand cmd;
    cmd.command = m_operation;
    cmd.arguments = arguments;
    cmd.workingDirectory = repoPath;
    cmd.timeout = timeout;

    qInfo() << "INFO: [GitOperationDialog::executeCommand] Starting" << m_operation
            << "with args:" << arguments << "in" << repoPath;

    m_executor->executeCommandAsync(cmd);
}

void GitOperationDialog::setOperationDescription(const QString &description)
{
    m_currentDescription = description;
    m_descriptionLabel->setText(description);
    m_descriptionLabel->setVisible(!description.isEmpty());
}

GitCommandExecutor::Result GitOperationDialog::getExecutionResult() const
{
    return m_executionResult;
}

void GitOperationDialog::onCommandFinished(const QString &command, GitCommandExecutor::Result result,
                                           const QString &output, const QString &error)
{
    Q_UNUSED(command)

    m_executionResult = result;

    // 停止字符动画
    m_animationWidget->stopAnimation();
    m_animationWidget->setVisible(false);

    updateUIState(false);
    showResult(result, output, error);

    if (result == GitCommandExecutor::Result::Success) {
        qInfo() << "INFO: [GitOperationDialog::onCommandFinished] Operation completed successfully:" << m_operation;
    } else {
        qWarning() << "WARNING: [GitOperationDialog::onCommandFinished] Operation failed:" << m_operation;
    }
}

void GitOperationDialog::onOutputReady(const QString &output, bool isError)
{
    if (isError) {
        m_outputText->setTextColor(QColor(200, 50, 50));
    } else {
        m_outputText->setTextColor(QColor(50, 50, 50));
    }

    m_outputText->append(output);

    // 自动滚动到底部
    QTextCursor cursor = m_outputText->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_outputText->setTextCursor(cursor);
}

void GitOperationDialog::onCancelClicked()
{
    if (m_isExecuting) {
        m_executor->cancelCurrentCommand();
        m_statusLabel->setText(tr("Operation cancelled"));

        // 停止字符动画
        m_animationWidget->stopAnimation();
        m_animationWidget->setVisible(false);

        updateUIState(false);
        qInfo() << "INFO: [GitOperationDialog::onCancelClicked] User cancelled operation:" << m_operation;
    } else {
        reject();
    }
}

void GitOperationDialog::onRetryClicked()
{
    if (!m_lastArguments.isEmpty() && !m_lastRepoPath.isEmpty()) {
        qInfo() << "INFO: [GitOperationDialog::onRetryClicked] Retrying operation:" << m_operation;
        executeCommand(m_lastRepoPath, m_lastArguments);
    }
}

void GitOperationDialog::onDetailsToggled(bool visible)
{
    m_showDetails = visible;
    m_outputWidget->setVisible(visible);
    m_detailsButton->setText(visible ? tr("Hide Details") : tr("Show Details"));

    if (visible) {
        resize(width(), height() + 250);
    } else {
        adjustSize();
    }
}

void GitOperationDialog::updateUIState(bool isExecuting)
{
    m_isExecuting = isExecuting;

    if (isExecuting) {
        m_cancelButton->setText(tr("Cancel"));
        m_cancelButton->setVisible(true);
        m_retryButton->setVisible(false);
        m_closeButton->setVisible(false);
    } else {
        if (m_executionResult == GitCommandExecutor::Result::Success) {
            m_cancelButton->setVisible(false);
            m_retryButton->setVisible(false);
            m_closeButton->setVisible(true);
        } else {
            m_cancelButton->setText(tr("Close"));
            m_cancelButton->setVisible(true);
            m_retryButton->setVisible(true);
            m_closeButton->setVisible(false);
        }
    }

    m_retryButton->setEnabled(!isExecuting);
    m_detailsButton->setEnabled(true);
}

void GitOperationDialog::showResult(GitCommandExecutor::Result result, const QString &output, const QString &error)
{
    QString statusText;
    QString styleSheet;

    switch (result) {
    case GitCommandExecutor::Result::Success:
        statusText = tr("✓ Operation completed successfully");
        styleSheet = "QLabel { color: #27ae60; font-weight: bold; }";
        break;
    case GitCommandExecutor::Result::CommandError:
        statusText = tr("✗ Git command execution failed");
        styleSheet = "QLabel { color: #e74c3c; font-weight: bold; }";
        if (!error.isEmpty()) {
            m_outputText->append("\n" + tr("Error information: ") + error);
        }
        break;
    case GitCommandExecutor::Result::Timeout:
        statusText = tr("⏱ Operation timed out");
        styleSheet = "QLabel { color: #f39c12; font-weight: bold; }";
        break;
    default:
        statusText = tr("✗ Unknown error");
        styleSheet = "QLabel { color: #e74c3c; font-weight: bold; }";
        break;
    }

    m_statusLabel->setText(statusText);
    m_statusLabel->setStyleSheet(styleSheet);

    if (!output.isEmpty() && !m_showDetails) {
        m_detailsButton->setText(tr("Show Details (New output)"));
        m_detailsButton->setStyleSheet("QPushButton { font-weight: bold; }");
    }

    if (!output.isEmpty()) {
        m_outputText->append("\n--- Operation completed ---\n" + output);
    }
}
