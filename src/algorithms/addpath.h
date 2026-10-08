#ifndef ADDPATH_H
#define ADDPATH_H

#include <QCoreApplication>
#include <QDir>
#include <QString>

class AddPath
{
public:
    AddPath();

    static inline QString getWorkspacePath()
    {
         return QDir(QCoreApplication::applicationDirPath()).filePath("workspace");
    }

    static inline QString getAnnotationPath()
    {
        return QDir(AddPath::getWorkspacePath()).filePath("annotations");
    }

    static inline QString getScriptsPath()
    {
        return QDir(QCoreApplication::applicationDirPath()).filePath("scripts");
    }

    static inline QString getModelsPath()
    {
        return QDir(QCoreApplication::applicationDirPath()).filePath("models");
    }

    static inline QString getPathToModelSegment()
    {
        return QDir(AddPath::getModelsPath()).filePath("segment.pt");
    }

    static inline QString getPathToModelClassify()
    {
        return QDir(AddPath::getModelsPath()).filePath("classification.pt");
    }

    static inline QString getGPPath()
    {
        return QDir(AddPath::getWorkspacePath()).filePath("gp");
    }

    static inline QString getGPSSPath()
    {
        return QDir(AddPath::getGPPath()).filePath("ss");
    }

    static inline QString getGPMapPath()
    {
        return QDir(AddPath::getGPPath()).filePath("map");
    }

    static inline QString getGPKladPath()
    {
        return QDir(AddPath::getGPPath()).filePath("klad");
    }

    static inline QString getGPBpejPath()
    {
        return QDir(AddPath::getGPPath()).filePath("bpej");
    }

    static inline QString getGPPopispolePath()
    {
        return QDir(AddPath::getGPPath()).filePath("popispole");
    }

    static inline QString getGPVykazPath()
    {
        return QDir(AddPath::getGPPath()).filePath("vykaz");
    }

    static inline QString getZPMZPath()
    {
        return QDir(AddPath::getWorkspacePath()).filePath("zpmz");
    }

    static inline QString getZadostPath()
    {
        return QDir(AddPath::getWorkspacePath()).filePath("zadost");
    }

    static inline QString getDSPSPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("dsps");
    }

    static inline QString getGNSSPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("gnss");
    }

    static inline QString getNacrtPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("nacrt");
    }

    static inline QString getOpravPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("oprav");
    }

    static inline QString getPopispolePath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("popispole");
    }

    static inline QString getProtPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("prot");
    }


    static inline QString getSezvlastPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("sezvlast");
    }

    static inline QString getVymeryPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("vymery");
    }

    static inline QString getVytycPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("vytyc");
    }

    static inline QString getZapPath()
    {
        return QDir(AddPath::getZPMZPath()).filePath("zap");
    }

    static inline QString getClassifyPath()
    {
        return QDir(AddPath::getWorkspacePath()).filePath("classify");
    }

};

#endif // ADDPATH_H
