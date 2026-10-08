/**
 * @file pdftextselectionoverlay.cpp
 * @brief Implementation of PdfTextSelectionOverlay.
 */

#include "pdftextselectionoverlay.h"

#include <QPdfView>
#include <QPdfDocument>
#include <QPdfSelection>
#include <QPdfPageNavigator>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGuiApplication>
#include <QClipboard>
#include <QMargins>
#include <QEvent>
#include <algorithm>

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------
PdfTextSelectionOverlay::PdfTextSelectionOverlay(QPdfView* pdfView, QPdfDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_pdfView(pdfView)
    , m_doc(doc)
{
    Q_ASSERT(pdfView);
    Q_ASSERT(doc);

    // The QPdfView is a QAbstractScrollArea; its drawing surface is the
    // viewport(). Reparent onto it so we sit exactly on top of the page.
    m_viewport = pdfView->viewport();
    setParent(m_viewport);
    setAttribute(Qt::WA_TransparentForMouseEvents); // pan mode by default
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(m_viewport->rect());
    show();
    raise();

    // Repaint the highlight whenever the view scrolls or is resized.
    m_viewport->installEventFilter(this);
    if (auto* sa = qobject_cast<QAbstractScrollArea*>(pdfView)) {
        if (sa->horizontalScrollBar())
            sa->horizontalScrollBar()->installEventFilter(this);
        if (sa->verticalScrollBar())
            sa->verticalScrollBar()->installEventFilter(this);
    }
}

// ---------------------------------------------------------------------------
//  Mode / selection state
// ---------------------------------------------------------------------------
void PdfTextSelectionOverlay::setSelectMode(bool on)
{
    m_selectMode = on;
    m_dragging   = false;

    // In select mode the overlay must receive mouse events; in pan mode it lets
    // them fall through to the viewport so the viewer's panning still works.
    setAttribute(Qt::WA_TransparentForMouseEvents, !on);
    setMouseTracking(on);

    if (on) {
        setCursor(Qt::IBeamCursor);
        raise();                 // sit above any other overlay (e.g. BBoxOverlay)
    } else {
        unsetCursor();
    }
    update();
}

void PdfTextSelectionOverlay::clearSelection()
{
    m_selBounds.clear();
    m_selText.clear();
    m_selPage  = -1;
    m_dragging = false;
    update();
}

void PdfTextSelectionOverlay::copySelectionToClipboard()
{
    if (m_selText.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(m_selText);
}

void PdfTextSelectionOverlay::notifyPageChanged(int page0based)
{
    // In single-page mode only one page is rendered, so a selection made on a
    // different page would map onto the wrong page — drop it. In multi-page
    // mode the selection keeps its own page, so just repaint.
    if (m_pdfView
        && m_pdfView->pageMode() == QPdfView::PageMode::SinglePage
        && page0based != m_selPage) {
        clearSelection();
    } else {
        update();
    }
}

void PdfTextSelectionOverlay::notifyZoomChanged()
{
    update();
}

// ---------------------------------------------------------------------------
//  Coordinate mapping (mirrors BBoxOverlay, with page margins/spacing)
// ---------------------------------------------------------------------------
PdfTextSelectionOverlay::PageRect PdfTextSelectionOverlay::pageRect(int page0based) const
{
    PageRect pr;
    if (!m_pdfView || !m_doc)
        return pr;
    if (page0based < 0 || page0based >= m_doc->pageCount())
        return pr;

    double zoom = m_pdfView->zoomFactor();
    if (zoom <= 0) zoom = 1.0;

    const double dpiScaleX = logicalDpiX() / 72.0;
    const double dpiScaleY = logicalDpiY() / 72.0;

    const QSizeF pageSize = m_doc->pagePointSize(page0based);
    if (pageSize.isEmpty())
        return pr;

    const double rendW = pageSize.width()  * dpiScaleX * zoom;
    const double rendH = pageSize.height() * dpiScaleY * zoom;

    auto* sa = qobject_cast<QAbstractScrollArea*>(m_pdfView);
    if (!sa)
        return pr;

    const int viewportW  = sa->viewport()->width();
    const int hScroll    = sa->horizontalScrollBar() ? sa->horizontalScrollBar()->value()   : 0;
    const int vScroll    = sa->verticalScrollBar()   ? sa->verticalScrollBar()->value()     : 0;
    const int hScrollMax = sa->horizontalScrollBar() ? sa->horizontalScrollBar()->maximum() : 0;

    // Horizontal: the page is centred within the scrollable content width; this
    // form naturally yields the left document margin once the page is wider than
    // the viewport, and a viewport-centred page otherwise.
    const double contentW = viewportW + hScrollMax;
    const double pageLeft = (contentW - rendW) / 2.0 - hScroll;

    // Vertical: pages stack from the top document margin, separated by the page
    // spacing. SinglePage mode renders one page at a time, so never accumulate.
    const QMargins margins = m_pdfView->documentMargins();
    double pageTop = margins.top() - vScroll;
    if (m_pdfView->pageMode() == QPdfView::PageMode::MultiPage) {
        const double gap = (m_pdfView->pageSpacing() > 0) ? m_pdfView->pageSpacing() : PAGE_GAP;
        for (int i = 0; i < page0based; ++i) {
            const QSizeF ps = m_doc->pagePointSize(i);
            pageTop += ps.height() * dpiScaleY * zoom + gap;
        }
    }

    pr = { pageLeft, pageTop, rendW, rendH, true };
    return pr;
}

int PdfTextSelectionOverlay::pageAt(const QPointF& viewportPos) const
{
    if (!m_pdfView || !m_doc)
        return -1;

    // SinglePage shows only the current page — selection always targets it.
    if (m_pdfView->pageMode() == QPdfView::PageMode::SinglePage)
        return m_pdfView->pageNavigator() ? m_pdfView->pageNavigator()->currentPage() : 0;

    for (int p = 0; p < m_doc->pageCount(); ++p) {
        const PageRect pr = pageRect(p);
        if (!pr.valid)
            continue;
        if (viewportPos.y() >= pr.top && viewportPos.y() <= pr.top + pr.height)
            return p;
    }
    return -1;
}

QPointF PdfTextSelectionOverlay::viewportToPagePoint(int page0based, const QPointF& viewportPos) const
{
    const PageRect pr = pageRect(page0based);
    if (!pr.valid || pr.width <= 0 || pr.height <= 0)
        return {};

    double nx = (viewportPos.x() - pr.left) / pr.width;
    double ny = (viewportPos.y() - pr.top)  / pr.height;
    nx = std::clamp(nx, 0.0, 1.0);
    ny = std::clamp(ny, 0.0, 1.0);

    const QSizeF ps = m_doc->pagePointSize(page0based);
    return QPointF(nx * ps.width(), ny * ps.height());
}

QPointF PdfTextSelectionOverlay::pagePointToViewport(int page0based, const QPointF& pagePt) const
{
    const PageRect pr = pageRect(page0based);
    if (!pr.valid)
        return {};

    const QSizeF ps = m_doc->pagePointSize(page0based);
    const double nx = (ps.width()  > 0) ? pagePt.x() / ps.width()  : 0.0;
    const double ny = (ps.height() > 0) ? pagePt.y() / ps.height() : 0.0;

    return QPointF(pr.left + nx * pr.width, pr.top + ny * pr.height);
}

void PdfTextSelectionOverlay::recomputeSelection(const QPointF& endViewportPos)
{
    if (!m_doc || m_selPage < 0) {
        m_selBounds.clear();
        m_selText.clear();
        update();
        return;
    }

    const QPointF endPt = viewportToPagePoint(m_selPage, endViewportPos);
    const QPdfSelection sel = m_doc->getSelection(m_selPage, m_startPagePt, endPt);

    m_selBounds = sel.bounds();
    m_selText   = sel.text();
    update();
}

// ---------------------------------------------------------------------------
//  Painting
// ---------------------------------------------------------------------------
void PdfTextSelectionOverlay::paintEvent(QPaintEvent*)
{
    if (m_selBounds.isEmpty() || m_selPage < 0)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(30, 120, 220, 70));   // translucent blue highlight

    for (const QPolygonF& poly : m_selBounds) {
        QPolygonF vp;
        vp.reserve(poly.size());
        for (const QPointF& pt : poly)
            vp << pagePointToViewport(m_selPage, pt);
        p.drawPolygon(vp);
    }
}

// ---------------------------------------------------------------------------
//  Mouse interaction
// ---------------------------------------------------------------------------
void PdfTextSelectionOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!m_selectMode || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int pg = pageAt(event->position());
    if (pg < 0) {
        clearSelection();
        return;
    }

    m_selPage      = pg;
    m_startPagePt  = viewportToPagePoint(pg, event->position());
    m_dragging     = true;
    m_selBounds.clear();
    m_selText.clear();
    update();
}

void PdfTextSelectionOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_selectMode) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (m_dragging)
        recomputeSelection(event->position());
}

void PdfTextSelectionOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_selectMode || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
}

void PdfTextSelectionOverlay::wheelEvent(QWheelEvent* event)
{
    // Never consume wheel events: let them propagate to the viewport so the
    // viewer's scroll and Ctrl+wheel zoom keep working even in select mode.
    event->ignore();
}

// ---------------------------------------------------------------------------
//  Keep the overlay covering the viewport; repaint on scroll/resize
// ---------------------------------------------------------------------------
bool PdfTextSelectionOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_viewport && event->type() == QEvent::Resize)
        setGeometry(m_viewport->rect());

    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::Move:
    case QEvent::Scroll:
        update();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}
