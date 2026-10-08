/**
 * @file mainwindow.h
 * @brief Declaration of MainWindow — top-level window for the Control_GP application.
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>
#include <QHash>
#include <QFileDialog>
#include <QMessageBox>

#include "geometricplan.h"
#include "controlgeometricplan.h"
#include "controlzpmz.h"
#include "controlzadost.h"
#include "pythonmanager.h"

#include "pdfviewerwindow.h"
#include "displayzpmz.h"
#include "displaygnss.h"
#include "displaygp.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/**
 * @class MainWindow
 * @brief Application main window — wires menu actions to the controllers and PDF viewers.
 *
 * Owns a single #GP (GeometricPlan) object that holds every file part of
 * the submission and is the single source of truth. Three controllers are
 * bound to slots of #GP and operate on its parts rather than holding their
 * own copies:
 * - #controlGP     — bound to GP's geometric-plan slot.
 * - #controlZadost — bound to GP's Žádost (request) slot.
 * - #controlZPMZ   — bound to GP's ZPMZ slots (Popispole, Náčrt, Zápisník, ...).
 *
 * Each controller is driven by a pair of menu-action slots: one to load a
 * file via QFileDialog, one to display it through checkAndDisplayPDF<T>().
 * The class also exposes Python-backed actions for segmentation, splitting
 * and SS-text extraction via PythonManager.
 *
 * Several PDF viewers can be open at once — one window per distinct PDF.
 * #m_pdfViewers maps each shown PDF's path to its viewer, so requesting a
 * file that is already open just raises that window instead of duplicating
 * it, and a window is only closed when its parameters (the path) change.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @brief Construct the main window and load the .ui.
     * @param parent Parent widget, or nullptr.
     */
    MainWindow(QWidget *parent = nullptr);

    /** @brief Destructor — frees the .ui object. */
    ~MainWindow();

    /**
     * @brief Display @p text in the results QTextEdit.
     * @param text Plain text to show; replaces any previous content.
     */
    void displayText(const QString& text);

    /**
     * @brief Append @p text to the results QTextEdit, preserving previous content.
     */
    void appendText(const QString& text);

    /**
     * @brief Clear all displayed text from the results QTextEdit.
     */
    void clearText();

