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

// Git 操作状态检查函数
bool canAddFile(const QString &filePath);
bool canRemoveFile(const QString &filePath);
bool canRevertFile(const QString &filePath);
bool canShowFileLog(const QString &filePath);
Global::ItemVersion getFileGitStatus(const QString &filePath);
}   // namespace Utils

#endif   // UTILS_H
