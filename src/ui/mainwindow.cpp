/**
 * @file mainwindow.cpp
 * @brief Implementation of MainWindow.
 */

#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QTextCursor>
#include <QtDebug>
#include <QInputDialog>
#include <QPushButton>

#include "addpath.h"
#include "deleteworkfiles.h"
#include "displayresults.h"
#include "controlpdf.h"
#include "processvfk.h"
#include "parsetext.h"
#include "maps.h"

namespace {

/**
 * @brief Populate a freshly loaded file slot of the GeometricPlan.
 *
 * Stores the path, derives the display name from it and marks the slot
 * present, so a manually loaded file carries the same state in the
 * GeometricPlan that a classified file would (present/name) rather than just
 * a path. Works for any slot type exposing setPath/setName/setPresent.
 */
template<typename FileT>
void markLoaded(FileT& file, const QString& path)
{
    file.setPath(path);
    file.setName(QFileInfo(path).fileName());
    file.setPresent(true);
}

} // namespace


/**
 * @brief Construct the main window and load the .ui.
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , GP()
    , controlGP(GP)
    , controlZadost(GP)
    , controlZPMZ(GP)
{
    ui->setupUi(this);
}

/**
 * @brief Destructor — frees the .ui object.
 */
MainWindow::~MainWindow()
{
    delete ui;
}


/**
 * @brief Display @p text in the results QTextEdit (replaces any previous content).
 */
void MainWindow::displayText(const QString& text)
{
    ui->textEditResults->setPlainText(text);
}


/**
 * @brief Append @p text to the results QTextEdit (preserves existing content).
 *
 * Uses moveCursor + insertPlainText rather than QTextEdit::append() so that
 * pre-formatted ASCII tables aren't separated from prior content by an
 * implicit empty paragraph.
 */
void MainWindow::appendText(const QString& text)
{
    QTextCursor cursor = ui->textEditResults->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!ui->textEditResults->toPlainText().isEmpty())
        cursor.insertText(QStringLiteral("\n"));
    cursor.insertText(text);
    ui->textEditResults->setTextCursor(cursor);
}


/**
 * @brief Clear all displayed text from the results QTextEdit.
 */
void MainWindow::clearText()
{
    ui->textEditResults->clear();
}


/**
 * @brief Open a file dialog and populate the matching ZPMZ slot for @p type.
 *
 * Helper used by every "Načíst …" ZPMZ menu action so the dialog/persist
 * boilerplate isn't repeated for each file type. ControlZPMZ only resolves
 * the slot; deriving the display name (and, for the VFK file, the format
 * from the suffix) and marking it present is done here.
 */
void MainWindow::loadFile(QString what_it_is)
{
    const QString name = NAMESMAP.value(what_it_is);
    const QString title = tr("Otevřít soubor: %1").arg(name);

    QString fileName = QFileDialog::getOpenFileName(this, title, "", tr("PDF dokument (*.pdf)"));
    if (!fileName.isEmpty())
    {
        FilePDF file;
        file.setPresent(true);
        file.setPath(fileName);
        file.setWhatItIs(what_it_is);
        placeIntoCategory(file, what_it_is);
    }
}


/**
 * @brief Display the file currently held by ControlZPMZ for @p type in a DisplayZPMZ viewer.
 *
 * Selects the file's current class in the viewer's dropdown (empty for files
 * whose class is not offered, e.g. GNSS) and connects the dropdown so that
 * choosing a different class reclassifies the file via reclassifyZPMZ().
 */
