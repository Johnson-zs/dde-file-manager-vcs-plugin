#ifndef UTILS_H
#define UTILS_H

#include <tuple>

#include <QString>
#include <QIODevice>

#include <global.h>

namespace Utils {
QString repositoryBaseDir(const QString &directory);
QString findPathBelowGitBaseDir(const QString &directory);
bool isInsideRepositoryDir(const QString &directory);
bool isInsideRepositoryFile(const QString &path);
int readUntilZeroChar(QIODevice *device, char *buffer, const int maxChars);
std::tuple<char, char, QString> parseLineGitStatus(const QString &line);
Global::ItemVersion parseXYState(Global::ItemVersion state, char X, char Y);
QStringList makeDirGroup(const QString &directory, const QString &relativeFileName);
bool isDirectoryEmpty(const QString &path);
bool isIgnoredDirectory(const QString &directory, const QString &path);
}   // namespace Utils

#endif   // UTILS_H
