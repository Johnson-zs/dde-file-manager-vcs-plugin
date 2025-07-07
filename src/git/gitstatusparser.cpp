#include "gitstatusparser.h"

#include <QProcess>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpressionMatchIterator>

// 静态成员初始化
QRegularExpression GitStatusParser::s_octalRegex("\\\\([0-7]{3})");

// ============================================================================
// GitFileInfo Implementation
// ============================================================================

QString GitFileInfo::fileName() const
{
    return QFileInfo(filePath).fileName();
}

QIcon GitFileInfo::statusIcon() const
{
    return GitStatusParser::getStatusIcon(status);
}

QString GitFileInfo::statusDisplayText() const
{
    if (!statusText.isEmpty()) {
        return statusText;
    }
    return GitStatusParser::getStatusDisplayText(status);
}

bool GitFileInfo::isFileStaged(GitFileStatus status)
{
    return status == GitFileStatus::Staged || status == GitFileStatus::StagedModified || status == GitFileStatus::StagedAdded || status == GitFileStatus::StagedDeleted || status == GitFileStatus::Renamed || status == GitFileStatus::Copied;
}

// ============================================================================
// GitStatusParser Implementation
// ============================================================================

GitStatusParser::GitStatusParser(QObject *parent)
    : QObject(parent)
{
}

QList<std::shared_ptr<GitFileInfo>> GitStatusParser::parseGitStatus(const QString &gitStatusOutput)
{
    QList<std::shared_ptr<GitFileInfo>> files;

    // Handle null-terminated output from git status -z
    QStringList lines;
    if (gitStatusOutput.contains('\0')) {
        // Split by null character for -z output
        lines = gitStatusOutput.split('\0', 
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts
#else
            QString::SkipEmptyParts
#endif
        );
    } else {
        // Fallback to newline split for regular output
        lines = gitStatusOutput.split('\n', 
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts
#else
            QString::SkipEmptyParts
#endif
        );
    }

    for (const QString &line : lines) {
        if (line.length() > 3) {
            const QString indexStatus = line.mid(0, 1);
            const QString workingStatus = line.mid(1, 1);
            QString filePath = line.mid(3);

            // Handle quoted filenames (Git quotes filenames with special characters)
            if (filePath.startsWith('"') && filePath.endsWith('"')) {
                filePath = unquoteGitFilename(filePath);
            }

            auto status = parseFileStatus(indexStatus, workingStatus);
            auto fileInfo = std::make_shared<GitFileInfo>(filePath, status);
            files.append(fileInfo);
        }
    }

    return files;
}

QList<std::shared_ptr<GitFileInfo>> GitStatusParser::getRepositoryStatus(const QString &repositoryPath)
{
    QProcess process;
    process.setWorkingDirectory(repositoryPath);

    // Get all changed files (staged, modified, untracked)
    // Use -z option to get null-terminated output for better handling of special characters
    process.start("git", QStringList() << "status"
                                       << "--porcelain"
                                       << "-z");

    if (process.waitForFinished(5000)) {
        const QByteArray output = process.readAllStandardOutput();
        auto files = parseGitStatus(QString::fromUtf8(output));

        qDebug() << "[GitStatusParser] Loaded" << files.size() << "changed files from repository:" << repositoryPath;
        return files;
    } else {
        qWarning() << "[GitStatusParser] Failed to get repository status:" << process.errorString();
        return {};
    }
}

QString GitStatusParser::unquoteGitFilename(const QString &quotedFilename)
{
    if (!quotedFilename.startsWith('"') || !quotedFilename.endsWith('"')) {
        return quotedFilename;
    }

    // Remove quotes
    QString filePath = quotedFilename.mid(1, quotedFilename.length() - 2);

    // Process common escape sequences
    filePath = filePath.replace("\\\"", "\"");
    filePath = filePath.replace("\\\\", "\\");
    filePath = filePath.replace("\\n", "\n");
    filePath = filePath.replace("\\t", "\t");

    // Process octal escape sequences
    filePath = processOctalEscapes(filePath);

    return filePath;
}

QString GitStatusParser::processOctalEscapes(const QString &text)
{
    QString result = text;

    // Process octal escape sequences (like \346\226\260)
    QRegularExpressionMatchIterator it = s_octalRegex.globalMatch(result);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString octalStr = match.captured(1);
        bool ok;
        int octalValue = octalStr.toInt(&ok, 8);
        if (ok) {
            QChar replacement = QChar(octalValue);
            result = result.replace(match.captured(0), replacement);
        }
    }

    return result;
}