void MainWindow::viewZPMZFile(ControlZPMZ::FileType type)
{
    // Vfk's slot is a FileNonPDF; every other type resolves to a FilePDF. Both
    // expose getPath()/getPresent(), so checkAndDisplayPDF handles either — but
    // they are distinct types, so the branch must pass the right slot accessor.
    DisplayZPMZ* viewer = (type == ControlZPMZ::Vfk)
                              ? checkAndDisplayPDF<DisplayZPMZ>(controlZPMZ.vfk())
                              : checkAndDisplayPDF<DisplayZPMZ>(controlZPMZ.pdf(type));
    if (!viewer)
        return;

    const QString currentCategory = (type == ControlZPMZ::Vfk)
                                        ? controlZPMZ.vfk().getWhatItIs()
                                        : controlZPMZ.pdf(type).getWhatItIs();
    viewer->setCurrentCategory(currentCategory);

    // checkAndDisplayPDF may hand back a window that is already open for this
    // path; drop any earlier categoryChanged handler first so a reused viewer
    // does not accumulate duplicate connections (which would reclassify twice).
    disconnect(viewer, &DisplayZPMZ::categoryChanged, this, nullptr);

    // Reclassifying moves the file out of `type`'s slot, so the viewer's context
    // is no longer valid afterwards — close it; the file is now reachable from
    // the menu of its newly selected class.
    connect(viewer, &DisplayZPMZ::categoryChanged, this,
            [this, viewer, type](const QString& newCategory) {
                reclassifyZPMZ(type, newCategory);
                viewer->close();
            });
}


/**
 * @brief Move the file opened for @p fromType into the class @p newCategory.
 *
 * See the header for the contract. The file is copied before its old slot is
 * cleared, so reclassifying into the same slot (or any other) never loses data;
 * the target class is overwritten if already occupied.
 */
void MainWindow::reclassifyZPMZ(ControlZPMZ::FileType fromType, const QString& newCategory)
{
    if (newCategory.isEmpty())
        return;

    // Take a copy of the file in the opened slot, then empty that slot.
    FilePDF moved;
    if (fromType == ControlZPMZ::Vfk)
    {
        // The VFK slot is a FileNonPDF; carry over the fields a PDF slot shares.
        const FileNonPDF& src = controlZPMZ.vfk();
        moved.setPresent(src.getPresent());
        moved.setName(src.getName());
        moved.setPath(src.getPath());
        controlZPMZ.vfk().clearAll();
    }
    else
    {
        moved = controlZPMZ.pdf(fromType);
        controlZPMZ.clearFile(fromType);
    }

    moved.setWhatItIs(newCategory);
    placeIntoCategory(moved, newCategory);
}


/**
 * @brief Write @p file into the GeometricPlan slot named by @p categoryKey (overwriting it).
 */
void MainWindow::placeIntoCategory(const FilePDF& file, const QString& categoryKey)
{
    if      (categoryKey == QStringLiteral("popispole")) GP.setPopispole(file);
    else if (categoryKey == QStringLiteral("nacrt"))     GP.setNacrt(file);
    else if (categoryKey == QStringLiteral("zap"))       GP.setZap(file);
    else if (categoryKey == QStringLiteral("prot"))      GP.setProt(file);
    else if (categoryKey == QStringLiteral("vymery"))    GP.setVymery(file);
    else if (categoryKey == QStringLiteral("sezvlast"))  GP.setSezvlast(file);
    else if (categoryKey == QStringLiteral("oprav"))     GP.setOprav(file);
    else if (categoryKey == QStringLiteral("dsps"))      GP.setDsps(file);
    else if (categoryKey == QStringLiteral("vytyc"))     GP.setVytyc(file);
    else if (categoryKey == QStringLiteral("gp"))        GP.setGp(file);
    else if (categoryKey == QStringLiteral("zadost"))    GP.setZadost(file);
}


/**
 * @brief Display a segmented GP part in a DisplayZPMZ viewer, or warn if it is unavailable.
 *
 * See the header for the contract. Two distinct conditions are reported:
 * an empty annotation folder means segmentation was never run, while a
 * missing "<abbr>.pdf" after segmentation means the GP does not contain
 * this part (the divider writes no file for a class with no bounding boxes).
 */
