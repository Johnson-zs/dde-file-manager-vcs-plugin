#pragma once

#include <QString>
#include <QHash>

// Git文件状态枚举定义 - 与Global::ItemVersion保持兼容
enum ItemVersion {
    /** The file is not under version control. */
    UnversionedVersion = 0,
    /**
     * The file is under version control and represents
     * the latest version.
     */
    NormalVersion = 1,
    /**
     * The file is under version control and a newer
     * version exists on the main branch.
     */
    UpdateRequiredVersion = 2,
    /**
     * The file is under version control and has been
     * modified locally. All modifications will be part
     * of the next commit.
     */
    LocallyModifiedVersion = 3,
    /**
     * The file has not been under version control but
     * has been marked to get added with the next commit.
     */
    AddedVersion = 4,
    /**
     * The file is under version control but has been marked
     * for getting removed with the next commit.
     */
    RemovedVersion = 5,
    /**
     * The file is under version control and has been locally
     * modified. A modification has also been done on the main
     * branch.
     */
    ConflictingVersion = 6,
    /**
     * The file is under version control and has local
     * modifications, which will not be part of the next
     * commit (or are "unstaged" in git jargon).
     */
    LocallyModifiedUnstagedVersion = 7,
    /**
     * The file is not under version control and is listed
     * in the ignore list of the version control system.
     */
    IgnoredVersion = 8,
    /**
     * The file is tracked by the version control system, but
     * is missing in the directory (e.g. by deleted without using
     * a version control command).
     */
    MissingVersion = 9
};

// Git仓库信息结构
struct GitRepositoryInfo {
    QString path;
    QString branch;
    bool isDirty;
    int ahead;
    int behind;
    
    GitRepositoryInfo() : isDirty(false), ahead(0), behind(0) {}
};

// 类型别名
using GitStatusMap = QHash<QString, ItemVersion>;

// 进程适配标识
#ifdef GIT_PLUGIN_PROCESS
#define GIT_PROCESS_TYPE_PLUGIN
#elif defined(GIT_DIALOG_PROCESS)
#define GIT_PROCESS_TYPE_DIALOG
#elif defined(GIT_DAEMON_PROCESS)
#define GIT_PROCESS_TYPE_DAEMON
#endif 