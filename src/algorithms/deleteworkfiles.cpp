#include "deleteworkfiles.h"

# include "addpath.h"

DeleteWorkFiles::DeleteWorkFiles() {}

void DeleteWorkFiles::deleteFiles()
{
    deleteFilesInFolder(AddPath::getAnnotationPath());
    deleteFilesInFolder(AddPath::getGPPath());
    deleteFilesInFolder(AddPath::getGPBpejPath());
    deleteFilesInFolder(AddPath::getGPPopispolePath());
    deleteFilesInFolder(AddPath::getGPSSPath());
    deleteFilesInFolder(AddPath::getGPMapPath());
    deleteFilesInFolder(AddPath::getGPVykazPath());
}


void DeleteWorkFiles::deleteFilesInFolder(const QString& path)
{
    QDir dir(path);

    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);

    QFileInfoList fileList = dir.entryInfoList();
    for (const QFileInfo &fileInfo : fileList) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.remove()) {
            qDebug() << "Deleted:" << fileInfo.fileName();
        } else {
            qWarning() << "Failed to delete:" << fileInfo.fileName();
        }
    }
}