void MainWindow::viewGPPart(const QString& partDir, const QString& abbr, const QString& partLabel)
{
    // No annotations at all → segmentation has not been run yet.
    QDir annotationDir(AddPath::getAnnotationPath());
    if (!annotationDir.exists() ||
        annotationDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
    {
        QMessageBox::warning(this, tr("Žádné výsledky"), tr("Nebyla provedena segmentace GP"));
        return;
    }

    // The cropped part PDFs are cut out of the GP on demand from the
    // annotations. Segmentation only writes the annotation files, so if this
    // part has not been divided out yet, run the divider now to populate the
    // crop folders before we try to display it.
    const QString partPath = QDir(partDir).filePath(abbr + QStringLiteral(".pdf"));
    if (!QFileInfo::exists(partPath))
    {
        PythonManager::divide_pdf_by_annotations(GP.getGp().getPath());
    }

    // Segmentation ran, but this particular part was not detected in the GP
    // (the divider writes no file for a class with no bounding boxes).
    if (!QFileInfo::exists(partPath))
    {
        QMessageBox::warning(this, tr("Část GP nenalezena"),
                             tr("GP neobsahuje část: %1").arg(partLabel));
        return;
    }

    // Display the cropped part, not the whole GP.
    FilePDF part;
    part.setPresent(true);
    part.setPath(partPath);
    part.setName(abbr + QStringLiteral(".pdf"));
    checkAndDisplayPDF<DisplayGNSS>(part);
}


/**
 * @brief Action: load a geometric-plan PDF and store its path in #controlGP.
 */
void MainWindow::on_actionNactiGP_triggered()
{
    loadFile("gp");
}


/**
 * @brief Action: bulk-import dialog (placeholder — selection is not yet wired anywhere).
 */
void MainWindow::on_actionOtevri_triggered()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Import souborů"),
        QString(),
        tr("PDF dokument (*.pdf);;ZIP archiv (*.zip);;Výměnný formát katastru (*.vfk);;Textový soubor (*.txt);;Časové razítko (*.tsr);;Podpis (*.p7s);;Všechny soubory (*.*)")
        );
}


/**
 * @brief Action: folder-import dialog (placeholder — selection is not yet wired anywhere).
 */
void MainWindow::on_actionImportSlozky_triggered()
{
    folder = QFileDialog::getExistingDirectory(
        this,
        tr("Import složky"),
        QString()
        );
}


/**
 * @brief Action: open the loaded GP in a DisplayGP viewer (with bounding-box overlay).
 */
void MainWindow::on_actionNahledUprava_triggered()
{
    checkAndDisplayPDF<DisplayGP>(GP.getGp());
}



/**
 * @brief Action: load the Žádost PDF into #controlZadost.
 */
void MainWindow::on_actionNacistZadost_triggered()
{
    loadFile("zadost");
}


/**
 * @brief Action: open the loaded Žádost PDF in a DisplayZPMZ viewer.
 */
void MainWindow::on_actionNahled_triggered()
{
    checkAndDisplayPDF<DisplayZPMZ>(GP.getZadost());
}


// ─── ZPMZ load/view action pairs ─────────────────────────────────────
// Each "Na_st_*" action delegates to loadZPMZFile() with the matching
// ControlZPMZ::FileType, dialog title and file filter; each "N_hled_*"
// action delegates to viewZPMZFile() with the same FileType. The slot
// names are auto-generated by Qt Designer from the .ui action names and
// follow Qt's auto-connection convention (on_<objectName>_triggered).

void MainWindow::on_actionNacist_triggered()
{
    loadFile("popispole");
}


void MainWindow::on_actionNahledUprava_2_triggered()
{
    viewZPMZFile(ControlZPMZ::Popispole);
}

void MainWindow::on_actionNacist_2_triggered()
{
    loadFile("nacrt");
}


void MainWindow::on_actionNahledUprava_3_triggered()
{
    viewZPMZFile(ControlZPMZ::Nacrt);
}


void MainWindow::on_actionNacist_3_triggered()
{
    loadFile("zap");
}


void MainWindow::on_actionNahledUprava_4_triggered()
{
    viewZPMZFile(ControlZPMZ::Zap);
}


void MainWindow::on_actionNacist_4_triggered()
{
    loadFile("prot");
}


void MainWindow::on_actionNahledUprava_5_triggered()
{
    viewZPMZFile(ControlZPMZ::Prot);
}


