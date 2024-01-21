#include "utils.h"

#include <QProcess>

namespace Utils {

QString localRepositoryRoot(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--show-toplevel" });
    if (process.waitForReadyRead(1000) && process.exitCode() == 0)
        return QString::fromUtf8(process.readAll().chopped(1));
    return QString();
}

bool isGitRepository(const QString &directory)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "rev-parse", "--is-inside-work-tree" });
    if (process.waitForFinished(1000) && process.exitCode() == 0)
        return true;
    return false;
}

}   // namespace Utils
