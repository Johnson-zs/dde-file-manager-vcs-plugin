#pragma once

#include "git-types.h"
#include <QString>
#include <QStringList>

class GitStatusParser
{
public:
    static GitStatusMap parseGitStatus(const QString &output);
    static ItemVersion parseFileStatus(const QString &statusLine);
    static QStringList parseGitLog(const QString &output);
    
private:
    GitStatusParser() = default;
}; 