void MainWindow::on_actionNacist_5_triggered()
{
    loadFile("vymery");
}


void MainWindow::on_actionNahledUprava_6_triggered()
{
    viewZPMZFile(ControlZPMZ::Vymery);
}


void MainWindow::on_actionNacist_7_triggered()
{
    loadFile("sezvlast");
}

void MainWindow::on_actionNahledUprava_8_triggered()
{
    viewZPMZFile(ControlZPMZ::Sezvlast);
}



void MainWindow::on_actionNacist_8_triggered()
{
    loadFile("oprav");
}



void MainWindow::on_actionNahledUprava_9_triggered()
{
    viewZPMZFile(ControlZPMZ::Oprav);
}


void MainWindow::on_actionNacist_9_triggered()
{
    loadFile("dsps");
}


void MainWindow::on_actionNahledUprava_10_triggered()
{
    viewZPMZFile(ControlZPMZ::Dsps);
}


void MainWindow::on_actionNacist_10_triggered()
{
    loadFile("vytyc");
}


void MainWindow::on_actionNahledUprava_11_triggered()
{
    viewZPMZFile(ControlZPMZ::Vytyc);
}

void MainWindow::on_actionNahledUprava_12_triggered()
{
    checkAndDisplayPDF<DisplayGNSS>(GP.getGnss());
}



/**
 * @brief Action: clear every controller and delete generated work files.
 *
 * Resets controlGP, controlZadost and controlZPMZ to their empty state
 * and removes any files DeleteWorkFiles is responsible for. The PDF
 * viewers themselves are not closed.
 */
void MainWindow::on_actionSmazatVysledky_triggered()
{
    GP.removeAll();
    clearText();
    ProcessVFK::clear();

    DeleteWorkFiles::deleteFiles();
}


/**
 * @brief Action: load the VFK návrh změny file (uses .vfk filter).
 */
void MainWindow::on_actionNacist_6_triggered()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Otevřít soubor návrh změny", "", tr("Výměnný formát katastru nemovitostí (*.vfk)"));

    FileNonPDF file;
    file.setPresent(true);
    file.setPath(fileName);
    file.setWhatItIs("vfk");
    ProcessVFK::process(fileName);
    GP.setVfk(file);
}

/**
 * @brief Action: run Python segmentation on the loaded GP to produce annotations.
 *
 * Refuses to run if no GP file is currently loaded — shows a warning
 * dialog instead. On success, PythonManager populates the annotation
 * folder consumed by on_actionRozd_lit_triggered().
 */
void MainWindow::on_actionSegmentace_triggered()
{
    QString path_to_gp = GP.getGp().getPath();

    QFileInfo info(path_to_gp);
    if (!GP.getGp().getPresent())
    {
        if (path_to_gp.isEmpty() || !info.exists() || !info.isFile())
        {
            QMessageBox::warning(this, tr("Neplatný soubor"), tr("Nebyl vybrán soubor GP"));
            return;
        }
    }

    PythonManager::create_annotaions(path_to_gp);
    PythonManager::divide_pdf_by_annotations(GP.getGp().getPath());

    DisplayResults displayResults;
    appendText(displayResults.generateSegmentationReport(AddPath::getAnnotationPath()));

    // If the GP is already open in a DisplayGP viewer, refresh its overlay so
    // the freshly produced bounding boxes are drawn without reopening it.
    const QString key = info.absoluteFilePath();
    if (QPointer<PdfViewerWindow> existing = m_pdfViewers.value(key))
    {
        if (DisplayGP* gpViewer = qobject_cast<DisplayGP*>(existing.data()))
            gpViewer->reloadAnnotations();
    }
}


/**
 * @brief Action: delete generated work files without touching controller state.
 */
void MainWindow::on_actionVycistitVysledky_triggered()
{
    clearText();
    DeleteWorkFiles::deleteFiles();
}


