#pragma once

#include "git-types.h"
#include <QString>

namespace GitUtils {
    // 检查文件是否在Git仓库中
    bool isInsideRepositoryFile(const QString &filePath);
    
    // 获取文件的Git状态
    ItemVersion getFileGitStatus(const QString &filePath);
    
    // 查找文件所在的Git仓库根目录
    QString findRepositoryRoot(const QString &filePath);
    
    // 检查路径是否为Git仓库
    bool isGitRepository(const QString &path);
    
    // 获取仓库信息
    GitRepositoryInfo getRepositoryInfo(const QString &repositoryPath);
} 