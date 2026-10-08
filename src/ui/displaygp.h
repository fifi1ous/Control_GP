/**
 * @file displaygp.h
 * @brief Declaration of DisplayGP — PDF viewer specialised for geometric-plan documents with bounding-box overlay.
 */

#ifndef DISPLAYGP_H
#define DISPLAYGP_H

#include "pdfviewerwindow.h"
#include "bboxoverlay.h"

namespace Ui { class DisplayGP; }

/**
 * @class DisplayGP
 * @brief Single-page PDF viewer that renders an interactive BBoxOverlay on top of the geometric plan.
 *
 * Extends PdfViewerWindow with a BBoxOverlay child widget. The viewer
 * starts in single-page mode and exposes two checkboxes from the .ui file:
 * - **"Viditelné"** — toggles overlay visibility.
 * - **"Úpravy"**    — toggles interactive edit mode (move / resize boxes).
 *
 * The overlay is informed of page changes and zoom changes through the
 * overridden onCurrentPageChanged() and applyZoom() hooks so it can repaint
 * its annotations in sync with the underlying QPdfView.
 */
class DisplayGP : public PdfViewerWindow
{
    Q_OBJECT

public:
    /**
     * @brief Construct a DisplayGP that loads the PDF at @p path.
     * @param path   Absolute path to the geometric-plan PDF.
     * @param parent Parent widget, or nullptr.
     */
    explicit DisplayGP(const QString &path, QWidget *parent = nullptr);

    /** @brief Destructor — frees the .ui object. */
    ~DisplayGP();

    /**
     * @brief Reload bounding boxes from disk and repaint the overlay.
     *
     * Call after the annotation .txt files have been regenerated (e.g. by
     * re-running segmentation) so an already-open viewer shows the new
     * detections without being closed and reopened.
     */
    void reloadAnnotations();

private Q_SLOTS:
    /**
     * @brief Forward page changes to the BBoxOverlay so it repaints for the new page.
     * @param page0based Zero-based page index.
     */
    void onCurrentPageChanged(int page0based) override;

private:
    /**
     * @brief Apply zoom and notify the overlay so it can rescale its boxes.
     * @param factor Multiplicative zoom factor (forwarded to base class).
     */
    void applyZoom(double factor) override;

    Ui::DisplayGP* m_displayUi = nullptr;   ///< uic-generated UI for this subclass.
    BBoxOverlay*   m_overlay = nullptr;     ///< Overlay widget that draws bounding boxes on top of #m_pdfView.
};

#endif