void MainWindow::on_actionKlasifikaceSouboru_triggered()
{
    if (folder.isEmpty())
    {
        QMessageBox::warning(this, tr("Prázdná složka"), tr("Složka pro klasifikaci je prázdná"));
        return;
    }

    const std::vector<InputFile> inputs = PythonManager::classify(folder);


    for (const InputFile& file : inputs)
    {
        FilePDF file_pdf;
        FileNonPDF file_nonpdf;
        if (file.format==QStringLiteral("pdf"))
        {
            file_pdf.setPresent(true);
            file_pdf.setName(file.name);
            file_pdf.setPath(file.path);
            file_pdf.setWhatItIs(file.category);
            if (file.has_metadata)
            {
                file_pdf.setPDFVersion(file.pdf_version);
                file_pdf.setPDFA(file.is_pdfa);
                if (file.is_pdfa)
                {
                    file_pdf.setPDFAVersion(file.pdfa_version_number);
                    file_pdf.setVersion(file.pdfa_version_description);
                }
                file_pdf.setNumSignatures(file.number_of_signatures);
                file_pdf.setCanSignature(file.can_add_signature);
                file_pdf.setNumPages(file.number_of_pages);
                file_pdf.setFormats(file.pages_formats);
            }

        }
        else
        {
            file_nonpdf.setPresent(true);
            file_nonpdf.setName(file.name);
            file_nonpdf.setPath(file.path);
            file_nonpdf.setFormat(file.format);
            file_nonpdf.setWhatItIs(file.category);
        }

        const QString& cat = file.category;

        if      (cat == QStringLiteral("popispole") && !GP.getPopispole().getPresent()){GP.setPopispole(file_pdf);}
        else if (cat == QStringLiteral("nacrt")     && !GP.getNacrt().getPresent()){GP.setNacrt(file_pdf);}
        else if (cat == QStringLiteral("zap")       && !GP.getZap().getPresent()){GP.setZap(file_pdf);}
        else if (cat == QStringLiteral("prot")      && !GP.getProt().getPresent()){GP.setProt(file_pdf);}
        else if (cat == QStringLiteral("vymery")    && !GP.getVymery().getPresent()){GP.setVymery(file_pdf);}
        else if (cat == QStringLiteral("sezvlast")  && !GP.getSezvlast().getPresent()){GP.setSezvlast(file_pdf);}
        else if (cat == QStringLiteral("oprav")     && !GP.getOprav().getPresent()){GP.setOprav(file_pdf);}
        else if (cat == QStringLiteral("dsps")      && !GP.getDsps().getPresent()){GP.setDsps(file_pdf);}
        else if (cat == QStringLiteral("vytyc")     && !GP.getVytyc().getPresent()){GP.setVytyc(file_pdf);}
        else if (cat == QStringLiteral("gnss")      && !GP.getGnss().getPresent()){GP.setGnss(file_pdf);}
        else if (cat == QStringLiteral("gp")        && !GP.getGp().getPresent()){GP.setGp(file_pdf);}
        else if (cat == QStringLiteral("zadost")    && !GP.getZadost().getPresent()){GP.setZadost(file_pdf);}
        else if (cat == QStringLiteral("overeni")   && !GP.getOvereni().getPresent()){GP.setOvereni(file_pdf);}
        else if (cat == QStringLiteral("AZI1")      && !GP.getAZI1().getPresent()){GP.setAZI1(file_nonpdf);}
        else if (cat == QStringLiteral("AZI2")      && !GP.getAZI2().getPresent()){GP.setAZI2(file_nonpdf);}
        else if (cat == QStringLiteral("AZI3")      && !GP.getAZI3().getPresent()){GP.setAZI3(file_nonpdf);}
        else if (cat == QStringLiteral("vfk")       && !GP.getVfk().getPresent() && file.format == "vfk")
        {GP.setVfk(file_nonpdf); ProcessVFK::process(file_nonpdf.getPath());}
        else if (cat == QStringLiteral("vfk")       && !GP.getSs().getPresent() && file.format == "txt")
        {GP.setSs(file_nonpdf);}
        else {GP.pushOther(FileOther(file.name, QFileInfo(file.name).suffix(), file.path, file.category));}
    }

    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_PDF()));
    appendText(displayResults.generateNonPdfReport(controlPDF.control_nonPDF()));
}

