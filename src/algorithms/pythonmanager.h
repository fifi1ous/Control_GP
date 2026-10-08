#ifndef PYTHONMANAGER_H
#define PYTHONMANAGER_H

#include <QString>
#include <QStringList>
#include <vector>
#include <string>

#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>

#include "addpath.h"
#include "structures.h"

namespace py = pybind11;

class PythonManager
{
public:
    PythonManager();
    // Starts the embedded CPython interpreter with PYTHONHOME pointed at the bundled
    // "python-embed" directory next to the executable. Must be called exactly once
    // AFTER QApplication is constructed, BEFORE any Python code runs.
    // Throws std::runtime_error if Python initialization fails.
    static void bootstrapInterpreter();

    // Tears down the CPython interpreter. Paired with bootstrapInterpreter().
    static void shutdownInterpreter();

    // Call once after QApplication is created to set up sys.path
    // and pre-import heavy libraries (ultralytics/torch).
    static void initialize();
    static void finalize();

    static void create_annotaions(const QString& path_to_pdf);
    static void divide_pdf_by_annotations(const QString& path_to_pdf);
    static std::vector<Coordinates> extract_SS_from_GP();
    static std::vector<Coordinates> extract_SS_from_prot(const QString& path_to_pdf);
    static std::vector<InputFile> classify(const QString& folder);
    static QString get_text_from_popisople_Zadost(const QString& path_to_pdf);
    static void get_text_from_vymery_GP(std::vector<Vymery> &vymeryOld,
                                        std::vector<Vymery> &vymeryNew,
                                        std::vector<Vymery> &vymeryPor);
    static void get_text_from_vymery_Prot(  const QString &path_to_pdf,
                                            std::vector<Vymery> &vymeryNew);
    static void get_text_from_vymery_Vymery(const QString &path_to_pdf,
                                            std::vector<Vymery> &vymeryOld,
                                            std::vector<Vymery> &vymeryNew,
                                            std::vector<Vymery> &vymeryPor);
    static InputFile get_info_about_PDF(const QString& path_to_pdf);

private:
    // Lazy getters — paths must not be resolved before QApplication exists
    static const QString& pathToSegmentModel() { static const QString s = AddPath::getPathToModelSegment(); return s; }
    static const QString& pathToClassifyModel(){ static const QString s = AddPath::getPathToModelClassify();return s; }
    static const QString& pathToAnnotation()   { static const QString s = AddPath::getAnnotationPath();     return s; }
    static const QString& pathToWorkspace()    { static const QString s = AddPath::getWorkspacePath();      return s; }
    static const QString& pathToScripts()      { static const QString s = AddPath::getScriptsPath();        return s; }
    static const QString& pathToGP()           { static const QString s = AddPath::getGPPath();             return s; }
    static const QString& pathToGNSS()         { static const QString s = AddPath::getGNSSPath();           return s; }
    static const QString& pathToClassify()     { static const QString s = AddPath::getClassifyPath();       return s; }

    static bool s_interpreterRunning;
    static bool s_initialized;

    static py::object s_bridge;             // cached work_with_pdf module
    static py::object s_bridge_popispole;   // cached work_with_pdf module
    static py::object s_bridge_vymery;   // cached work_with_pdf module
    static py::object s_bridge_ss;      // cached work_with_pdf module
    static py::object s_yoloModel;          // cached YOLO model instance
    static py::object s_classify_model;  // cached YOLO model instance
};

#endif // PYTHONMANAGER_H
