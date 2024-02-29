#include "utils.h"

#include <QProcess>

namespace Utils {

QString repositoryBaseDir(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--show-toplevel" });
    if (process.waitForReadyRead(1000) && process.exitCode() == 0)
        return QString::fromUtf8(process.readAll().chopped(1));
    return QString();
}

QString findPathBelowGitBaseDir(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--show-prefix" });
    QString dirBelowBaseDir;
    while (process.waitForReadyRead()) {
        char buffer[512];
        while (process.readLine(buffer, sizeof(buffer)) > 0) {
            dirBelowBaseDir = QString(buffer).trimmed();   // ends in "/" or is empty
        }
    }
    return dirBelowBaseDir;
}

bool isInsideRepositoryDir(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--is-inside-work-tree" });
    if (process.waitForFinished(1000) && process.exitCode() == 0)
        return true;
    return false;
}

bool isInsideRepositoryFile(const QString &path)
{
    // TODO: cache
    return false;
}

}   // namespace Utils
