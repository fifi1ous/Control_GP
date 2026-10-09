#ifndef DELETEWORKFILES_H
#define DELETEWORKFILES_H

#include <QDir>
#include <QFile>
#include <QFileInfo>

# include "addpath.h"

class DeleteWorkFiles
{
public:
    DeleteWorkFiles();

    static void deleteFiles();

private:
    static void deleteFilesInFolder(const QString& path);
};

#endif // DELETEWORKFILES_H
