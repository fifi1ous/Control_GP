#include "pythonmanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringList>
#include <stdexcept>

#include "maps.h"
#include "ocrmanager.h"
#include "parsetext.h"

bool       PythonManager::s_interpreterRunning = false;
bool       PythonManager::s_initialized = false;
py::object PythonManager::s_bridge;
py::object PythonManager::s_bridge_popispole;
py::object PythonManager::s_bridge_vymery;
py::object PythonManager::s_bridge_ss;
py::object PythonManager::s_yoloModel;
py::object PythonManager::s_classify_model;

PythonManager::PythonManager() {}


void PythonManager::bootstrapInterpreter()
{
    if (s_interpreterRunning)
        return;

    // Build absolute paths relative to the executable. This is the key change
    // that makes the app portable: everything Python needs sits next to the .exe.
    const QString exeDir  = QCoreApplication::applicationDirPath();
    const QString pyHome  = QDir(exeDir).filePath("python-embed");
    const QString pyStdlib       = pyHome + "/python313.zip";
    const QString pySitePackages = pyHome + "/Lib/site-packages";
    const QString scriptsDir     = QDir(exeDir).filePath("scripts");

    // Sanity-check: if python-embed isn't there, fail loudly with a useful message.
    if (!QFileInfo::exists(pyHome)) {
        throw std::runtime_error(
            ("Python runtime directory not found: " + pyHome).toStdString());
    }
    if (!QFileInfo::exists(pyStdlib)) {
        throw std::runtime_error(
            ("Python standard library zip not found: " + pyStdlib +
             "\n(Check that the Python version in pythonmanager.cpp matches "
             "the embeddable package you bundled.)").toStdString());
    }

    // Configure CPython.
    PyConfig config;
    PyConfig_InitIsolatedConfig(&config);
    // Isolated config = ignore PYTHONPATH/PYTHONHOME env vars, ignore the Windows
    // registry, ignore the user's site-packages. We control everything explicitly,
    // which is exactly what we want for a portable deployment.

    PyStatus status;

    // Set PYTHONHOME — tells CPython where to find its standard library.
    {
        std::wstring w = pyHome.toStdWString();
        status = PyConfig_SetString(&config, &config.home, w.c_str());
        if (PyStatus_Exception(status)) {
            PyConfig_Clear(&config);
            throw std::runtime_error("PyConfig_SetString(home) failed");
        }
    }

    // Build sys.path manually: stdlib zip, site-packages, and our scripts dir.
    config.module_search_paths_set = 1;

    auto appendPath = [&](const QString &p) {
        std::wstring w = p.toStdWString();
        PyStatus st = PyWideStringList_Append(&config.module_search_paths, w.c_str());
        if (PyStatus_Exception(st)) {
            PyConfig_Clear(&config);
            throw std::runtime_error(
                ("PyWideStringList_Append failed for: " + p).toStdString());
        }
    };

    appendPath(pyStdlib);        // stdlib (encodings, os, io, ...)
    appendPath(pyHome);          // python-embed itself (DLLs, etc.)
    appendPath(pySitePackages);  // pip-installed packages (numpy, ultralytics, ...)
    appendPath(scriptsDir);      // our own Python modules (work_with_pdf.py)

    // Fire up the interpreter.
    status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status)) {
        std::string msg = "Py_InitializeFromConfig failed";
        if (status.err_msg) {
            msg += std::string(": ") + status.err_msg;
        }
        throw std::runtime_error(msg);
    }

    s_interpreterRunning = true;
    qDebug() << "PythonManager: interpreter bootstrapped, PYTHONHOME =" << pyHome;
}

void PythonManager::shutdownInterpreter()
{
    if (!s_interpreterRunning)
        return;
    Py_Finalize();
    s_interpreterRunning = false;
}

void PythonManager::initialize()
{
    if (s_initialized)
        return;

    try {
        py::module_ sys = py::module_::import("sys");

        // Add scripts directory (your .py files)
        sys.attr("path").attr("append")(pathToScripts().toStdString());

        // Add bundled site-packages (pypdf, ultralytics, etc.)
        QString sitePackages = pathToWorkspace() + "/../python/Lib/site-packages";
        sys.attr("path").attr("insert")(0, sitePackages.toStdString());

        // Pre-import the bridge module (triggers import of ultralytics, pypdf, etc.)
        s_bridge = py::module_::import("work_with_pdf");
        s_bridge_popispole = py::module_::import("process_popispole");
        s_bridge_vymery = py::module_::import("process_vymery");
        s_bridge_ss = py::module_::import("process_ss");

        // Pre-load the YOLO model so it stays in memory
        py::module_ ultralytics = py::module_::import("ultralytics");
        s_yoloModel = ultralytics.attr("YOLO")(pathToSegmentModel().toStdString());
        s_classify_model = ultralytics.attr("YOLO")(pathToClassifyModel().toStdString());

        s_initialized = true;
        qDebug() << "PythonManager: inicializace dokončena (moduly a YOLO model načteny).";

    } catch (py::error_already_set &e) {
        qDebug() << "PythonManager: chyba při inicializaci:" << e.what();
    }
}

