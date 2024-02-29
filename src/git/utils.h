#ifndef UTILS_H
#define UTILS_H

#include <QString>

namespace Utils {
QString repositoryBaseDir(const QString &directory);
QString findPathBelowGitBaseDir(const QString &directory);
bool isInsideRepositoryDir(const QString &directory);
bool isInsideRepositoryFile(const QString &path);
}   // namespace Utils

#endif   // UTILS_H
