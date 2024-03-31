#include "utils.h"

#include <QProcess>
#include <QUrl>
#include <QDir>

#include <cache.h>

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
    const auto &repositoryPaths { Global::Cache::instance().allRepositoryPaths() };
    return std::any_of(repositoryPaths.begin(), repositoryPaths.end(), [&path](const auto &repositoryPath) {
        return path.startsWith(repositoryPath + "/") && path != repositoryPath;
    });
}

int readUntilZeroChar(QIODevice *device, char *buffer, const int maxChars)
{
    if (buffer == nullptr) {   // discard until next \0
        char c;
        while (device->getChar(&c) && c != '\0')
            ;
        return 0;
    }
    int index = -1;
    while (++index < maxChars) {
        if (!device->getChar(&buffer[index])) {
            if (device->waitForReadyRead(30000)) {   // 30 seconds to be consistent with QProcess::waitForReadyRead default
                --index;
                continue;
            } else {
                buffer[index] = '\0';
                return index <= 0 ? 0 : index + 1;
            }
        }
        if (buffer[index] == '\0') {   // line end or we put it there (see above)
            return index + 1;
        }
    }
    return maxChars;
}

std::tuple<char, char, QString> parseLineGitStatus(const QString &line)
{
    Q_ASSERT(line.size() >= 3);
    return { line[0].toLatin1(), line[1].toLatin1(), line.mid(3) };
}

Global::ItemVersion parseXYState(Global::ItemVersion state, char X, char Y)
{
    using Global::ItemVersion;
    switch (X) {
    case '!':
        state = ItemVersion::IgnoredVersion;
        break;
    case '?':
        state = ItemVersion::UnversionedVersion;
        break;
    case 'C':   // handle copied as added version
    case 'A':
        state = ItemVersion::AddedVersion;
        break;
    case 'D':
        state = ItemVersion::RemovedVersion;
        break;
    case 'M':
        state = ItemVersion::LocallyModifiedVersion;
        break;
    }

    // overwrite status depending on the working tree
    switch (Y) {
    case 'D':   // handle "deleted in working tree" as "modified in working tree"
    case 'M':
        state = ItemVersion::LocallyModifiedUnstagedVersion;
        break;
    }

    return state;
}

QStringList makeDirGroup(const QString &directory, const QString &relativeFileName)
{
    Q_ASSERT(relativeFileName.contains("/"));
    Q_ASSERT(QUrl::fromLocalFile(directory).isValid());

    QStringList group;
    int index = relativeFileName.indexOf('/');
    while (index != -1) {
        group.append(directory + relativeFileName.left(index));
        index = relativeFileName.indexOf('/', index + 1);
    }
    return group;
}

bool isDirectoryEmpty(const QString &path)
{
    QDir directory(path);
    return directory.exists() && directory.isEmpty();
}

bool isIgnoredDirectory(const QString &directory, const QString &path)
{
    QProcess process;
    process.setWorkingDirectory(directory);
    process.start("git", { "check-ignore", "-v", path });
    if (process.waitForFinished(1000) && process.exitCode() == 0) {
        const QString &result { QString::fromUtf8(process.readAll().chopped(1)) };
        return result.startsWith(".ignore");
    }

    return false;
}

}   // namespace Utils
