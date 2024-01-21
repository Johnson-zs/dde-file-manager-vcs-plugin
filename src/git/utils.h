#ifndef UTILS_H
#define UTILS_H

#include <QString>

namespace Utils {
QString localRepositoryRoot(const QString &directory);
bool isGitRepository(const QString &directory);
}   // namespace Utils

#endif   // UTILS_H
