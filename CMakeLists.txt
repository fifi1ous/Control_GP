cmake_minimum_required(VERSION 3.16)
project(Control_GP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Pdf PdfWidgets)

# ---- Python must be found BEFORE pybind11 ----
set(PYBIND11_FINDPYTHON ON)
find_package(Python REQUIRED COMPONENTS Interpreter Development)

include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)

set(TESSERACT_INSTALL_DIR "C:/Program Files/Tesseract-OCR"
    CACHE PATH "Directory containing libtesseract-5.dll and tessdata/")

set(TESSERACT_DLL          "${TESSERACT_INSTALL_DIR}/libtesseract-5.dll")
set(TESSERACT_TESSDATA_DIR "${TESSERACT_INSTALL_DIR}/tessdata")
set(TESSERACT_DEF          "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract/libtesseract.def")
set(TESSERACT_IMPLIB_DIR   "${CMAKE_CURRENT_BINARY_DIR}/third_party/tesseract")
set(TESSERACT_IMPLIB       "${TESSERACT_IMPLIB_DIR}/libtesseract-5.lib")

if(NOT EXISTS "${TESSERACT_DLL}")
    message(FATAL_ERROR
        "Tesseract DLL not found at:\n  ${TESSERACT_DLL}\n"
        "Install the UB Mannheim Tesseract build or pass "
        "-DTESSERACT_INSTALL_DIR=<path> to cmake.")
endif()
if(NOT EXISTS "${TESSERACT_TESSDATA_DIR}/ces.traineddata")
    message(WARNING
        "Czech traineddata not found at ${TESSERACT_TESSDATA_DIR}/ces.traineddata. "
        "OCR will fail at runtime until you install it.")
endif()

file(MAKE_DIRECTORY "${TESSERACT_IMPLIB_DIR}")

add_custom_command(
    OUTPUT  "${TESSERACT_IMPLIB}"
    COMMAND "${CMAKE_AR}" /nologo
            "/def:${TESSERACT_DEF}"
            "/out:${TESSERACT_IMPLIB}"
            /machine:x64
    DEPENDS "${TESSERACT_DEF}"
    WORKING_DIRECTORY "${TESSERACT_IMPLIB_DIR}"
    COMMENT "Generating Tesseract import library from libtesseract.def"
    VERBATIM)

add_custom_target(generate_tesseract_implib DEPENDS "${TESSERACT_IMPLIB}")

add_library(tesseract_capi SHARED IMPORTED)
set_target_properties(tesseract_capi PROPERTIES
    IMPORTED_LOCATION             "${TESSERACT_DLL}"
    IMPORTED_IMPLIB               "${TESSERACT_IMPLIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract")

add_executable(Control_GP WIN32

    # =========================
    # App (entry point)
    # =========================
    src/app/main.cpp


    # =========================
    # UI (windows, widgets, views)
    # =========================
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui

    src/ui/pdfviewerwindow.cpp
    src/ui/pdfviewerwindow.h
    src/ui/pdfviewerwindow.ui

    src/ui/displaygp.cpp
    src/ui/displaygp.h
    src/ui/displaygp.ui

    src/ui/displayzpmz.cpp
    src/ui/displayzpmz.h
    src/ui/displayzpmz.ui

    src/ui/displaygnss.cpp
    src/ui/displaygnss.h
    src/ui/displaygnss.ui

    icons.qrc


    # =========================
    # Structures (data models)
    # =========================
    src/structures/filenonpdf.cpp
    src/structures/filenonpdf.h

    src/structures/fileother.cpp
    src/structures/fileother.h

    src/structures/filepdf.cpp
    src/structures/filepdf.h

    src/structures/geometricplan.cpp
    src/structures/geometricplan.h

    src/structures/structures.h
    src/structures/vfk_maps.h

    src/structures/maps.h


    # =========================
    # Algorithms (processing logic)
    # =========================
    src/algorithms/controlgeometricplan.cpp
    src/algorithms/controlgeometricplan.h

    src/algorithms/processvfk.cpp
    src/algorithms/processvfk.h

    src/algorithms/bboxoverlay.cpp
    src/algorithms/bboxoverlay.h

    src/algorithms/pdftextselectionoverlay.cpp
    src/algorithms/pdftextselectionoverlay.h

    src/algorithms/pythonmanager.h
    src/algorithms/pythonmanager.cpp

    src/algorithms/ocrmanager.h
    src/algorithms/ocrmanager.cpp

    src/algorithms/addpath.h

    src/algorithms/controlzpmz.h
    src/algorithms/controlzpmz.cpp

    src/algorithms/controlzadost.h
    src/algorithms/controlzadost.cpp

    src/algorithms/deleteworkfiles.h
    src/algorithms/deleteworkfiles.cpp

    src/algorithms/displayresults.h
    src/algorithms/displayresults.cpp

    src/algorithms/controlpdf.h
    src/algorithms/controlpdf.cpp

    src/algorithms/parsetext.h
    src/algorithms/parsetext.cpp
)

