#pragma once

#include "git-types.h"
#include <QString>
#include <QStringList>
#include <tuple>

class QIODevice;

namespace GitUtils {
    // === 仓库检测函数 ===
    
    /**
     * @brief 获取仓库根目录
     * @param directory 目录路径
     * @return 仓库根目录路径，如果不在仓库中返回空字符串
     */
    QString repositoryBaseDir(const QString &directory);
    
    /**
     * @brief 查找相对于仓库根目录的路径
     * @param directory 目录路径
     * @return 相对路径，以"/"结尾或为空
     */
    QString findPathBelowGitBaseDir(const QString &directory);
    
    /**
     * @brief 检查目录是否在Git仓库的工作树中
     * @param directory 目录路径
     * @return 如果在工作树中返回true
     */
    bool isInsideRepositoryDir(const QString &directory);
    
    /**
     * @brief 检查文件是否在Git仓库中
     * @param filePath 文件路径
     * @return 如果在仓库中返回true
     */
    bool isInsideRepositoryFile(const QString &filePath);
    
    /**
     * @brief 检查路径是否为Git仓库根目录
     * @param directoryPath 目录路径
     * @return 如果是仓库根目录返回true
     */
    bool isGitRepositoryRoot(const QString &directoryPath);
    
    // === 文件状态函数 ===
    
    /**
     * @brief 获取文件的Git状态
     * @param filePath 文件路径
     * @return 文件的Git状态
     */
    ItemVersion getFileGitStatus(const QString &filePath);
    
    /**
     * @brief 获取文件状态描述
     * @param filePath 文件路径
     * @return 状态描述字符串
     */
    QString getFileStatusDescription(const QString &filePath);
    
    // === 操作权限检查函数 ===
    
    /**
     * @brief 检查文件是否可以添加到Git
     * @param filePath 文件路径
     * @return 如果可以添加返回true
     */
    bool canAddFile(const QString &filePath);
    
    /**
     * @brief 检查文件是否可以从Git中移除
     * @param filePath 文件路径
     * @return 如果可以移除返回true
     */
    bool canRemoveFile(const QString &filePath);
    
    /**
     * @brief 检查文件是否可以恢复
     * @param filePath 文件路径
     * @return 如果可以恢复返回true
     */
    bool canRevertFile(const QString &filePath);
    
    /**
     * @brief 检查文件是否可以显示日志
     * @param filePath 文件路径
     * @return 如果可以显示日志返回true
     */
    bool canShowFileLog(const QString &filePath);
    
    /**
     * @brief 检查文件是否可以显示差异
     * @param filePath 文件路径
     * @return 如果可以显示差异返回true
     */
    bool canShowFileDiff(const QString &filePath);
    
    /**
     * @brief 检查文件是否可以显示归属信息
     * @param filePath 文件路径
     * @return 如果可以显示归属返回true
     */
    bool canShowFileBlame(const QString &filePath);
    
    /**
     * @brief 检查文件是否可以被stash
     * @param filePath 文件路径
     * @return 如果文件有未提交的更改且可以被stash，返回true
     */
    bool canStashFile(const QString &filePath);
    
    // === 仓库状态函数 ===
    
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
    
    /**
     * @brief 获取当前分支名称
     * @param repositoryPath 仓库路径
     * @return 分支名称
     */
    QString getBranchName(const QString &repositoryPath);
    
    /**
     * @brief 获取仓库信息
     * @param repositoryPath 仓库路径
     * @return 仓库信息结构
     */
    GitRepositoryInfo getRepositoryInfo(const QString &repositoryPath);
    
    // === 工具函数 ===
    
    /**
     * @brief 从设备读取直到遇到零字符
     * @param device IO设备
     * @param buffer 缓冲区
     * @param maxChars 最大字符数
     * @return 读取的字符数
     */
    int readUntilZeroChar(QIODevice *device, char *buffer, const int maxChars);
    
    /**
     * @brief 解析Git状态行
     * @param line 状态行
     * @return 索引状态、工作树状态和文件名的元组
     */
    std::tuple<char, char, QString> parseLineGitStatus(const QString &line);
    
    /**
     * @brief 解析XY状态码
     * @param state 当前状态
     * @param X 索引状态字符
     * @param Y 工作树状态字符
     * @return 解析后的状态
     */
    ItemVersion parseXYState(ItemVersion state, char X, char Y);
    
    /**
     * @brief 创建目录组
     * @param directory 基础目录
     * @param relativeFileName 相对文件名
     * @return 目录组列表
     */
    QStringList makeDirGroup(const QString &directory, const QString &relativeFileName);
    
    /**
     * @brief 检查目录是否为空
     * @param path 目录路径
     * @return 如果目录为空返回true
     */
    bool isDirectoryEmpty(const QString &path);
    
    /**
     * @brief 检查目录是否被忽略
     * @param directory 基础目录
     * @param path 检查路径
     * @return 如果被忽略返回true
     */
    bool isIgnoredDirectory(const QString &directory, const QString &path);
} 