void PythonManager::finalize()
{
    // Assign default (null) py::object — this Py_XDECREF's the old value
    // and sets the internal pointer to nullptr.  The later static
    // destructor is then a no-op, which is critical because static
    // destruction runs AFTER py::scoped_interpreter has already called
    // Py_Finalize.  Using py::none() here would leave a live reference
    // to Py_None that would crash on Py_XDECREF after the interpreter
    // is gone.
    s_yoloModel       = py::object();
    s_classify_model  = py::object();
    s_bridge          = py::object();
    s_bridge_popispole= py::object();
    s_bridge_vymery   = py::object();
    s_bridge_ss       = py::object();
    s_initialized     = false;
}

void PythonManager::create_annotaions(const QString& path_to_pdf)
{
    if (!s_initialized)
        initialize();

    try {
        // Call the segmentation function, passing the cached YOLO model
        s_bridge.attr("segment_geometric_plan")(
            path_to_pdf.toStdString(),
            pathToGP().toStdString(),
            s_yoloModel,
            pathToAnnotation().toStdString()
        );

        qDebug() << "Segmentace a anotace byly úspěšně dokončeny.";

    } catch (py::error_already_set &e) {
        qDebug() << "Kritická chyba v Python segmentaci:" << e.what();
    }
}

void PythonManager::divide_pdf_by_annotations(const QString& path_to_pdf)
{
    if (!s_initialized)
        initialize();

    const auto& names = BB_NAMES();
    for (auto it = names.begin(); it != names.end(); ++it)
    {
        int id = it.key();
        QString abbr = it.value().abbreviation;
        QString path = it.value().path;

        // Safety: Ensure we don't pass an empty path to Python
        if (path.isEmpty()) continue;

        try {
            s_bridge.attr("divide_pdf_into_separate_pages")(
                path_to_pdf.toStdString(),
                pathToAnnotation().toStdString(),
                path.toStdString(),
                id,
                abbr.toStdString()
                );
        } catch (const std::exception& e) {
            qCritical() << "Python error on ID" << id << ":" << e.what();
        }
    }
}

std::vector<Coordinates> PythonManager::extract_SS_from_GP()
{
    if (!s_initialized)
        initialize();

    auto it = BB_NAMES().constFind(3);


    QString abbr = it.value().abbreviation;
    QString path = it.value().path;
    QString path_to_pdf = QDir(path).filePath(abbr + ".pdf");

    QString numericText;
    QString alphaText;
    std::vector<Coordinates> ss;

    QString text;

    constexpr int kMinPdfTextChars = 10;

    try {
        // Pass the "plain" mode argument directly to your Python function
        auto py_result = s_bridge_ss.attr("extract_text")(path_to_pdf.toStdString());
        text = QString::fromStdString(py_result.cast<std::string>());
    }
    catch (const std::exception& e) {
        qDebug() << "Python Error (mode: plain):" << e.what();
        text = "";
    }

    qDebug() << "Extracted length:" << text.trimmed().size();

    // 3. Final validation and parsing
    if (text.size() >= kMinPdfTextChars) {
        ss = ParseText::parseSSTextPDF(text);
    }
    else
    {
        std::vector<std::string> pages;
        try {
            auto pages_names = s_bridge_ss.attr("convert_pdf_to_images")(
                path_to_pdf.toStdString(),
                path.toStdString()
            );
            pages = pages_names.cast<std::vector<std::string>>();
        } catch (const std::exception& e) {
            qWarning() << "extract_SS_from_GP: convert_pdf_to_images selhal:" << e.what();;
        }

        QStringList numericPages;
        QStringList alphaPages;
        numericPages.reserve(static_cast<int>(pages.size()));
        alphaPages.reserve(static_cast<int>(pages.size()));
        for (const std::string &page_path : pages) {
            const QString img = QString::fromStdString(page_path);
            numericPages << OcrManager::ocrImage(img, OcrManager::OcrCharset::Numeric_coord);
            alphaPages   << OcrManager::ocrImage(img, OcrManager::OcrCharset::Alphanumeric);
        }
        numericText = numericPages.join(QStringLiteral("\n\n"));
        alphaText   = alphaPages.join(QStringLiteral("\n\n"));

        ss = ParseText::parseSSTextOCR(numericText,alphaText);
    }

    return ss;
}

