#include "gitstashutils.h"
#include "gitcommandexecutor.h"
#include "utils.h"

#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

// 静态正则表达式模式定义
const QRegularExpression GitStashUtils::s_stashRefPattern(R"(stash@\{(\d+)\})");
const QRegularExpression GitStashUtils::s_branchPattern(R"(On\s+(\w+):)");
const QRegularExpression GitStashUtils::s_timePattern(R"((\d+)\s+(second|minute|hour|day|week|month|year)s?\s+ago)");

QList<GitStashInfo> GitStashUtils::parseStashList(const QStringList &stashListOutput)
{
    QList<GitStashInfo> stashList;
    
    for (const QString &line : stashListOutput) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        
        GitStashInfo info = parseStashLine(line);
        if (info.isValid()) {
            stashList.append(info);
        }
    }
    
    qInfo() << "INFO: [GitStashUtils::parseStashList] Parsed" << stashList.size() << "stashes";
    return stashList;
}

GitStashInfo GitStashUtils::parseStashLine(const QString &line)
{
    // 期望格式: stash@{0}: On main: Work in progress
    // 或者带时间和作者的格式: stash@{0}|message|time ago|author
    
    if (line.contains('|')) {
        // 解析带分隔符的格式
        QStringList parts = line.split('|');
        if (parts.size() < 4) {
            qWarning() << "WARNING: [GitStashUtils::parseStashLine] Invalid stash line format:" << line;
            return GitStashInfo();
        }
        
        // 提取stash索引
        QRegularExpressionMatch match = s_stashRefPattern.match(parts[0]);
        if (!match.hasMatch()) {
            qWarning() << "WARNING: [GitStashUtils::parseStashLine] Cannot parse stash index:" << parts[0];
            return GitStashInfo();
        }
        
        int index = match.captured(1).toInt();
        QString message = parts[1].trimmed();
        QString timeAgo = parts[2].trimmed();
        QString author = parts[3].trimmed();
        
        // 解析分支信息
        QString branch = "unknown";
        QRegularExpressionMatch branchMatch = s_branchPattern.match(message);
        if (branchMatch.hasMatch()) {
            branch = branchMatch.captured(1);
        }
        
        // 解析时间
        QDateTime timestamp = parseRelativeTime(timeAgo);
        
        QString fullRef = generateStashRef(index);
        return GitStashInfo(index, message, branch, timestamp, fullRef, author, fullRef);
    } else {
        // 解析标准格式: stash@{0}: On main: Work in progress
        QRegularExpressionMatch match = s_stashRefPattern.match(line);
        if (!match.hasMatch()) {
            qWarning() << "WARNING: [GitStashUtils::parseStashLine] Cannot parse stash reference:" << line;
            return GitStashInfo();
        }
        
        int index = match.captured(1).toInt();
        
        // 提取消息部分（冒号后的内容）
        int colonIndex = line.indexOf(':', match.capturedEnd());
        QString message = line.mid(colonIndex + 1).trimmed();
        
        // 解析分支信息
        QString branch = "unknown";
        QRegularExpressionMatch branchMatch = s_branchPattern.match(message);
        if (branchMatch.hasMatch()) {
            branch = branchMatch.captured(1);
        }
        
        QString fullRef = generateStashRef(index);
        QDateTime timestamp = QDateTime::currentDateTime(); // 默认使用当前时间
        
        return GitStashInfo(index, message, branch, timestamp, fullRef, "unknown", fullRef);
    }
}

QString GitStashUtils::formatStashDisplayText(const GitStashInfo &info)
{
    if (!info.isValid()) {
        return QString();
    }
    
    return QString("stash@{%1}: %2").arg(info.index).arg(info.message);
}

QString GitStashUtils::formatStashDetailText(const GitStashInfo &info)
{
    if (!info.isValid()) {
        return QString();
    }
    
    QString detail;
    detail += QString("Stash: %1\n").arg(info.fullRef);
    detail += QString("Message: %1\n").arg(info.message);
    detail += QString("Branch: %1\n").arg(info.branch);
    detail += QString("Author: %1\n").arg(info.author);
    detail += QString("Created: %1\n").arg(info.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    detail += QString("Time ago: %1").arg(formatTimeAgo(info.timestamp));
    
    return detail;
}

bool GitStashUtils::isValidStashIndex(int index, int maxIndex)
{
    return index >= 0 && index <= maxIndex;
}

bool GitStashUtils::hasStashes(const QString &repositoryPath)
{
    return getStashCount(repositoryPath) > 0;
}

int GitStashUtils::getStashCount(const QString &repositoryPath)
{
    GitCommandExecutor executor;
    QString output, error;
    
    GitCommandExecutor::GitCommand cmd;
    cmd.command = "git";
    cmd.arguments = QStringList() << "stash" << "list";
    cmd.workingDirectory = repositoryPath;
    cmd.timeout = 5000;
    
    auto result = executor.executeCommand(cmd, output, error);
    
    if (result == GitCommandExecutor::Result::Success) {
        QStringList lines = output.split('\n', 
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts
#else
            QString::SkipEmptyParts
#endif
        );
        return lines.size();
    }
    
    return 0;
}

QString GitStashUtils::generateStashRef(int index)
{
    return QString("stash@{%1}").arg(index);
}

int GitStashUtils::extractStashIndex(const QString &stashRef)
{
    QRegularExpressionMatch match = s_stashRefPattern.match(stashRef);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }
    return -1;
}

