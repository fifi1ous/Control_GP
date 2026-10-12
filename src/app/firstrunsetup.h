#ifndef FIRSTRUNSETUP_H
#define FIRSTRUNSETUP_H

// Checks the runtime dependencies before the embedded Python starts.
//
// A released build ships without the Python packages from requirements.txt
// and without Poppler. ensureDependencies() checks both on every start and,
// when something is missing, asks the user:
//   - Python packages: download them now (runs tools/setup_deps.ps1 -Only python),
//   - Poppler: download automatically (setup_deps.ps1 -Only poppler), or
//     download it manually in the browser and unzip it into the app folder.
//
// The Poppler folder that was found is passed to the Python scripts in the
// environment variable CONTROL_GP_POPPLER_BIN.
//
// In a developer build (no tools/setup_deps.ps1 next to the exe) nothing is
// downloaded, because CMake syncs the dependencies next to the exe.
namespace FirstRunSetup
{
    // Returns true when the application can start. Call after QApplication is
    // constructed and before PythonManager::bootstrapInterpreter().
    bool ensureDependencies();
}

#endif // FIRSTRUNSETUP_H