std::vector<InputFile> PythonManager::classify(const QString& folder)
{
    if (!s_initialized)
        initialize();

    std::vector<InputFile> results;

    QFile gnssfile(QDir(pathToGNSS()).filePath("gnss.pdf"));
    if (gnssfile.exists())
        gnssfile.remove();

    py::object py_result = s_bridge.attr("classify_pdf")(
        folder.toStdString(),
        pathToGNSS().toStdString(),
        pathToClassify().toStdString(),
        s_classify_model
        );

    // --- convert the Python list-of-lists into InputFile structs ---
    py::list rows = py_result.cast<py::list>();
    results.reserve(rows.size());

    for (py::handle row_h : rows)
    {
        py::list row = row_h.cast<py::list>();
        const std::size_t n = row.size();

        InputFile f;

        if (n == 1) {
            f.category = QString::fromStdString(row[0].cast<std::string>());
            f.path = QDir(pathToGNSS()).filePath("gnss.pdf");
            f.format = "pdf";
        }
        else if (n == 2) {
            // ['Overeni_UOZI.txt', 'C:\\...'] — bare aux file, no category
            f.name = QString::fromStdString(row[0].cast<std::string>());
            f.path = QString::fromStdString(row[1].cast<std::string>());
            f.format = QFileInfo(f.name).suffix();
        }
        else if (n == 3) {
            // ['vfk', '...vfk', 'C:\\...'] — categorized non-PDF file
            f.category = QString::fromStdString(row[0].cast<std::string>());
            f.name     = QString::fromStdString(row[1].cast<std::string>());
            f.path     = QString::fromStdString(row[2].cast<std::string>());
            f.format = QFileInfo(f.name).suffix();
        }
        else if (n >= 11) {
            // full PDF record
            f.has_metadata = true;
            f.category = QString::fromStdString(row[0].cast<std::string>());
            f.name     = QString::fromStdString(row[1].cast<std::string>());
            f.path     = QString::fromStdString(row[2].cast<std::string>());
            f.format = QFileInfo(f.name).suffix();

            // these two arrive as strings
            f.pdf_version = QString::fromStdString(row[3].cast<std::string>()).toFloat();
            f.is_pdfa     = row[4].cast<bool>();

            // may be None for non-PDF/A files
            if (!row[5].is_none())
                f.pdfa_version_number =
                    QString::fromStdString(row[5].cast<std::string>()).toFloat();
            if (!row[6].is_none())
                f.pdfa_version_description =
                    QString::fromStdString(row[6].cast<std::string>());

            f.number_of_signatures = row[7].cast<int>();
            f.can_add_signature    = row[8].cast<bool>();
            f.number_of_pages      = row[9].cast<int>();

            for (py::handle fmt : row[10].cast<py::list>())
                f.pages_formats.push_back(
                    QString::fromStdString(fmt.cast<std::string>()));

        }
        else {
            qWarning() << "classify(): unexpected row length" << int(n);
            continue;
        }

        results.push_back(std::move(f));
    }

    // clean up generated PNGs
    QDir pngDir(pathToClassify());
    pngDir.setNameFilters({"*.png"});
    pngDir.setFilter(QDir::Files | QDir::NoSymLinks);
    for (const QFileInfo &pngInfo : pngDir.entryInfoList())
        QFile::remove(pngInfo.absoluteFilePath());

    return results;
}

std::vector<Coordinates> PythonManager::extract_SS_from_prot(const QString& path_to_pdf)
{
    std::vector<Coordinates> coordinates;

    py::object py_result = s_bridge_ss.attr("get_coordinates_from_prot")(
        path_to_pdf.toStdString()
        );

    std::vector<std::string> cpp_result = py_result.cast<std::vector<std::string>>();

    for (const auto& text : cpp_result) {
        Coordinates coordinate = ParseText::parseSSTextProt(QString::fromStdString(text));
        coordinates.push_back(coordinate);
    }

    return coordinates;
}

