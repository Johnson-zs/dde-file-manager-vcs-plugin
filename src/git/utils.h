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
bool isGitRepositoryRoot(const QString &directoryPath);

// Git 操作状态检查函数
bool canAddFile(const QString &filePath);
bool canRemoveFile(const QString &filePath);
bool canRevertFile(const QString &filePath);
bool canShowFileLog(const QString &filePath);
Global::ItemVersion getFileGitStatus(const QString &filePath);

// 新增的操作状态检查函数
bool canShowFileDiff(const QString &filePath);
bool canShowFileBlame(const QString &filePath);

/**
 * @brief 检查文件是否可以被stash
 * @param filePath 文件路径
 * @return 如果文件有未提交的更改且可以被stash，返回true
 */
bool canStashFile(const QString &filePath);

/**
 * @brief 检查仓库是否有未提交的更改可以stash
 * @param repositoryPath 仓库路径
 * @return 如果有未提交的更改可以stash，返回true
 */
bool hasUncommittedChanges(const QString &repositoryPath);

/**
 * @brief 检查仓库是否有stash
 * @param repositoryPath 仓库路径
 * @return 如果仓库有stash，返回true
 */
bool hasStashes(const QString &repositoryPath);

/**
 * @brief 检查工作目录是否干净（没有未提交的更改）
 * @param repositoryPath 仓库路径
 * @return 如果工作目录干净，返回true
 */
bool isWorkingDirectoryClean(const QString &repositoryPath);

QString getFileStatusDescription(const QString &filePath);
QString getBranchName(const QString &repositoryPath);
}   // namespace Utils

#endif   // UTILS_H