void MainWindow::on_actionKontrolaSS_triggered()
{
    if (checkAnnotations())
    {
        return;
    }
    PythonManager::divide_pdf_by_annotations(GP.getGp().getPath());

    std::vector<Coordinates> ss_gp = PythonManager::extract_SS_from_GP();

    if (!GP.getVfk().getPresent())
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Nebyl vybrán soubor vfk"));
        return;
    }
    std::vector<Coordinates> ss_vfk = ProcessVFK::getSS();

    if (!GP.getProt().getPresent())
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Nebyl vybrán protokol"));
        return;
    }
    // Obtain the SS coordinates from the protocol.
    std::vector<Coordinates> ss_prot;

    // Prompt the user to enter/paste the protocol SS coordinates manually.
    // Fills ss_prot and returns true, or returns false if the user cancels.
    auto enterManually = [this, &ss_prot]() -> bool {
        bool ok = false;
        const QString pasted = QInputDialog::getMultiLineText(
            this,
            tr("Vložit souřadnice SS"),
            tr("Vložte text se souřadnicemi (jeden bod na řádek):"),
            QString(),
            &ok);

        if (!ok)
        {
            return false;
        }
        qDebug() << pasted;

        ss_prot = ParseText::parseSSTextProtPasted(pasted);
        return true;
    };

    if (GP.getProt().getPresent())
    {
        // Protocol present: let the user choose whether to extract the
        // coordinates automatically or enter them manually.
        QMessageBox sourceBox(this);
        sourceBox.setWindowTitle(tr("Zdroj souřadnic SS"));
        sourceBox.setText(tr("Jak chcete získat souřadnice SS z protokolu?"));
        sourceBox.setIcon(QMessageBox::Question);
        QPushButton* extractButton = sourceBox.addButton(tr("Extrahovat z protokolu"), QMessageBox::AcceptRole);
        QPushButton* manualButton  = sourceBox.addButton(tr("Zadat ručně"),            QMessageBox::ActionRole);
        sourceBox.addButton(QMessageBox::Cancel);
        sourceBox.exec();

        if (sourceBox.clickedButton() == extractButton)
        {
            ss_prot = PythonManager::extract_SS_from_prot(GP.getProt().getPath());
        }
        else if (sourceBox.clickedButton() == manualButton)
        {
            if (!enterManually())
                return;
        }
        else
        {
            return;
        }
    }
    else
    {
        // No protocol: the coordinates can only be entered manually.
        if (!enterManually())
            return;
    }

    SSControlReport ssReport = controlGP.controlSS(ss_gp, ss_vfk, ss_prot);
    DisplayResults displayResults;
    appendText(displayResults.generateSSReport(ssReport));
}



void MainWindow::on_actionSeznamSouradnic_triggered()
{
    viewGPPart(AddPath::getGPSSPath(), QStringLiteral("ss"), tr("Seznam souřadnic"));
}


void MainWindow::on_actionVykazVymer_2_triggered()
{
    viewGPPart(AddPath::getGPVykazPath(), QStringLiteral("vykaz"), tr("Výkaz výměr"));
}


void MainWindow::on_actionPopisovePole_2_triggered()
{
    viewGPPart(AddPath::getGPPopispolePath(), QStringLiteral("popispole"), tr("Popisové pole"));
}


void MainWindow::on_actionBPEJ_2_triggered()
{
    viewGPPart(AddPath::getGPBpejPath(), QStringLiteral("bpej"), tr("BPEJ"));
}


void MainWindow::on_actionMapovePole_triggered()
{
    viewGPPart(AddPath::getGPMapPath(), QStringLiteral("map"), tr("Mapové pole"));
}


void MainWindow::on_actionKladMapovychDilu_triggered()
{
    viewGPPart(AddPath::getGPKladPath(), QStringLiteral("klad"), tr("Klad mapových dílů"));
}


