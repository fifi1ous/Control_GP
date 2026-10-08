/**
 * @file pdftextselectionoverlay.h
 * @brief Declaration of PdfTextSelectionOverlay — a transparent overlay that
 *        adds drag-to-select text and copy-to-clipboard to a QPdfView.
 */

#ifndef PDFTEXTSELECTIONOVERLAY_H
#define PDFTEXTSELECTIONOVERLAY_H

#include <QWidget>
#include <QString>
#include <QList>
#include <QPolygonF>
#include <QPointF>

class QPdfView;
class QPdfDocument;

/**
 * @class PdfTextSelectionOverlay
 * @brief Transparent overlay reparented onto a QPdfView's viewport that
 *        implements interactive text selection.
 *
 * Qt's widget-based QPdfView has no built-in text selection, only search. This
 * overlay fills that gap: in *select* mode a left-button drag picks the text
 * between two points via QPdfDocument::getSelection(), the selection is
 * highlighted, and copySelectionToClipboard() (wired to Ctrl+C by the viewer)
 * copies the text. In *pan* mode the overlay is transparent to the mouse so the
 * viewer's normal click-and-drag panning keeps working.
 *
 * The pixel↔page-point coordinate mapping mirrors BBoxOverlay so the highlight
 * stays aligned with the rendered page across zoom and scroll. Selection is
 * confined to the single page where the drag began.
 */
class PdfTextSelectionOverlay : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Construct the overlay and reparent it onto @p pdfView's viewport.
     * @param pdfView The view to overlay (must be valid).
     * @param doc     The document shown by @p pdfView (provides the text).
     * @param parent  Initial parent (the overlay reparents itself to the viewport).
     */
    explicit PdfTextSelectionOverlay(QPdfView* pdfView, QPdfDocument* doc, QWidget* parent = nullptr);

    /** @brief Enable (true) text selection or (false) pass-through pan mode. */
    void setSelectMode(bool on);
    bool selectMode() const { return m_selectMode; }

    /** @brief Whether a non-empty text selection currently exists. */
    bool hasSelection() const { return !m_selText.isEmpty(); }
    QString selectedText() const { return m_selText; }

    /** @brief Re-evaluate after the viewer changes page; clears a stale selection in single-page mode. */
    void notifyPageChanged(int page0based);
    /** @brief Repaint after the viewer zooms. */
    void notifyZoomChanged();

public Q_SLOTS:
    /** @brief Copy the current selection text to the system clipboard (no-op if empty). */
    void copySelectionToClipboard();
    /** @brief Drop the current selection and repaint. */
    void clearSelection();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// Geometry of one page inside the viewport, in device pixels.
    struct PageRect {
        double left = 0, top = 0, width = 0, height = 0;
        bool   valid = false;
    };

    /** @brief Compute where page @p page0based sits inside the viewport. */
    PageRect pageRect(int page0based) const;

    /** @brief The page under a viewport position, or -1 if none. */
    int pageAt(const QPointF& viewportPos) const;

    /** @brief Map a viewport pixel position to page-point coordinates. */
    QPointF viewportToPagePoint(int page0based, const QPointF& viewportPos) const;
    /** @brief Map a page-point coordinate to a viewport pixel position. */
    QPointF pagePointToViewport(int page0based, const QPointF& pagePt) const;

    /** @brief Recompute the selection from the drag start to @p endViewportPos. */
    void recomputeSelection(const QPointF& endViewportPos);

    QPdfView*     m_pdfView  = nullptr;
    QPdfDocument* m_doc      = nullptr;
    QWidget*      m_viewport = nullptr;   ///< QPdfView's scrollable drawing surface.

    bool m_selectMode = false;
    bool m_dragging   = false;

    int     m_selPage = -1;        ///< Page the current selection lives on.
    QPointF m_startPagePt;         ///< Drag-start position in page points.

    QList<QPolygonF> m_selBounds;  ///< Selection outline polygons, in page points.
    QString          m_selText;    ///< Selected text.

    /// Gap between consecutive pages in MultiPage mode (matches BBoxOverlay).
    static constexpr double PAGE_GAP = 4.0;
};

#endif // PDFTEXTSELECTIONOVERLAY_H