bool GitStashUtils::isValidStashMessage(const QString &message)
{
    if (message.trimmed().isEmpty()) {
        return false;
    }
    
    // 检查消息长度（Git建议提交消息不超过72个字符）
    if (message.length() > 200) {
        return false;
    }
    
    // 检查是否包含非法字符
    if (message.contains('\n') || message.contains('\r')) {
        return false;
    }
    
    return true;
}

QString GitStashUtils::cleanStashMessage(const QString &message)
{
    QString cleaned = message.trimmed();
    
    // 移除换行符
    cleaned.replace('\n', ' ');
    cleaned.replace('\r', ' ');
    
    // 移除多余的空格
    cleaned = cleaned.simplified();
    
    // 限制长度
    if (cleaned.length() > 200) {
        cleaned = cleaned.left(197) + "...";
    }
    
    return cleaned;
}

QString GitStashUtils::generateDefaultStashMessage(const QString &branchName)
{
    QString message = "Work in progress";
    
    if (!branchName.isEmpty()) {
        message = QString("WIP on %1").arg(branchName);
    }
    
    // 添加时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    message += QString(" at %1").arg(timestamp);
    
    return message;
}

QDateTime GitStashUtils::parseRelativeTime(const QString &relativeTime)
{
    QDateTime now = QDateTime::currentDateTime();
    
    if (relativeTime.isEmpty() || relativeTime == "now") {
        return now;
    }
    
    QRegularExpressionMatch match = s_timePattern.match(relativeTime);
    if (!match.hasMatch()) {
        qWarning() << "WARNING: [GitStashUtils::parseRelativeTime] Cannot parse time:" << relativeTime;
        return now;
    }
    
    int value = match.captured(1).toInt();
    QString unit = match.captured(2);
    
    if (unit.startsWith("second")) {
        return now.addSecs(-value);
    } else if (unit.startsWith("minute")) {
        return now.addSecs(-value * 60);
    } else if (unit.startsWith("hour")) {
        return now.addSecs(-value * 3600);
    } else if (unit.startsWith("day")) {
        return now.addDays(-value);
    } else if (unit.startsWith("week")) {
        return now.addDays(-value * 7);
    } else if (unit.startsWith("month")) {
        return now.addMonths(-value);
    } else if (unit.startsWith("year")) {
        return now.addYears(-value);
    }
    
    return now;
}

QString GitStashUtils::formatTimeAgo(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return "unknown";
    }
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 seconds = dateTime.secsTo(now);
    
    if (seconds < 0) {
        return "in the future";
    }
    
    if (seconds < 60) {
        return QString("%1 second%2 ago").arg(seconds).arg(seconds == 1 ? "" : "s");
    }
    
    qint64 minutes = seconds / 60;
    if (minutes < 60) {
        return QString("%1 minute%2 ago").arg(minutes).arg(minutes == 1 ? "" : "s");
    }
    
    qint64 hours = minutes / 60;
    if (hours < 24) {
        return QString("%1 hour%2 ago").arg(hours).arg(hours == 1 ? "" : "s");
    }
    
    qint64 days = hours / 24;
    if (days < 7) {
        return QString("%1 day%2 ago").arg(days).arg(days == 1 ? "" : "s");
    }
    
    qint64 weeks = days / 7;
    if (weeks < 4) {
        return QString("%1 week%2 ago").arg(weeks).arg(weeks == 1 ? "" : "s");
    }
    
    qint64 months = days / 30;
    if (months < 12) {
        return QString("%1 month%2 ago").arg(months).arg(months == 1 ? "" : "s");
    }
    
    qint64 years = days / 365;
    return QString("%1 year%2 ago").arg(years).arg(years == 1 ? "" : "s");
} 