QString PythonManager::get_text_from_popisople_Zadost(const QString& path_to_pdf)
{
    if (!s_initialized)
        initialize();

    std::string text;
    /*auto it = BB_NAMES().constFind(4);

    QString abbr = it.value().abbreviation;
    QString path = it.value().path;
    QString path_to_pdf = QDir(path).filePath(abbr + ".pdf");*/

    try
    {
        auto py_jp = s_bridge_popispole.attr("zadost_jp")(path_to_pdf.toStdString());
        // 2. Convert python string to std::string, then to QString
        text = py_jp.cast<std::string>();

        auto py_cinnost = s_bridge_popispole.attr("over_zememericskou_cinnost")(text);
        bool cinnost = py_cinnost.cast<bool>();

        auto py_azi = s_bridge_popispole.attr("get_get_azi_number")(text);
        QString azi = QString::fromStdString(py_azi.cast<std::string>());

        QString results = QString::fromStdString(text);
        results = results + " " + azi;
        results = results + " " + (cinnost ? "1" : "0");

        return results;
    }
    catch (const py::error_already_set& e)
    {
        qWarning() << "Python error in get_text_from_popisople_Zadost:" << e.what();
        return QString::fromStdString(text);
    }
}

void PythonManager::get_text_from_vymery_GP(std::vector<Vymery> &vymeryOld,
                                            std::vector<Vymery> &vymeryNew,
                                            std::vector<Vymery> &vymeryPor)
{
    if (!s_initialized)
        initialize();

    QString text;
    auto it = BB_NAMES().constFind(1);

    QString abbr = it.value().abbreviation;
    QString path = it.value().path;
    QString path_to_pdf = QDir(path).filePath(abbr + ".pdf");

    auto py_result = s_bridge_vymery.attr("extract_vymery_gp")(path_to_pdf.toStdString());

    // --- NEW CONDITION STARTS HERE ---

    // 1. Check if the Python function returned 'None'
    if (py_result.is_none()) {
        qDebug() << "Output from Python is None. Handling empty output...";
        return;
    }

    // 2. Cast the result to a generic Python list
    py::list outer_list = py_result.cast<py::list>();

    // 3. Check if the Python list is empty (e.g., [])
    if (!outer_list.empty()) {
        for (auto item : outer_list) {
            py::list inner_list = item.cast<py::list>();

            // 1. Collect all items of this row into a QStringList
            QStringList row_data;
            for (auto inner_item : inner_list) {
                std::string str_val = py::str(inner_item).cast<std::string>();
                row_data.append(QString::fromStdString(str_val));
            }

            // Skip empty lists just in case
            if (row_data.isEmpty()) continue;

            // 2. Initialize our struct with safe defaults (to prevent memory garbage)
            Vymery v;
            v.stav_dat = 0;
            v.kmenove = 0;
            v.poddeleni = 0;
            v.vymera = 0;
            v.zpus_urc = 0;

            // First item is always the state
            v.stav_dat = row_data[0].toShort();

            // 3. Parse based on state (Old / New)
            if (v.stav_dat == 1 || v.stav_dat == 2) {
                if (row_data.size() >= 4) {
                    // Parse "cislovani" and parcel numbers (e.g., "389/1" or "st.253")
                    QString parcel_str = row_data[1];
                    if (parcel_str.startsWith("st.")) {
                        v.cislovani = "st.";
                        parcel_str.remove(0, 3); // Strip "st." to isolate the numbers
                    }

                    // Split by "/" for kmenove and poddeleni
                    QStringList parcel_parts = parcel_str.split('/');
                    if (parcel_parts.size() > 0) v.kmenove = parcel_parts[0].toInt();
                    if (parcel_parts.size() > 1) v.poddeleni = parcel_parts[1].toInt();

                    // Parse vymera and druh_poz
                    v.vymera = row_data[2].toInt();
                    v.druh_poz = row_data[3];

                    // Parse optional zpus_urc (only present in some format 2s)
                    if (row_data.size() >= 5) {
                        v.zpus_urc = row_data[4].toShort();
                    }
                }
            }
            // 4. Parse based on state (Porovnani)
            else if (v.stav_dat == 3) {
                if (row_data.size() >= 3) {
                    // Based on your note "vymera = 801-801", I am assuming index 1 and 2
                    // are the two values you want to calculate the difference of.
                    // Adjust this math if it should be (new - old) instead of (old - new).
                    int val1 = row_data[1].toInt();
                    int val2 = row_data[2].toInt();
                    v.vymera = val1 - val2;
                }
            }

            // 5. Route to the correct vector based on stav_dat
            if (v.stav_dat == 1) {
                vymeryOld.push_back(v);
            } else if (v.stav_dat == 2) {
                vymeryNew.push_back(v);
            } else if (v.stav_dat == 3) {
                vymeryPor.push_back(v);
            }
        }
    }
    else
    {
        QString numericText;
        QString alphaText;

        std::vector<std::string> pages;
        try {
            auto pages_names = s_bridge_vymery.attr("convert_pdf_to_images")(
                path_to_pdf.toStdString(),
                path.toStdString()
                );
            pages = pages_names.cast<std::vector<std::string>>();
        } catch (const std::exception& e) {
            qWarning() << "extract_SS_from_GP: convert_pdf_to_images selhal:" << e.what();;
        }

        QStringList numericPages;
        QStringList alphaPages;
        numericPages.reserve(static_cast<int>(pages.size()));
        alphaPages.reserve(static_cast<int>(pages.size()));
        for (const std::string &page_path : pages) {
            const QString img = QString::fromStdString(page_path);
            numericPages << OcrManager::ocrImage(img, OcrManager::OcrCharset::Numeric_parcel);
            alphaPages   << OcrManager::ocrImage(img, OcrManager::OcrCharset::Alphanumeric);
        }
        numericText = numericPages.join(QStringLiteral("\n\n"));
        alphaText   = alphaPages.join(QStringLiteral("\n\n"));

        qDebug()<<alphaText;
        qDebug()<<numericText;
        ParseText::parseVymeryTextOcr(numericText, alphaText,
                                      vymeryOld, vymeryNew, vymeryPor);
    }
}