GitFileStatus GitStatusParser::parseFileStatus(const QString &indexStatus, const QString &workingStatus)
{
    // Parse git status codes to our enum
    if (indexStatus != " " && indexStatus != "?") {
        // File is staged
        if (indexStatus == "A") {
            return GitFileStatus::StagedAdded;
        } else if (indexStatus == "M") {
            return GitFileStatus::StagedModified;
        } else if (indexStatus == "D") {
            return GitFileStatus::StagedDeleted;
        } else if (indexStatus == "R") {
            return GitFileStatus::Renamed;
        } else if (indexStatus == "C") {
            return GitFileStatus::Copied;
        } else {
            return GitFileStatus::Staged;
        }
    } else if (workingStatus == "?") {
        return GitFileStatus::Untracked;
    } else {
        // File is modified but not staged
        if (workingStatus == "M") {
            return GitFileStatus::Modified;
        } else if (workingStatus == "D") {
            return GitFileStatus::Deleted;
        } else {
            return GitFileStatus::Modified;
        }
    }
}

QIcon GitStatusParser::getStatusIcon(GitFileStatus status)
{
    switch (status) {
    case GitFileStatus::Modified:
    case GitFileStatus::StagedModified:
        return QIcon::fromTheme("document-edit");
    case GitFileStatus::Staged:
    case GitFileStatus::StagedAdded:
        return QIcon::fromTheme("list-add");
    case GitFileStatus::Deleted:
    case GitFileStatus::StagedDeleted:
        return QIcon::fromTheme("list-remove");
    case GitFileStatus::Untracked:
        return QIcon::fromTheme("document-new");
    case GitFileStatus::Renamed:
        return QIcon::fromTheme("edit-rename");
    case GitFileStatus::Copied:
        return QIcon::fromTheme("edit-copy");
    default:
        return QIcon::fromTheme("document-properties");
    }
}

QString GitStatusParser::getStatusDisplayText(GitFileStatus status)
{
    switch (status) {
    case GitFileStatus::Modified:
        return QObject::tr("Modified");
    case GitFileStatus::Staged:
        return QObject::tr("Staged");
    case GitFileStatus::StagedModified:
        return QObject::tr("Staged (Modified)");
    case GitFileStatus::StagedAdded:
        return QObject::tr("Staged (Added)");
    case GitFileStatus::StagedDeleted:
        return QObject::tr("Staged (Deleted)");
    case GitFileStatus::Deleted:
        return QObject::tr("Deleted");
    case GitFileStatus::Untracked:
        return QObject::tr("Untracked");
    case GitFileStatus::Renamed:
        return QObject::tr("Renamed");
    case GitFileStatus::Copied:
        return QObject::tr("Copied");
    default:
        return QObject::tr("Unknown");
    }
}

QString GitStatusParser::getStatusDescription(const QString &statusCode)
{
    if (statusCode.length() != 2) return QObject::tr("Unknown");

    const QChar index = statusCode.at(0);
    const QChar workTree = statusCode.at(1);

    QString desc;

    // 索引状态
    switch (index.toLatin1()) {
    case 'A':
        desc += QObject::tr("Added");
        break;
    case 'M':
        desc += QObject::tr("Modified");
        break;
    case 'D':
        desc += QObject::tr("Deleted");
        break;
    case 'R':
        desc += QObject::tr("Renamed");
        break;
    case 'C':
        desc += QObject::tr("Copied");
        break;
    case ' ':
        break;
    default:
        desc += QObject::tr("Unknown");
        break;
    }

    // 工作树状态
    if (workTree != ' ') {
        if (!desc.isEmpty()) desc += ", ";
        switch (workTree.toLatin1()) {
        case 'M':
            desc += QObject::tr("Modified in working tree");
            break;
        case 'D':
            desc += QObject::tr("Deleted in working tree");
            break;
        case '?':
            desc += QObject::tr("Untracked");
            break;
        default:
            desc += QObject::tr("Unknown working tree status");
            break;
        }
    }

    return desc.isEmpty() ? QObject::tr("Unchanged") : desc;
}

QIcon GitStatusParser::getStatusIcon(const QString &statusCode)
{
    if (statusCode.length() != 2) return QIcon();

    const QChar index = statusCode.at(0);
    const QChar workTree = statusCode.at(1);

    // 优先显示索引状态的图标
    switch (index.toLatin1()) {
    case 'A':
        return QIcon::fromTheme("list-add");
    case 'M':
        return QIcon::fromTheme("document-edit");
    case 'D':
        return QIcon::fromTheme("list-remove");
    case 'R':
        return QIcon::fromTheme("edit-rename");
    case 'C':
        return QIcon::fromTheme("edit-copy");
    default:
        // 如果索引没有状态，检查工作树状态
        switch (workTree.toLatin1()) {
        case 'M':
            return QIcon::fromTheme("document-edit");
        case 'D':
            return QIcon::fromTheme("list-remove");
        case '?':
            return QIcon::fromTheme("document-new");
        default:
            return QIcon::fromTheme("text-plain");
        }
    }
}
