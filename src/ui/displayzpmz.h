/**
 * @file displayzpmz.h
 * @brief Declaration of DisplayZPMZ — multi-page PDF viewer for ZPMZ documents.
 */

#ifndef DISPLAYZPMZ_H
#define DISPLAYZPMZ_H

#include "pdfviewerwindow.h"

namespace Ui { class DisplayZPMZ;}

/**
 * @class DisplayZPMZ
 * @brief PDF viewer specialised for ZPMZ documents.
 *
 * Thin subclass of PdfViewerWindow that loads its own .ui file and runs
 * the underlying QPdfView in multi-page mode. It overrides
 * onCurrentPageChanged() and applyZoom() purely as forwarding hooks —
 * unlike DisplayGP it does not maintain any extra overlay, but the
 * overrides are kept in place so future ZPMZ-specific behaviour can plug
 * in without further wiring.
 */
class DisplayZPMZ : public PdfViewerWindow
{
    Q_OBJECT

public:
    /**
     * @brief Construct a DisplayZPMZ that loads the PDF at @p path in multi-page mode.
     * @param path   Absolute path to the ZPMZ PDF.
     * @param parent Parent widget, or nullptr.
     */
    explicit DisplayZPMZ(const QString &path, QWidget *parent = nullptr);

    /** @brief Destructor — frees the .ui object. */
    ~DisplayZPMZ();

    /**
     * @brief Select the class matching @p categoryKey in the dropdown.
     *
     * @p categoryKey is the internal category string stored in
     * FilePDF/FileNonPDF::what_it_is (e.g. "nacrt"). If it is empty or not one
     * of the offered classes (e.g. a GNSS file), the dropdown is left with no
     * selection. Setting the selection here does not emit categoryChanged() —
     * that signal is reserved for user-driven changes.
     */
    void setCurrentCategory(const QString& categoryKey);

Q_SIGNALS:
    /**
     * @brief Emitted when the user picks a different class in the dropdown.
     * @param categoryKey Internal category key (the FilePDF::what_it_is value).
     */
    void categoryChanged(const QString& categoryKey);

private Q_SLOTS:
    /**
     * @brief Forward page-change events to the base class.
     * @param page0based Zero-based page index.
     */
    void onCurrentPageChanged(int page0based) override;

private:
    /**
     * @brief Forward zoom requests to the base class.
     * @param factor Multiplicative zoom factor.
     */
    void applyZoom(double factor) override;

    Ui::DisplayZPMZ* m_displayUi = nullptr; ///< uic-generated UI for this subclass.
};

#endif // DISPLAYZPMZ_H