void PythonManager::get_text_from_vymery_Prot(const QString &path_to_pdf, std::vector<Vymery> &vymeryNew)
{
    if (!s_initialized)
        initialize();

    try {
        py::object py_result = s_bridge_vymery.attr("extract_vymery_prot")(path_to_pdf.toStdString());

        std::vector<std::string> extracted_strings = py_result.cast<std::vector<std::string>>();

        QRegularExpression re("^(?:([A-Za-z.]+)\\s+)?(\\d+)(?:/(\\d+))?\\s+([\\d.]+)$");

        for (const auto& std_str : extracted_strings) {
            QString line = QString::fromStdString(std_str).trimmed();
            QRegularExpressionMatch match = re.match(line);

            if (match.hasMatch()) {
                Vymery v;

                v.stav_dat = 2;

                v.cislovani = match.captured(1);

                v.kmenove = match.captured(2).toInt();

                if (!match.captured(3).isEmpty()) {
                    v.poddeleni = match.captured(3).toInt();
                } else {
                    v.poddeleni = 0;
                }
                // The protocol carries the area as a float string (e.g. "123.45"
                // or even "123.0"), so toInt() would fail and yield 0. Keep the raw
                // double for display; the rounded int feeds the comparison logic,
                // which works in whole m² like the other sources.
                v.vymery_d = match.captured(4).toDouble();
                v.vymera = qRound(v.vymery_d);

                v.zpus_urc = 0;
                v.druh_poz = "";
                vymeryNew.push_back(v);
            }
        }
    } catch (const py::error_already_set &e) {

    } catch (const py::cast_error &e) {

    }
}
void PythonManager::get_text_from_vymery_Vymery(const QString &path_to_pdf,
                                                std::vector<Vymery> &vymeryOld,
                                                std::vector<Vymery> &vymeryNew,
                                                std::vector<Vymery> &vymeryPor)
{
    if (!s_initialized)
        initialize();

    auto py_result = s_bridge_vymery.attr("extract_vymery_vypocet")(path_to_pdf.toStdString());

    if (py_result.is_none()) {
        qDebug() << "Output from Python is None. Handling empty output...";
        return;
    }

    // 2. Cast the result to a generic Python list
    py::list outer_list = py_result.cast<py::list>();

    // 3. Check if the Python list is empty (e.g., [])
    if (!outer_list.empty()){
        for (auto item : outer_list) {
            py::list inner_list = item.cast<py::list>();

            // 1. Collect all items of this row into a QStringList
            QStringList row_data;
            for (auto inner_item : inner_list) {
                std::string str_val = py::str(inner_item).cast<std::string>();
                row_data.append(QString::fromStdString(str_val));
            }

            // Skip empty lists just in case
            if (row_data.isEmpty()) continue;

            // 2. Initialize our struct with safe defaults (to prevent memory garbage)
            Vymery v;
            v.stav_dat = 0;
            v.kmenove = 0;
            v.poddeleni = 0;
            v.vymera = 0;
            v.zpus_urc = 0;

            // First item is always the state
            v.stav_dat = row_data[0].toShort();

            // 3. Parse based on state (Old / New)
            if (v.stav_dat == 2) {
                    // Parse "cislovani" and parcel numbers (e.g., "389/1" or "st.253")
                    QString parcel_str = row_data[1];
                    if (parcel_str.startsWith("st.")) {
                        v.cislovani = "st.";
                        parcel_str.remove(0, 3); // Strip "st." to isolate the numbers
                    }

                    // Split by "/" for kmenove and poddeleni
                    QStringList parcel_parts = parcel_str.split('/');
                    if (parcel_parts.size() > 0) v.kmenove = parcel_parts[0].toInt();
                    if (parcel_parts.size() > 1) v.poddeleni = parcel_parts[1].toInt();

                    // Parse vymera and druh_poz
                    v.zpus_urc = row_data[2].toInt();
                    v.vymera = row_data[3].toInt();

                    // Parse optional zpus_urc (only present in some format 2s)
                    if (row_data.size() >= 5) {
                        v.zpus_urc = row_data[4].toShort();
                    }
            }
            else if (v.stav_dat == 1)
            {
                QString parcel_str = row_data[1];
                if (parcel_str.startsWith("st.")) {
                    v.cislovani = "st.";
                    parcel_str.remove(0, 3); // Strip "st." to isolate the numbers
                }

                // Split by "/" for kmenove and poddeleni
                QStringList parcel_parts = parcel_str.split('/');
                if (parcel_parts.size() > 0) v.kmenove = parcel_parts[0].toInt();
                if (parcel_parts.size() > 1) v.poddeleni = parcel_parts[1].toInt();

                v.vymera = row_data[2].toInt();

            }
            // 4. Parse based on state (Porovnani)
            else if (v.stav_dat == 3) {
                if (row_data.size() >= 3) {
                    // Based on your note "vymera = 801-801", I am assuming index 1 and 2
                    // are the two values you want to calculate the difference of.
                    // Adjust this math if it should be (new - old) instead of (old - new).
                    int val1 = row_data[1].toInt();
                    int val2 = row_data[2].toInt();
                    v.vymera = val1 - val2;
                }
            }

            // 5. Route to the correct vector based on stav_dat
            if (v.stav_dat == 1) {
                vymeryOld.push_back(v);
            } else if (v.stav_dat == 2) {
                vymeryNew.push_back(v);
            } else if (v.stav_dat == 3) {
                vymeryPor.push_back(v);
            }
        }
    }
}

