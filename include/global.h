#ifndef GLOBAL_H
#define GLOBAL_H

#include <QObject>
#include <QHash>

namespace Global {
enum class ItemVersion {
    /** The file is not under version control. */
    UnversionedVersion,
    /**
     * The file is under version control and represents
     * the latest version.
     */
    NormalVersion,
    /**
     * The file is under version control and a newer
     * version exists on the main branch.
     */
    UpdateRequiredVersion,
    /**
     * The file is under version control and has been
     * modified locally. All modifications will be part
     * of the next commit.
     */
    LocallyModifiedVersion,
    /**
     * The file has not been under version control but
     * has been marked to get added with the next commit.
     */
    AddedVersion,
    /**
     * The file is under version control but has been marked
     * for getting removed with the next commit.
     */
    RemovedVersion,
    /**
     * The file is under version control and has been locally
     * modified. A modification has also been done on the main
     * branch.
     */
    ConflictingVersion,
    /**
     * The file is under version control and has local
     * modifications, which will not be part of the next
     * commit (or are "unstaged" in git jargon).
     * @since 4.6
     */
    LocallyModifiedUnstagedVersion,
    /**
     * The file is not under version control and is listed
     * in the ignore list of the version control system.
     * @since 4.8
     */
    IgnoredVersion,
    /**
     * The file is tracked by the version control system, but
     * is missing in the directory (e.g. by deleted without using
     * a version control command).
     * @since 4.8
     */
    MissingVersion
};

}   // namespace Global

#endif   // GLOBAL_H
