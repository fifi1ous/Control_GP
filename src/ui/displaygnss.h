/**
 * @file displaygnss.h
 * @brief Declaration of DisplayGNSS — multi-page PDF viewer for GNSS documents.
 */

#ifndef DISPLAYGNSS_H
#define DISPLAYGNSS_H

#include "pdfviewerwindow.h"

namespace Ui { class DisplayGNSS;}

/**
 * @class DisplayGNSS
 * @brief PDF viewer specialised for GNSS documents.
 *
 * Thin subclass of PdfViewerWindow that loads its own .ui file and runs
 * the underlying QPdfView in multi-page mode. It overrides
 * onCurrentPageChanged() and applyZoom() purely as forwarding hooks —
 * unlike DisplayGP it does not maintain any extra overlay, but the
 * overrides are kept in place so future GNSS-specific behaviour can plug
 * in without further wiring.
 */
class DisplayGNSS : public PdfViewerWindow
{
    Q_OBJECT

public:
    /**
     * @brief Construct a DisplayGNSS that loads the PDF at @p path in multi-page mode.
     * @param path   Absolute path to the GNSS PDF.
     * @param parent Parent widget, or nullptr.
     */
    explicit DisplayGNSS(const QString &path, QWidget *parent = nullptr);

    /** @brief Destructor — frees the .ui object. */
    ~DisplayGNSS();

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

    Ui::DisplayGNSS* m_displayUi = nullptr; ///< uic-generated UI for this subclass.
};

#endif // DISPLAYGNSS_H