private Q_SLOTS:
    /** @name Geometric-plan and import actions @{ */
    void on_actionNactiGP_triggered();          ///< Load a geometric-plan PDF into #controlGP.
    void on_actionOtevri_triggered();           ///< Multi-file import dialog (placeholder).
    void on_actionImportSlozky_triggered();    ///< Folder import dialog (placeholder).
    void on_actionNahledUprava_triggered();   ///< View the loaded GP in a DisplayGP viewer.
    /** @} */

    /** @name Žádost (request) actions @{ */
    void on_actionNacistZadost_triggered();       ///< Load the Žádost PDF.
    void on_actionNahled_triggered();           ///< View the loaded Žádost PDF.
    /** @} */

    /** @name ZPMZ document load/view pairs
     *  Each ZPMZ document type has a load (Na_st_*) and view (N_hled_*)
     *  action that wraps loadZPMZFile() / viewZPMZFile().
     *  @{ */
    void on_actionNacist_triggered();                ///< Load Popispole PDF.
    void on_actionNahledUprava_2_triggered();     ///< View Popispole PDF.

    void on_actionNacist_2_triggered();              ///< Load Náčrt PDF.
    void on_actionNahledUprava_3_triggered();     ///< View Náčrt PDF.

    void on_actionNacist_3_triggered();              ///< Load Zápisník měření PDF.
    void on_actionNahledUprava_4_triggered();     ///< View Zápisník měření PDF.

    void on_actionNacist_4_triggered();              ///< Load Protokol PDF.
    void on_actionNahledUprava_5_triggered();     ///< View Protokol PDF.

    void on_actionNacist_5_triggered();              ///< Load Výměry PDF.
    void on_actionNahledUprava_6_triggered();         ///< View Výměry PDF.

    void on_actionNacist_7_triggered();              ///< Load Seznámení vlastníků PDF.

    void on_actionNacist_8_triggered();              ///< Load Opravy PDF.
    void on_actionNahledUprava_8_triggered();     ///< View Opravy PDF.

    void on_actionNacist_9_triggered();              ///< Load DSPS PDF.
    void on_actionNahledUprava_9_triggered();     ///< View DSPS PDF.

    void on_actionNacist_10_triggered();             ///< Load Dokumentace o vytyčení PDF.
    void on_actionNahledUprava_10_triggered();    ///< View Dokumentace o vytyčení PDF.

    void on_actionNacist_6_triggered();              ///< Load VFK návrh změny file.
    /** @} */

    /** @name Workspace and processing actions @{ */
    void on_actionSmazatVysledky_triggered();      ///< Clear all controllers and delete generated work files.
    void on_actionSegmentace_triggered();           ///< Run Python annotation/segmentation on the loaded GP.
    void on_actionVycistitVysledky_triggered();    ///< Delete generated work files only.
    /** @} */

    void on_actionNahledUprava_11_triggered();

    void on_actionKlasifikaceSouboru_triggered();

    void on_actionKontrolaSS_triggered();

    /** @name Segmented GP-part viewers
     *  Each action opens the corresponding part produced by segmentation
     *  (#on_actionSegmentace_triggered) in a DisplayZPMZ viewer, or warns if
     *  the GP was not segmented / does not contain that part. All delegate to
     *  viewGPPart().
     *  @{ */
    void on_actionSeznamSouradnic_triggered();   ///< View the segmented "Seznam souřadnic" (SS) part.
    void on_actionVykazVymer_2_triggered();      ///< View the segmented "Výkaz výměr" part.
    void on_actionPopisovePole_2_triggered();    ///< View the segmented "Popisové pole" part.
    void on_actionBPEJ_2_triggered();            ///< View the segmented "BPEJ" part.
    void on_actionMapovePole_triggered();        ///< View the segmented "Mapové pole" part.
    void on_actionKladMapovychDilu_triggered();  ///< View the segmented "Klad mapových dílů" part.
    /** @} */

    void on_actionOdebrat_triggered();

    void on_actionOdebrat_2_triggered();

    void on_actionOdebrat_3_triggered();

    void on_actionOdebrat_4_triggered();

    void on_actionOdebrat_5_triggered();

    void on_actionOdebrat_6_triggered();

    void on_actionOdebrat_7_triggered();

    void on_actionOdebrat_8_triggered();

    void on_actionOdebrat_9_triggered();

    void on_actionOdebrat_10_triggered();

    void on_actionOdebrat_11_triggered();

    void on_actionNahledUprava_12_triggered();

    void on_actionKontrolaPopispole_triggered();

    void on_actionKontrola_3_triggered();

    void on_actionKontrola_4_triggered();

    void on_actionKontrola_5_triggered();

    void on_actionKontrola_6_triggered();

    void on_actionKontrola_7_triggered();

    void on_actionKontrola_9_triggered();

    void on_actionKontrola_10_triggered();

    void on_actionKontrola_11_triggered();

    void on_actionKontrola_12_triggered();

    void on_actionKontrola_13_triggered();

    void on_actionForm_ln_kontrola_triggered();

    void on_actionKontrola_vykaz_triggered();

