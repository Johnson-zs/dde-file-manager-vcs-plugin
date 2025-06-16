#pragma once

#include <QString>
#include <QHash>

// Git文件状态枚举定义
enum ItemVersion {
    UnversionedVersion = 0,
    LocallyModifiedVersion = 1,
    AddedVersion = 2,
    DeletedVersion = 3,
    RenamedVersion = 4,
    CopiedVersion = 5,
    UpdatedButUnmergedVersion = 6,
    UntrackedVersion = 7,
    IgnoredVersion = 8,
    UnmodifiedVersion = 9,
    StagedVersion = 10,
    ConflictVersion = 11
};

// Git仓库信息结构
struct GitRepositoryInfo {
    QString path;
    QString branch;
    bool isDirty;
    int ahead;
    int behind;
};

// 类型别名
using GitStatusMap = QHash<QString, ItemVersion>; 