void MainWindow::on_actionOdebrat_triggered()
{
    if(GP.getPopispole().getPresent())
    {
        GP.removePopispole();
    }
}


void MainWindow::on_actionOdebrat_2_triggered()
{
    if(GP.getNacrt().getPresent())
    {
        GP.removeNacrt();
    }
}


void MainWindow::on_actionOdebrat_3_triggered()
{
    if(GP.getZap().getPresent())
    {
        GP.removeZap();
    }
}


void MainWindow::on_actionOdebrat_4_triggered()
{
    if(GP.getProt().getPresent())
    {
        GP.removeProt();
    }
}


void MainWindow::on_actionOdebrat_5_triggered()
{
    if(GP.getVymery().getPresent())
    {
        GP.removeVymery();
    }
}


void MainWindow::on_actionOdebrat_6_triggered()
{
    if(GP.getVfk().getPresent())
    {
        GP.removeVfk();
    }
}


void MainWindow::on_actionOdebrat_7_triggered()
{
    if(GP.getSezvlast().getPresent())
    {
        GP.removeSezvlast();
    }
}


void MainWindow::on_actionOdebrat_8_triggered()
{
    if(GP.getOprav().getPresent())
    {
        GP.removeOprav();
    }
}


void MainWindow::on_actionOdebrat_9_triggered()
{
    if(GP.getDsps().getPresent())
    {
        GP.removeDsps();
    }
}


void MainWindow::on_actionOdebrat_10_triggered()
{
    if(GP.getVytyc().getPresent())
    {
        GP.removeVytyc();
    }
}


void MainWindow::on_actionOdebrat_11_triggered()
{
    if(GP.getGnss().getPresent())
    {
        GP.removeGnss();
    }
}

bool MainWindow::checkAnnotations()
{
    QString path_to_annotations = AddPath::getAnnotationPath();
    QDir dir(path_to_annotations);


    if (dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty())
    {
        QMessageBox::warning(this, tr("Žádné výsledky"), tr("Nebyla provedena segmentace GP"));
        return true;
    }
    return false;
}


void MainWindow::on_actionKontrolaPopispole_triggered()
{
    /*if (checkAnnotations())
    {
        return;
    }*/

    if (!GP.getZadost().getPresent())
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Nebyla vybrána žádost"));
        return;
    }
    QString text = PythonManager::get_text_from_popisople_Zadost(GP.getZadost().getPath());
    qDebug()<<text;
    // Quick check: text has the form "Prijmeni Jmeno AZI_number authorisation",
    // where authorisation is up to 3 letters separated by spaces. The author has
    // correct authorisation only if one of those letters is "a".
    QStringList tokens = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    // The last token is an activity flag: "0" = author has no surveying activity,
    // "1" = author has surveying activity.
    QString activityFlag = tokens.isEmpty() ? QString() : tokens.takeLast();
    bool hasActivity = (activityFlag == "1");

    QStringList authorisation = tokens.mid(3, 3);

    bool hasAuthorisation = false;
    for (const QString &letter : authorisation)
    {
        if (letter.compare("a", Qt::CaseInsensitive) == 0)
        {
            hasAuthorisation = true;
            break;
        }
    }

    // Reformat as "Příjmení Jméno | autorizační číslo | autorizace".
    // tokens: [0]=Příjmení, [1]=Jméno, [2]=autorizační číslo, [3..]=autorizace.
    QString prijmeniJmeno = (tokens.value(0) + " " + tokens.value(1)).trimmed();
    QString aziNumber = tokens.value(2);
    QString result = prijmeniJmeno
                     + " | " + aziNumber
                     + " | " + authorisation.join(" ")
                     + "\n---- Autor "
                     + (hasAuthorisation ? tr("má") : tr("nemá"))
                     + tr(" správnou autorizaci.")
                     + "\n---- "
                     + (hasActivity ? tr("Autor má činnost v zeměměřictví")
                                    : tr("Autor nemá činnost v zeměměřictví"));

    appendText(result);
}