private:
    Ui::MainWindow *ui;                         ///< uic-generated UI for the main window.
    GeometricPlan GP;                           ///< Single source of truth — owns every file part of the submission.
    ControlGeometricPlan controlGP;             ///< Controller bound to GP's geometric-plan slot.
    ControlZadost controlZadost;                ///< Controller bound to GP's Žádost slot.
    ControlZPMZ controlZPMZ;                    ///< Controller bound to GP's ZPMZ slots.
    QHash<QString, QPointer<PdfViewerWindow>> m_pdfViewers; ///< Open viewers, keyed by the PDF path they show.
    QString folder;
    QString ss_prot;

    /**
     * @brief Open a file dialog and store the chosen path in #controlZPMZ for the given @p type.
     * @param type        ZPMZ file type that the chosen path should populate.
     * @param dialogTitle Title for the file dialog.
     * @param filter      Qt file filter string.
     */
    void loadFile(QString what_it_is);

    /**
     * @brief Display the file currently associated with @p type using a DisplayZPMZ viewer.
     *
     * Also reflects the file's current class in the viewer's dropdown and wires
     * the dropdown so that picking a different class reclassifies the file (see
     * reclassifyZPMZ()).
     *
     * @param type ZPMZ file type to view.
     */
    void viewZPMZFile(ControlZPMZ::FileType type);

    /**
     * @brief Move the file shown for @p fromType into the class @p newCategory.
     *
     * Triggered when the user picks a different class in a DisplayZPMZ dropdown.
     * The file is copied out of its current slot, retagged with @p newCategory,
     * removed from the old slot and written into the slot for @p newCategory
     * (overwriting any existing occupant). After the move the original menu no
     * longer opens it — the file now lives under the newly selected class.
     *
     * @param fromType    Slot the viewer was opened from.
     * @param newCategory Internal category key chosen in the dropdown.
     */
    void reclassifyZPMZ(ControlZPMZ::FileType fromType, const QString& newCategory);

    /**
     * @brief Write @p file into the GeometricPlan slot named by @p categoryKey.
     *
     * Resolves the internal category key (e.g. "nacrt", "gp", "zadost") to the
     * matching GeometricPlan setter, overwriting whatever was there. Unknown
     * keys are ignored.
     */
    void placeIntoCategory(const FilePDF& file, const QString& categoryKey);

    /**
     * @brief Display a segmented GP part in a DisplayZPMZ viewer, or warn if it is unavailable.
     *
     * Segmentation (#on_actionSegmentace_triggered) writes each detected part
     * of the geometric plan as "<abbr>.pdf" into its own folder. A part PDF is
     * only written when at least one bounding box of that class is found, so a
     * missing file means either the GP was never segmented (the annotation
     * folder is empty) or it simply does not contain this part. Both cases
     * produce a warning dialog instead of opening a viewer.
     *
     * @param partDir   Folder holding the segmented part (e.g. AddPath::getGPSSPath()).
     * @param abbr      File-name stem of the part PDF (e.g. "ss").
     * @param partLabel Human-readable part name used in the "not present" warning.
     */
    void viewGPPart(const QString& partDir, const QString& abbr, const QString& partLabel);

    void getInfoSingleFile(FilePDF &pdf);


    bool checkAnnotations();
    /**
     * @brief Display @p file in a viewer of type @p T, one window per PDF.
     *
     * Validates that @p file is present and points to an existing file. Viewers
     * are tracked in #m_pdfViewers keyed by the PDF path, so several different
     * PDFs can be open side by side:
     * - If a window of the same type is already showing this exact path, it is
     *   reused — raised to the front rather than duplicated.
     * - If a window of a *different* type is showing this path (its parameters
     *   changed), that stale window is closed before the new one is created.
     *
     * The new window is parented as a top-level window, marked delete-on-close
     * and removed from #m_pdfViewers automatically when it is destroyed.
     *
     * @tparam T   PdfViewerWindow subclass to instantiate.
     * @tparam F   File-metadata type exposing getPath()/getPresent()
     *             (FilePDF or FileNonPDF).
     * @param file File metadata describing the document to display.
     * @return The shown viewer (new or reused), or nullptr if @p file was not a valid file.
     */
    template<typename T, typename F>
    T* checkAndDisplayPDF(const F &file)
    {
        QFileInfo info(file.getPath());
        if (file.getPath().isEmpty() || !info.exists() || !info.isFile() || !file.getPresent())
        {
            QMessageBox::warning(this, tr("Neplatný soubor"), tr("Vybraný soubor není platné PDF."));
            return nullptr;
        }

        // A window is identified by the PDF it shows (its absolute path), so
        // opening a different PDF never disturbs the windows already open.
        const QString key = info.absoluteFilePath();

        if (QPointer<PdfViewerWindow> existing = m_pdfViewers.value(key))
        {
            if (T* same = qobject_cast<T*>(existing.data()))
            {
                // Same PDF already open in the same kind of viewer — reuse it.
                same->raise();
                same->activateWindow();
                return same;
            }
            // Same path, different viewer type: parameters changed, so replace.
            existing->close();
            m_pdfViewers.remove(key);
        }

        T* viewer = new T(file.getPath(), this);
        m_pdfViewers.insert(key, viewer);
        // Drop the entry when the window goes away so the key can be reopened.
        connect(viewer, &QObject::destroyed, this,
                [this, key]() { m_pdfViewers.remove(key); });

        viewer->setWindowFlag(Qt::Window, true);
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
        return viewer;
    }

};
#endif // MAINWINDOW_H