InputFile PythonManager::get_info_about_PDF(const QString& path_to_pdf)
{
    if (!s_initialized)
        initialize();

    // Initialize the struct and populate basic file info
    InputFile f;
    QFileInfo fileInfo(path_to_pdf);

    f.path = path_to_pdf;
    f.name = fileInfo.fileName();
    f.format = fileInfo.suffix();

    try {
        // Call the Python function
        py::object py_result = s_bridge.attr("get_info_about_pdf")(path_to_pdf.toStdString());

        // Cast result to a Python list
        py::list row = py_result.cast<py::list>();
        const std::size_t n = row.size();

        if (n >= 8) {
            f.has_metadata = true;

            // Map the 8 Python list elements to the InputFile struct
            f.pdf_version = QString::fromStdString(row[0].cast<std::string>()).toFloat();
            f.is_pdfa     = row[1].cast<bool>();

            // Handle potential None values for PDF/A details
            if (!row[2].is_none())
                f.pdfa_version_number = QString::fromStdString(row[2].cast<std::string>()).toFloat();
            if (!row[3].is_none())
                f.pdfa_version_description = QString::fromStdString(row[3].cast<std::string>());

            f.number_of_signatures = row[4].cast<int>();
            f.can_add_signature    = row[5].cast<bool>();
            f.number_of_pages      = row[6].cast<int>();

            // Parse the list of page formats
            for (py::handle fmt : row[7].cast<py::list>()) {
                f.pages_formats.push_back(QString::fromStdString(fmt.cast<std::string>()));
            }
        }
        else {
            qWarning() << "get_info_about_PDF(): unexpected row length" << int(n);
        }
    }
    catch (const py::error_already_set& e) {
        qWarning() << "Python error in get_info_about_pdf:" << e.what();
    }

    return f;
}

