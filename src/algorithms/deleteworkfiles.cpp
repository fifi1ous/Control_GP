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
    // Resolve symlinks/junctions first and refuse anything that does not
    // really live inside workspace/. A missing folder has nothing to delete.
    const QString workspace = QDir(AddPath::getWorkspacePath()).canonicalPath();
    const QString target = QDir(path).canonicalPath();
    if (workspace.isEmpty() || target.isEmpty())
        return;
    if (!target.startsWith(workspace + '/', Qt::CaseInsensitive)) {
        qWarning() << "Refusing to delete outside workspace:" << path;
        return;
    }

    QDir dir(target);

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