void MainWindow::getInfoSingleFile(FilePDF &pdf)
{
    InputFile file = PythonManager::get_info_about_PDF(pdf.getPath());

    pdf.setName(file.name);
    if (file.has_metadata)
    {
        pdf.setPDFVersion(file.pdf_version);
        pdf.setPDFA(file.is_pdfa);
        if (file.is_pdfa)
        {
            pdf.setPDFAVersion(file.pdfa_version_number);
            pdf.setVersion(file.pdfa_version_description);
        }
        pdf.setNumSignatures(file.number_of_signatures);
        pdf.setCanSignature(file.can_add_signature);
        pdf.setNumPages(file.number_of_pages);
        pdf.setFormats(file.pages_formats);
    }
}

void MainWindow::on_actionKontrola_3_triggered()
{
    // popisovepole
    FilePDF pdf = GP.getPopispole();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }
    getInfoSingleFile(pdf);
    GP.removePopispole();
    GP.setPopispole(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_4_triggered()
{
    // nacrt
    FilePDF pdf = GP.getNacrt();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }
    getInfoSingleFile(pdf);
    GP.removeNacrt();
    GP.setNacrt(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_5_triggered()
{
    // zap
    FilePDF pdf = GP.getZap();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }
    getInfoSingleFile(pdf);
    GP.removeZap();
    GP.setZap(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}



void MainWindow::on_actionKontrola_6_triggered()
{
    // protokol
    FilePDF pdf = GP.getProt();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeProt();
    GP.setProt(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_7_triggered()
{
    // vymery
    FilePDF pdf = GP.getVymery();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeVymery();
    GP.setVymery(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_9_triggered()
{
    // sezvlast
    FilePDF pdf = GP.getZap();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeSezvlast();
    GP.setSezvlast(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_10_triggered()
{
    // oprav
    FilePDF pdf = GP.getZap();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeOprav();
    GP.setOprav(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_11_triggered()
{
    // dsps
    FilePDF pdf = GP.getDsps();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeDsps();
    GP.setDsps(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_12_triggered()
{
    // vytyc
    FilePDF pdf = GP.getVytyc();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeVytyc();
    GP.setVytyc(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_13_triggered()
{
    // zadost
    FilePDF pdf = GP.getZadost();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeZadost();
    GP.setZadost(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionForm_ln_kontrola_triggered()
{
    FilePDF pdf = GP.getGp();

    bool present = pdf.getPresent();
    if (!present)
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
        return;
    }

    getInfoSingleFile(pdf);
    GP.removeGp();
    GP.setGp(pdf);
    ControlPDF controlPDF(GP);
    DisplayResults displayResults;
    appendText(displayResults.generatePdfReport(controlPDF.control_single_PDF(pdf)));
}


void MainWindow::on_actionKontrola_vykaz_triggered()
{
    if (checkAnnotations())
    {
        return;
    }
    VymeryInputs in;

    // Geometric plan (výkaz výměr): old, new and the comparison (por) column.
    PythonManager::get_text_from_vymery_GP(in.oldGp, in.newGp, in.porGp);

    // VFK: old and new state.
    in.oldVfk = ProcessVFK::getVymeryOld();
    in.newVfk = ProcessVFK::getVymeryNew();

    qDebug()<<in.oldVfk.size();

    // Výměry (výpočet výměr) document: old, new and the comparison column.
    if (!GP.getVymery().getPresent())
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Nebyl vybrán soubor výměry"));
        return;
    }
    PythonManager::get_text_from_vymery_Vymery(GP.getVymery().getPath(),
                                               in.oldVym, in.newVym, in.porVym);

    //Protocol: only the new state, informative (parcel number and new area).
    if (!GP.getProt().getPresent())
    {
        QMessageBox::warning(this, tr("Neplatný soubor"), tr("Nebyl vybrán protokol o výpočtech"));
        return;
    }
    PythonManager::get_text_from_vymery_Prot(GP.getProt().getPath(), in.newProt);

    VymeryControlReport report = controlGP.controlVymery(in);
    DisplayResults displayResults;
    appendText(displayResults.generateVymeryReport(report));
}

