#include "mainwindow.h"
#include "pythonmanager.h"

#include <QApplication>
#include <pybind11/embed.h>
namespace py = pybind11;

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    // Initialize the Qt GUI application framework.
    // Must come first — PythonManager::bootstrapInterpreter() needs
    // QCoreApplication::applicationDirPath() to locate the bundled Python runtime.
    QApplication a(argc, argv);

    // Bootstrap the embedded Python interpreter with a configuration that points
    // PYTHONHOME at the bundled "python-embed" directory next to the executable.
    // This makes the app portable — no system Python required on the target machine.
    try {
        PythonManager::bootstrapInterpreter();
    } catch (const std::exception &e) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("Python initialization failed"),
                              QString::fromUtf8(e.what()));
        return 1;
    }

    // Load Python modules and pre-instantiate YOLO models.
    PythonManager::initialize();

    // Construct and display the main application window.
    MainWindow w;
    w.show();

    // Enter the Qt event loop.
    int result = a.exec();

    // Clean up Python objects before the interpreter shuts down.
    PythonManager::finalize();

    // Shut down the interpreter itself (paired with bootstrapInterpreter).
    PythonManager::shutdownInterpreter();

    return result;
}
