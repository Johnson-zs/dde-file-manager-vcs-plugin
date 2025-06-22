#include "git-status-parser.h"
#include "git-utils.h"
#include <QProcess>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpressionMatchIterator>
#include <QObject>

// 静态成员初始化
QRegularExpression GitStatusParser::s_octalRegex("\\\\([0-7]{3})");

GitStatusMap GitStatusParser::parseGitStatus(const QString &gitStatusOutput)
{
    GitStatusMap statusMap;

    // Handle null-terminated output from git status -z
    QStringList lines;
    if (gitStatusOutput.contains('\0')) {
        // Split by null character for -z output
        lines = gitStatusOutput.split('\0', Qt::SkipEmptyParts);
    } else {
        // Fallback to newline split for regular output
        lines = gitStatusOutput.split('\n', Qt::SkipEmptyParts);
    }

    for (const QString &line : lines) {
        if (line.length() >= 3) {
            const char indexStatus = line[0].toLatin1();
            const char workingStatus = line[1].toLatin1();
            QString filePath = line.mid(3);

            // Handle quoted filenames (Git quotes filenames with special characters)
            if (filePath.startsWith('"') && filePath.endsWith('"')) {
                filePath = unquoteGitFilename(filePath);
            }

            auto status = parseFileStatusFromChars(indexStatus, workingStatus);
            statusMap[filePath] = status;
        }
    }

    return statusMap;
}

ItemVersion GitStatusParser::parseFileStatus(const QString &statusLine)
{
    if (statusLine.length() >= 3) {
        const char indexStatus = statusLine[0].toLatin1();
        const char workingStatus = statusLine[1].toLatin1();
        return parseFileStatusFromChars(indexStatus, workingStatus);
    }
    return UnversionedVersion;
}

ItemVersion GitStatusParser::parseFileStatusFromChars(char indexStatus, char workingStatus)
{
    // 处理冲突状态
    if ((indexStatus == 'U' && workingStatus == 'U') ||  // 两边都修改的冲突
        (indexStatus == 'A' && workingStatus == 'A') ||  // 两边都添加的冲突
        (indexStatus == 'D' && workingStatus == 'D') ||  // 两边都删除的冲突
        (indexStatus == 'U' && workingStatus != 'U') ||  // 一边冲突
        (indexStatus != 'U' && workingStatus == 'U')) {  // 另一边冲突
        return ConflictingVersion;
    }
    
    // Parse git status codes to our enum
    if (indexStatus != ' ' && indexStatus != '?') {
        // File is staged
        if (indexStatus == 'A') {
            return AddedVersion;
        } else if (indexStatus == 'M') {
            return LocallyModifiedVersion;
        } else if (indexStatus == 'D') {
            return RemovedVersion;
        } else if (indexStatus == 'R') {
            return LocallyModifiedVersion;  // Renamed files treated as modified
        } else if (indexStatus == 'C') {
            return AddedVersion;  // Copied files treated as added
        } else {
            return LocallyModifiedVersion;  // Other staged changes
        }
    } else if (workingStatus == '?') {
        return UnversionedVersion;
    } else if (workingStatus == '!') {
        return IgnoredVersion;
    } else {
        // File is modified but not staged
        if (workingStatus == 'M') {
            return LocallyModifiedUnstagedVersion;
        } else if (workingStatus == 'D') {
            return MissingVersion;  // Deleted in working tree
        } else {
            return LocallyModifiedUnstagedVersion;
        }
    }
}

GitStatusMap GitStatusParser::getRepositoryStatus(const QString &repositoryPath)
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
        auto statusMap = parseGitStatus(QString::fromUtf8(output));

        qDebug() << "[GitStatusParser] Loaded" << statusMap.size() << "changed files from repository:" << repositoryPath;
        return statusMap;
    } else {
        qWarning() << "[GitStatusParser] Failed to get repository status:" << process.errorString();
        return GitStatusMap();
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

QStringList GitStatusParser::parseGitLog(const QString &output)
{
    QStringList logEntries;
    
    // Simple implementation: split by commit boundaries
    QStringList lines = output.split('\n', Qt::KeepEmptyParts);
    QString currentEntry;
    
    for (const QString &line : lines) {
        if (line.startsWith("commit ")) {
            if (!currentEntry.isEmpty()) {
                logEntries.append(currentEntry.trimmed());
                currentEntry.clear();
            }
            currentEntry = line + "\n";
        } else {
            currentEntry += line + "\n";
        }
    }
    
    if (!currentEntry.isEmpty()) {
        logEntries.append(currentEntry.trimmed());
    }
    
    return logEntries;
}

QString GitStatusParser::getStatusDescription(ItemVersion status)
{
    switch (status) {
    case UnversionedVersion:
        return QObject::tr("Untracked");
    case NormalVersion:
        return QObject::tr("Up to date");
    case UpdateRequiredVersion:
        return QObject::tr("Update required");
    case LocallyModifiedVersion:
        return QObject::tr("Modified (staged)");
    case LocallyModifiedUnstagedVersion:
        return QObject::tr("Modified");
    case AddedVersion:
        return QObject::tr("Added");
    case RemovedVersion:
        return QObject::tr("Removed");
    case ConflictingVersion:
        return QObject::tr("Conflicted");
    case IgnoredVersion:
        return QObject::tr("Ignored");
    case MissingVersion:
        return QObject::tr("Missing");
    default:
        return QObject::tr("Unknown");
    }
}

QString GitStatusParser::getStatusDescription(const QString &statusCode)
{
    if (statusCode.length() >= 2) {
        char indexStatus = statusCode[0].toLatin1();
        char workingStatus = statusCode[1].toLatin1();
        ItemVersion status = parseFileStatusFromChars(indexStatus, workingStatus);
        return getStatusDescription(status);
    }
    return QObject::tr("Unknown");
} 