add_definitions(-DQT_NO_KEYWORDS)

target_link_directories(Control_GP PRIVATE "${Python_LIBRARY_DIRS}")

target_link_libraries(Control_GP PRIVATE
    Qt6::Widgets
    pybind11::embed
    Qt6::Pdf
    Qt6::PdfWidgets
    ${Python_LIBRARIES}
    tesseract_capi
)

add_dependencies(Control_GP generate_tesseract_implib)

target_compile_definitions(Control_GP PRIVATE
    CONTROL_GP_TESSDATA_PATH="${TESSERACT_TESSDATA_DIR}")

target_include_directories(Control_GP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/algorithms
    ${CMAKE_CURRENT_SOURCE_DIR}/src/structures
)

set(WORKSPACE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/workspace")
set(MODELS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/models")
set(SCRIPTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
set(DATABASE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/database")

file(GLOB PYTHON_DLLS "${Python_RUNTIME_LIBRARY_DIRS}/python3*.dll")

if(WIN32)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        # Get the Python DLL directory
        get_filename_component(Python_DLL_DIR "${Python_RUNTIME_LIBRARY_DIRS}" ABSOLUTE)

        add_custom_command(TARGET Control_GP POST_BUILD
          COMMAND "${WINDEPLOYQT_EXECUTABLE}" --verbose 0 --no-compiler-runtime "$<TARGET_FILE:Control_GP>"

          # --- Sync embedded Python tree next to exe ---
          COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_CURRENT_SOURCE_DIR}/python-embed" -DDST="$<TARGET_FILE_DIR:Control_GP>/python-embed" -P "${CMAKE_CURRENT_SOURCE_DIR}/sync_dir.cmake"

          COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${TESSERACT_INSTALL_DIR}/libtesseract-5.dll"
          "${TESSERACT_INSTALL_DIR}/libleptonica-6.dll"
          "${TESSERACT_INSTALL_DIR}/libgcc_s_seh-1.dll"
          "${TESSERACT_INSTALL_DIR}/libstdc++-6.dll"
          "${TESSERACT_INSTALL_DIR}/libwinpthread-1.dll"
          "$<TARGET_FILE_DIR:Control_GP>/"

          # --- Sync data/code directories ---
          COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_CURRENT_SOURCE_DIR}/poppler" -DDST="$<TARGET_FILE_DIR:Control_GP>/poppler" -P "${CMAKE_CURRENT_SOURCE_DIR}/sync_dir.cmake"
          COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_CURRENT_SOURCE_DIR}/scripts" -DDST="$<TARGET_FILE_DIR:Control_GP>/scripts" -P "${CMAKE_CURRENT_SOURCE_DIR}/sync_dir.cmake"
          COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_CURRENT_SOURCE_DIR}/models" -DDST="$<TARGET_FILE_DIR:Control_GP>/models" -P "${CMAKE_CURRENT_SOURCE_DIR}/sync_dir.cmake"
          COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_CURRENT_SOURCE_DIR}/workspace" -DDST="$<TARGET_FILE_DIR:Control_GP>/workspace" -P "${CMAKE_CURRENT_SOURCE_DIR}/sync_dir.cmake"
          COMMAND ${CMAKE_COMMAND} -DSRC="${CMAKE_CURRENT_SOURCE_DIR}/database" -DDST="$<TARGET_FILE_DIR:Control_GP>/database" -P "${CMAKE_CURRENT_SOURCE_DIR}/sync_dir.cmake"
          COMMENT "Deploying Qt and syncing Python + scripts + models (incremental)..."
        )
    endif()
endif()
