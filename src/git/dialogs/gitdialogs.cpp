#include "gitdialogs.h"
#include "gitcommitdialog.h"
#include "gitstatusdialog.h"
#include "gitlogdialog.h"
#include "gitblamedialog.h"
#include "gitdiffdialog.h"
#include "gitcheckoutdialog.h"
#include "gitbranchcompariondialog.h"
#include "gitoperationdialog.h"
#include "gitfilepreviewdialog.h"

#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QDebug>
#include <QProcess>
#include <QMimeDatabase>
#include <QMimeType>
#include <QMenu>
#include <QInputDialog>
#include <QStandardPaths>
#include <QCursor>
#include <QLineEdit>
#include <QFileDialog>
#include <QRegularExpression>
#include <QSettings>

GitDialogManager *GitDialogManager::s_instance = nullptr;

GitDialogManager *GitDialogManager::instance()
{
    if (!s_instance) {
        s_instance = new GitDialogManager();
    }
    return s_instance;
}

void GitDialogManager::showCommitDialog(const QString &repositoryPath, QWidget *parent)
{
    auto *dialog = new GitCommitDialog(repositoryPath, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened commit dialog for repository:" << repositoryPath;
}

void GitDialogManager::showCommitDialog(const QString &repositoryPath, const QStringList &files, QWidget *parent)
{
    auto *dialog = new GitCommitDialog(repositoryPath, files, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened commit dialog for files:" << files;
}

void GitDialogManager::showStatusDialog(const QString &repositoryPath, QWidget *parent)
{
    auto *dialog = new GitStatusDialog(repositoryPath, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened status dialog for repository:" << repositoryPath;
}

void GitDialogManager::showLogDialog(const QString &repositoryPath, QWidget *parent)
{
    auto *dialog = new GitLogDialog(repositoryPath, QString(), parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened log dialog for repository:" << repositoryPath;
}

void GitDialogManager::showLogDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
{
    auto *dialog = new GitLogDialog(repositoryPath, filePath, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened log dialog for file:" << filePath;
}

void GitDialogManager::showBlameDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
{
    auto *dialog = new GitBlameDialog(repositoryPath, filePath, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened blame dialog for file:" << filePath;
}

void GitDialogManager::showDiffDialog(const QString &repositoryPath, const QString &filePath, QWidget *parent)
{
    auto *dialog = new GitDiffDialog(repositoryPath, filePath, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened diff dialog for file:" << filePath;
}

void GitDialogManager::showCheckoutDialog(const QString &repositoryPath, QWidget *parent)
{
    auto *dialog = new GitCheckoutDialog(repositoryPath, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened checkout dialog for repository:" << repositoryPath;
}

void GitDialogManager::showOperationDialog(const QString &operation, QWidget *parent)
{
    auto *dialog = new GitOperationDialog(operation, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened operation dialog for:" << operation;
}

void GitDialogManager::showOperationDialog(const QString &operation, const QString &workingDir,
                                           const QStringList &arguments, QWidget *parent)
{
    auto *dialog = new GitOperationDialog(operation, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QString description = QObject::tr("Preparing to execute %1 operation in repository").arg(operation);
    dialog->setOperationDescription(description);

    // Connect completion signal for potential future use
    QObject::connect(dialog, &QDialog::finished, [operation](int result) {
        qDebug() << "[GitDialogManager] Operation dialog finished:" << operation
                 << "result:" << (result == QDialog::Accepted ? "accepted" : "rejected");
    });

    dialog->executeCommand(workingDir, arguments);
    dialog->show();

    qDebug() << "[GitDialogManager] Opened operation dialog for:" << operation
             << "with arguments:" << arguments;
}

void GitDialogManager::openFile(const QString &filePath, QWidget *parent)
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        QMessageBox::warning(parent, QObject::tr("File Not Found"),
                             QObject::tr("The file '%1' does not exist.").arg(filePath));
        return;
    }

    // Try to open with default application
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        QMessageBox::warning(parent, QObject::tr("Open Failed"),
                             QObject::tr("Failed to open file '%1' with default application.").arg(filePath));
    } else {
        qDebug() << "[GitDialogManager] Opened file:" << filePath;
    }
}

void GitDialogManager::showFileInFolder(const QString &filePath, QWidget *parent)
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        QMessageBox::warning(parent, QObject::tr("File Not Found"),
                             QObject::tr("The file '%1' does not exist.").arg(filePath));
        return;
    }

    QString dirPath = fileInfo.absoluteDir().absolutePath();

    if (QStandardPaths::findExecutable("dbus-send").isEmpty() == false) {
        // Try using D-Bus to show file in file manager
        QStringList args;
        args << "--session"
             << "--dest=org.freedesktop.FileManager1"
             << "--type=method_call"
             << "/org/freedesktop/FileManager1"
             << "org.freedesktop.FileManager1.ShowItems"
             << QString("array:string:file://%1").arg(filePath) << "string:";

        if (QProcess::startDetached("dbus-send", args)) {
            qDebug() << "[GitDialogManager] Showed file in folder using D-Bus:" << filePath;
            return;
        }
    }

    // Fallback: use QDesktopServices to open the containing directory
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath))) {
        QMessageBox::warning(parent, QObject::tr("Open Failed"),
                             QObject::tr("Failed to open file manager for directory '%1'.").arg(dirPath));
    } else {
        qDebug() << "[GitDialogManager] Showed directory in file manager:" << dirPath;
    }

    qDebug() << "[GitDialogManager] Showed file in folder:" << filePath;
}

void GitDialogManager::deleteFile(const QString &filePath, QWidget *parent)
{
    QFileInfo fileInfo(filePath);

    if (!fileInfo.exists()) {
        QMessageBox::warning(parent, QObject::tr("File Not Found"),
                             QObject::tr("The file '%1' does not exist.").arg(filePath));
        return;
    }

    int ret = QMessageBox::warning(parent, QObject::tr("Delete File"),
                                   QObject::tr("Are you sure you want to delete the file?\n\n%1\n\n"
                                               "This action cannot be undone.")
                                           .arg(filePath),
                                   QMessageBox::Yes | QMessageBox::No,
                                   QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        if (QFile::remove(filePath)) {
            // QMessageBox::information(parent, QObject::tr("File Deleted"),
            //                          QObject::tr("File deleted successfully."));
            qDebug() << "[GitDialogManager] Deleted file:" << filePath;
        } else {
            QMessageBox::critical(parent, QObject::tr("Delete Failed"),
                                  QObject::tr("Failed to delete the file."));
            qWarning() << "[GitDialogManager] Failed to delete file:" << filePath;
        }
    }
}

void GitDialogManager::showBranchComparisonDialog(const QString &repositoryPath, const QString &baseBranch, 
                                                   const QString &compareBranch, QWidget *parent)
{
    auto *dialog = new GitBranchComparisonDialog(repositoryPath, baseBranch, compareBranch, parent);
    dialog->show();
    qDebug() << "[GitDialogManager] Opened branch comparison dialog:" << baseBranch << "vs" << compareBranch;
}

GitFilePreviewDialog* GitDialogManager::showFilePreview(const QString &repositoryPath, const QString &filePath, QWidget *parent)
{
    auto *dialog = GitFilePreviewDialog::showFilePreview(repositoryPath, filePath, parent);
    qDebug() << "[GitDialogManager] Opened file preview dialog for:" << filePath;
    return dialog;
}

GitFilePreviewDialog* GitDialogManager::showFilePreviewAtCommit(const QString &repositoryPath, const QString &filePath, 
                                                                const QString &commitHash, QWidget *parent)
{
    auto *dialog = GitFilePreviewDialog::showFilePreviewAtCommit(repositoryPath, filePath, commitHash, parent);
    qDebug() << "[GitDialogManager] Opened file preview dialog for:" << filePath << "at commit:" << commitHash.left(8);
    return dialog;
}
