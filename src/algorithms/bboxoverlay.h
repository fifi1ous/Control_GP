#ifndef BBOXOVERLAY_H
#define BBOXOVERLAY_H

#include <QWidget>
#include <QColor>
#include <QMap>
#include <QVector>
#include <QString>
#include <QPointF>

#include "structures.h"
#include "addpath.h"

class QPdfView;
class QPdfDocument;

// ---------------------------------------------------------------------------
//  One detection box (coordinates in PDF points, origin = top-left of page)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  BBoxOverlay
//
//  A transparent overlay widget that is reparented onto the QPdfView's
//  viewport.  It draws bounding boxes for the currently visible page(s).
//
//  Usage
//  -----
//  1.  Construct after the QPdfView and QPdfDocument are ready:
//
//        m_overlay = new BBoxOverlay(ui->pdfView, m_doc, this);
//
//  2.  Set the directory that contains the per-page txt files:
//
//        m_overlay->setAnnotationDir("/path/to/txts");
//
//      Expected filename pattern:  page_<N>.txt   (N = 1-based page number)
//      Expected line format:       classId xMin yMin xMax yMax
//      (all coordinates in PDF points, origin top-left of page)
//
//  3.  Call notifyPageChanged(page0based) and notifyZoomChanged() from
//      PdfViewerWindow whenever the page or zoom level changes.
//
//  Optional
//  --------
//  • setClassNames(QMap<int,QString>) – supply human-readable class names.
//  • setFillAlpha(int)                – 0-255, default 40.
// ---------------------------------------------------------------------------
class BBoxOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit BBoxOverlay(QPdfView* pdfView,
                         QPdfDocument* doc,
                         QWidget* parent = nullptr);

    // Transparency of the filled rectangle (0 = invisible fill, 255 = solid).
    void setFillAlpha(int alpha);   // default: 40

    // Toggle interactive edit mode (move / resize boxes, save to .txt).
    void setEditMode(bool on);
    bool editMode() const { return m_editMode; }

    // Call these from PdfViewerWindow's slots/overrides.
    void notifyPageChanged(int page0based);
    void notifyZoomChanged();

    // Drop the cached boxes and repaint — call after the annotation .txt
    // files on disk have changed (e.g. after re-running segmentation) so the
    // overlay reflects the new detections without reopening the window.
    void reloadAnnotations();

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // ---- handle hit-testing ------------------------------------------------
    enum Handle {
        HandleNone = 0,
        HandleMove,        // inside the box body
        HandleTopLeft,
        HandleTop,
        HandleTopRight,
        HandleRight,
        HandleBottomRight,
        HandleBottom,
        HandleBottomLeft,
        HandleLeft
    };

    struct HitResult {
        int    boxIndex = -1;   // index into the page's box vector
        Handle handle   = HandleNone;
    };

    static constexpr int HANDLE_SIZE = 6;   // half-size of corner/edge grips

    HitResult hitTest(const QPointF& pos) const;
    Qt::CursorShape cursorForHandle(Handle h) const;

    // Convert overlay pixel position back to PDF-point coordinates.
    QPointF overlayToPdf(int page0based, const QPointF& pos) const;

    // Save all boxes for the given page back to the annotation .txt file.
    void saveAnnotations(int page0based);

    // ---- helpers -----------------------------------------------------------
    QColor  colorForClass(int classId) const;
    QString labelForClass(int classId) const;

    // Load (and cache) annotations for a given 0-based page.
    const QVector<BBox>& boxesForPage(int page0based);

    // Mutable access to boxes (for editing).
    QVector<BBox>& mutableBoxesForPage(int page0based);

    // Map a PDF-point coordinate to a pixel position inside the overlay.
    // Returns QRectF in overlay-local coordinates, or an invalid rect if the
    // mapping cannot be performed (document not ready, etc.).
    QRectF  pdfRectToOverlay(int page0based, const BBox& box) const;

    // ---- data --------------------------------------------------------------
    QPdfView*     m_pdfView   = nullptr;
    QPdfDocument* m_doc       = nullptr;
    QWidget*      m_viewport  = nullptr;   // the actual scrollable child of QPdfView

    QString m_annotationDir = AddPath::getAnnotationPath();
    int     m_currentPage = 0;   // 0-based
    int     m_fillAlpha   = 40;

    QMap<int, QVector<BBox>>   m_cache;      // page0 → boxes

    // ---- edit-mode state ---------------------------------------------------
    bool    m_editMode     = false;
    int     m_selectedBox  = -1;   // index of the selected box (-1 = none)
    Handle  m_activeHandle = HandleNone;
    bool    m_dragging     = false;
    QPointF m_dragStartPdf;         // PDF-point position at mouse-press
    BBox    m_dragOrigBox;          // original box coords at drag start

    // Palette of distinct colours (hue wheel, 15° steps, high saturation)
    static QColor paletteColor(int index);
};

#endif // BBOXOVERLAY_H
