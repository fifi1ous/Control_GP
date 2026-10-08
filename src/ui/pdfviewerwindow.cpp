/**
 * @file pdfviewerwindow.cpp
 * @brief Implementation of PdfViewerWindow.
 */

#include "pdfviewerwindow.h"
#include "ui_pdfviewerwindow.h"
#include "pdftextselectionoverlay.h"

#include <QPdfPageNavigator>
#include <QCloseEvent>
#include <QGestureEvent>
#include <QPinchGesture>
#include <QScrollBar>
#include <QCursor>
#include <QPushButton>
#include <QBoxLayout>
#include <QKeySequence>

namespace {

/**
 * @brief Find the QBoxLayout that directly manages @p w.
 *
 * Searches the layout tree of @p w's parent widget (the toolbar lives in a
 * nested QHBoxLayout inside the window's top QVBoxLayout), so a new toolbar
 * button can be inserted next to the existing ones regardless of which .ui a
 * subclass supplied. Returns nullptr if no managing box layout is found.
 */
QBoxLayout* findManagingBox(QWidget* w)
{
    if (!w)
        return nullptr;
    QWidget* parent = w->parentWidget();
    if (!parent || !parent->layout())
        return nullptr;

    QList<QLayout*> stack{ parent->layout() };
    while (!stack.isEmpty()) {
        QLayout* l = stack.takeFirst();
        if (auto* box = qobject_cast<QBoxLayout*>(l))
            if (box->indexOf(w) >= 0)
                return box;
        for (int i = 0; i < l->count(); ++i)
            if (QLayout* sub = l->itemAt(i)->layout())
                stack << sub;
    }
    return nullptr;
}

} // namespace

/**
 * @brief Protected constructor for subclasses that supply their own .ui file.
 *
 * Allocates the QPdfDocument as a child of this widget, but leaves the
 * widget pointers and event filters uninitialized — the subclass is
 * expected to populate them and then call initViewer().
 */
PdfViewerWindow::PdfViewerWindow(QWidget* parent)
    : QWidget(parent)
    , m_doc(new QPdfDocument(this))
{}

/**
 * @brief Public constructor — instantiates the base .ui and loads @p pdfPath.
 */
PdfViewerWindow::PdfViewerWindow(const QString& pdfPath, QWidget* parent, QPdfView::PageMode pageMode)
    : QWidget(parent)
    , m_doc(new QPdfDocument(this))
{
    m_baseUi = new Ui::PdfViewerWindow;
    m_baseUi->setupUi(this);

    m_pdfView        = m_baseUi->pdfView;
    m_zoomInBtn      = m_baseUi->zoomInBtn;
    m_zoomOutBtn     = m_baseUi->zoomOutBtn;
    m_prevBtn        = m_baseUi->prevBtn;
    m_nextBtn        = m_baseUi->nextBtn;
    m_pageSpin       = m_baseUi->pageSpin;
    m_pageCountLabel = m_baseUi->pageCountLabel;

    initViewer(pdfPath, pageMode);
}

/**
 * @brief Block document/navigator signals before deleting the UI.
 *
 * Prevents queued slot invocations on already-destroyed members during
 * widget teardown.
 */
PdfViewerWindow::~PdfViewerWindow()
{
    if (m_doc) m_doc->blockSignals(true);
    if (m_nav) m_nav->blockSignals(true);

    delete m_baseUi;
}

/**
 * @brief Wire up the PDF view: filters, signals, gestures and document load.
 *
 * Installs event filters on the QPdfView and its viewport so we can
 * intercept Ctrl+wheel, arrow keys and mouse-drag panning before the
 * default QPdfView handlers run. Connects all toolbar buttons and the
 * page-navigator signals, then triggers the asynchronous document load.
 */
void PdfViewerWindow::initViewer(const QString& pdfPath, QPdfView::PageMode pageMode)
{
    m_pdfView->installEventFilter(this);
    m_pdfView->viewport()->installEventFilter(this);
    m_zoomTimer.start();
    m_scrollTimer.start();

    m_pdfView->setDocument(m_doc);
    m_pdfView->setPageMode(pageMode);
    m_nav = m_pdfView->pageNavigator();

    m_pdfView->grabGesture(Qt::PinchGesture);

    connect(m_zoomInBtn,  &QPushButton::clicked, this, &PdfViewerWindow::zoomIn);
    connect(m_zoomOutBtn, &QPushButton::clicked, this, &PdfViewerWindow::zoomOut);
    connect(m_prevBtn,    &QPushButton::clicked, this, &PdfViewerWindow::prevPage);
    connect(m_nextBtn,    &QPushButton::clicked, this, &PdfViewerWindow::nextPage);

    connect(m_pageSpin, &QSpinBox::valueChanged,
            this, &PdfViewerWindow::pageSpinChanged);
    connect(m_doc, &QPdfDocument::pageCountChanged,
            this,  &PdfViewerWindow::onPageCountChanged);
    connect(m_nav, &QPdfPageNavigator::currentPageChanged,
            this,  &PdfViewerWindow::onCurrentPageChanged);

    // Text-selection overlay + a Pan/Select toolbar toggle. The overlay sits on
    // the viewport; in pan mode it is transparent to the mouse, so the default
    // panning is untouched. Created programmatically so every subclass viewer
    // gets the feature without editing each .ui file.
    m_selectionOverlay = new PdfTextSelectionOverlay(m_pdfView, m_doc, this);

    m_selectModeBtn = new QPushButton(tr("Výběr textu"), this);
    m_selectModeBtn->setCheckable(true);
    m_selectModeBtn->setToolTip(
        tr("Přepnout mezi posunem a výběrem textu. Táhnutím vyberte text, Ctrl+C jej zkopíruje."));
    if (QBoxLayout* box = findManagingBox(m_zoomOutBtn ? m_zoomOutBtn : m_zoomInBtn)) {
        QWidget* anchor = m_zoomOutBtn ? m_zoomOutBtn : m_zoomInBtn;
        const int idx = box->indexOf(anchor);
        if (idx >= 0)
            box->insertWidget(idx + 1, m_selectModeBtn);
        else
            box->addWidget(m_selectModeBtn);
    }
    connect(m_selectModeBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_selectionOverlay)
            m_selectionOverlay->setSelectMode(on);
    });

    m_doc->load(pdfPath);
}


/**
 * @brief Apply a relative zoom @p factor while keeping the cursor anchor in place.
 *
 * Steps:
 *   1. Validate @p factor and clamp the new zoom into [#MIN_ZOOM, #MAX_ZOOM].
 *   2. Pick an anchor point — the mouse cursor if it is over the viewport,
 *      otherwise the viewport center (e.g. for toolbar zoom buttons).
 *   3. Convert the anchor to "absolute document" coordinates using the
 *      current scrollbar offsets.
 *   4. Apply the zoom and shift the scrollbars so the anchor stays put.
 */
void PdfViewerWindow::applyZoom(double factor)
{
    if (factor <= 0 || qIsNaN(factor) || qIsInf(factor))
        return;

    double current = m_pdfView->zoomFactor();
    double newZoom = qBound(MIN_ZOOM, current * factor, MAX_ZOOM);

    if (qAbs(newZoom - current) < 0.001)
        return;

    double ratio = newZoom / current;

    QScrollBar *hBar = m_pdfView->horizontalScrollBar();
    QScrollBar *vBar = m_pdfView->verticalScrollBar();

    // Figure out where to anchor the zoom
    QPoint globalCursorPos = QCursor::pos();
    QPoint viewportPos = m_pdfView->viewport()->mapFromGlobal(globalCursorPos);
    QPointF center;

    // If the mouse cursor is over the PDF, zoom to the cursor.
    // Otherwise, zoom to the center of the viewport (e.g., when using toolbar buttons).
    if (m_pdfView->viewport()->rect().contains(viewportPos)) {
        center = viewportPos;
    } else {
        center = QPointF(m_pdfView->viewport()->width() / 2.0,
                         m_pdfView->viewport()->height() / 2.0);
    }

    // Calculate absolute point before zooming
    double oldX = hBar->value() + center.x();
    double oldY = vBar->value() + center.y();

    // Apply the zoom
    m_pdfView->setZoomFactor(newZoom);

    // Shift scrollbars to keep the anchor point in the same spot
    hBar->setValue(qRound(oldX * ratio - center.x()));
    vBar->setValue(qRound(oldY * ratio - center.y()));

    // Keep the text-selection highlight aligned with the rescaled page.
    if (m_selectionOverlay)
        m_selectionOverlay->notifyZoomChanged();
}

/** @brief Zoom in by 20%. */
void PdfViewerWindow::zoomIn()  { applyZoom(1.2); }

/** @brief Zoom out by ~9% (1 / 1.1). */
void PdfViewerWindow::zoomOut() { applyZoom(1.0 / 1.1); }

/** @brief Navigate to the previous page (no-op at page 0). */
void PdfViewerWindow::prevPage() { jumpToPage0(m_nav->currentPage() - 1); }

/** @brief Navigate to the next page (no-op at the last page). */
void PdfViewerWindow::nextPage() { jumpToPage0(m_nav->currentPage() + 1); }

/**
 * @brief Spin-box slot — convert from 1-based to 0-based and jump.
 */
void PdfViewerWindow::pageSpinChanged(int page1based)
{
    jumpToPage0(page1based - 1);
}

/**
 * @brief Slot reacting to QPdfDocument::pageCountChanged.
 *
 * Updates the "/ N" label and adjusts the spin-box maximum to match the
 * new page count.
 */
void PdfViewerWindow::onPageCountChanged(int pageCount)
{
    m_pageCountLabel->setText(QString("/ %1").arg(pageCount));
    m_pageSpin->setMaximum(qMax(1, pageCount));
    updateUiEnabled();
}

/**
 * @brief Update the page spin-box without re-emitting valueChanged.
 */
void PdfViewerWindow::onCurrentPageChanged(int page0based)
{
    m_pageSpin->blockSignals(true);
    m_pageSpin->setValue(page0based + 1);
    m_pageSpin->blockSignals(false);
    updateUiEnabled();

    if (m_selectionOverlay)
        m_selectionOverlay->notifyPageChanged(page0based);
}

/**
 * @brief Jump to a 0-based page index after bounds-checking.
 *
 * In single-page mode, if the user was zoomed in we reset the zoom to 1.0
 * before jumping so each page is shown fitted to the viewport — otherwise
 * the next page would be scrolled off-screen.
 */
void PdfViewerWindow::jumpToPage0(int page0based)
{
    if (page0based >= 0 && page0based < m_doc->pageCount()) {
        const bool isSinglePage = (m_pdfView->pageMode() == QPdfView::PageMode::SinglePage);
        if (isSinglePage && m_pdfView->zoomFactor() > 1.0 + 0.001)
            m_pdfView->setZoomFactor(1.0);
        m_nav->jump(page0based, QPoint(), m_nav->currentZoom());
    }
}

/**
 * @brief Refresh enabled state of the prev/next buttons and the page spin box.
 */
void PdfViewerWindow::updateUiEnabled()
{
    int current = m_nav->currentPage();
    int count   = m_doc->pageCount();
    m_prevBtn->setEnabled(current > 0);
    m_nextBtn->setEnabled(current < count - 1);
    m_pageSpin->setEnabled(count > 0);
}

/**
 * @brief Double-click zoom: left = +10%, right = -10%, middle = reset to 1.0.
 */
void PdfViewerWindow::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        applyZoom(1.1);
    else if (event->button() == Qt::RightButton)
        applyZoom(1.0 / 1.1);
    else if (event->button() == Qt::MiddleButton)
        m_pdfView->setZoomFactor(1.0);
    else
        QWidget::mouseDoubleClickEvent(event);

    event->accept();
}

/**
 * @brief Wheel handler — Ctrl+wheel zooms, plain wheel scrolls.
 *
 * Both branches are throttled by elapsed timers so high-resolution
 * touchpads or mice don't queue more zoom/scroll events than the renderer
 * can keep up with. The Ctrl+wheel branch returns early so the base class
 * never sees the event and cannot also scroll the view.
 */
void PdfViewerWindow::wheelEvent(QWheelEvent *event)
{
    // ── Guard: ignore empty events ───────────────────────────────
    if (event->angleDelta().isNull() && event->pixelDelta().isNull()) {
        event->accept();
        return;
    }

    if (event->modifiers() & Qt::ControlModifier)
    {
        // ── Throttle zoom ────────────────────────────────────────
        if (m_zoomTimer.elapsed() < ZOOM_THROTTLE_MS) {
            event->accept();
            return;
        }
        m_zoomTimer.restart();

        // ── Prefer pixelDelta for touchpad ───────────────────────
        int delta = !event->pixelDelta().isNull()
                        ? event->pixelDelta().y()
                        : event->angleDelta().y();

        if (delta > 0)
            applyZoom(1.05);
        else if (delta < 0)
            applyZoom(1.0 / 1.05);

        event->accept();
        return;  // Do NOT fall through to base class — that would scroll the view
    }

    // ── Throttle scroll ──────────────────────────────────────────
    if (m_scrollTimer.elapsed() < SCROLL_THROTTLE_MS) {
        event->accept();
        return;
    }
    m_scrollTimer.restart();

    QWidget::wheelEvent(event);
}

/**
 * @brief Generic event override — dispatches QPinchGesture to applyZoom().
 *
 * Gesture delivery uses the same throttle timer as the wheel zoom path
 * to keep the two input modes consistent.
 */
bool PdfViewerWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Gesture)
    {
        QGestureEvent *gestureEvent = static_cast<QGestureEvent*>(event);
        if (QPinchGesture *pinch = static_cast<QPinchGesture*>(
                gestureEvent->gesture(Qt::PinchGesture)))
        {
            if (m_zoomTimer.elapsed() >= ZOOM_THROTTLE_MS)
            {
                double scale = pinch->scaleFactor();
                if (scale != 1.0)
                    applyZoom(scale);
                m_zoomTimer.restart();
            }
            return true;
        }
    }
    return QWidget::event(event);
}

/**
 * @brief Top-level key shortcuts: +/- zoom, 0 reset, Esc close.
 *
 * Arrow-key navigation is handled in eventFilter() because it has to
 * intercept events before QPdfView's own scrolling kicks in.
 */
void PdfViewerWindow::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+C copies the current text selection (if any) to the clipboard.
    if (event->matches(QKeySequence::Copy)) {
        if (m_selectionOverlay)
            m_selectionOverlay->copySelectionToClipboard();
        event->accept();
        return;
    }

    switch (event->key())
    {
    case Qt::Key_Plus:
        applyZoom(1.2);
        break;
    case Qt::Key_Minus:
        applyZoom(1.0 / 1.2);
        break;
    case Qt::Key_0:
        m_pdfView->setZoomFactor(1.0);
        break;
    case Qt::Key_Escape:
        close();
        break;

    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

/**
 * @brief Decide whether arrow keys should turn pages or scroll the view.
 *
 * Heuristic:
 *   - Multi-page mode: arrows turn pages while zoom ≤ 1.10, otherwise
 *     they scroll the page contents.
 *   - Single-page mode: arrows turn pages only when at least 90% of the
 *     page height is visible — i.e. the user isn't reading a zoomed-in
 *     section that they would lose by paging.
 */
bool PdfViewerWindow::shouldAllowArrowPageTurn() const
{
    const bool isMultiPage = (m_pdfView->pageMode() == QPdfView::PageMode::MultiPage);

    if (isMultiPage) {
        return m_pdfView->zoomFactor() <= 1.10;
    } else {
        // In single-page mode: allow page turn only when ≥90% of the page height is visible
        int page = m_nav->currentPage();
        if (page < 0 || page >= m_doc->pageCount())
            return true;

        QSizeF pageSize = m_doc->pagePointSize(page); // in points
        if (pageSize.height() <= 0)
            return true;

        // Scale page height to viewport pixels using current zoom
        double zoom         = m_pdfView->zoomFactor();
        double pageHeightPx = pageSize.height() * zoom;
        double viewportH    = static_cast<double>(m_pdfView->viewport()->height());

        return (viewportH / pageHeightPx) >= 0.9;
    }
}

/**
 * @brief Filter events on the QPdfView and its viewport.
 *
 * Implements three pieces of behaviour the default QPdfView lacks:
 *   - Ctrl+wheel zoom interception so QPdfView never scrolls instead.
 *   - Keyboard navigation: Up/Down scroll, Space (Shift+Space) next/prev,
 *     Left/Right scroll-or-turn-page driven by shouldAllowArrowPageTurn().
 *   - Click-and-drag panning with a closed-hand cursor.
 *
 * Returns true to consume an event after we have handled it.
 */
bool PdfViewerWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Accept events from pdfView itself OR its internal viewport child
    const bool isPdfView  = (watched == m_pdfView);
    const bool isViewport = (watched == m_pdfView->viewport());
    if (!isPdfView && !isViewport)
        return QWidget::eventFilter(watched, event);

    // ───────── WHEEL (block Ctrl+scroll from scrolling the view) ─────────
    if (event->type() == QEvent::Wheel)
    {
        QWheelEvent *we = static_cast<QWheelEvent*>(event);
        if (we->modifiers() & Qt::ControlModifier)
        {
            // Forward to our own wheelEvent handler which does zoom-only, then
            // return true so QPdfView never sees it and cannot scroll.
            wheelEvent(we);
            return true;
        }
    }

    // ───────── KEYBOARD ─────────
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        int scrollStep = 50; // Tweak this value for faster/slower keyboard scrolling

        switch (keyEvent->key())
        {
        case Qt::Key_Up:
            m_pdfView->verticalScrollBar()->setValue(
                m_pdfView->verticalScrollBar()->value() - scrollStep);
            return true;

        case Qt::Key_Down:
            m_pdfView->verticalScrollBar()->setValue(
                m_pdfView->verticalScrollBar()->value() + scrollStep);
            return true;

        case Qt::Key_Space:
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                prevPage();
            else
                nextPage();
            return true;

        case Qt::Key_Right:
            if (m_pdfView->zoomFactor() > 1.0 + 0.001) {
                m_pdfView->horizontalScrollBar()->setValue(
                    m_pdfView->horizontalScrollBar()->value() + scrollStep);
            } else if (shouldAllowArrowPageTurn()) {
                nextPage();
            }
            return true;

        case Qt::Key_Left:
            if (m_pdfView->zoomFactor() > 1.0 + 0.001) {
                m_pdfView->horizontalScrollBar()->setValue(
                    m_pdfView->horizontalScrollBar()->value() - scrollStep);
            } else if (shouldAllowArrowPageTurn()) {
                prevPage();
            }
            return true;

        default:
            break;
        }
    }

    // ───────── MOUSE PRESS (start panning) ─────────
    // Skip panning while text selection is active — the overlay handles the
    // drag then (it normally also consumes the event before it reaches here).
    const bool selecting = m_selectionOverlay && m_selectionOverlay->selectMode();
    if (event->type() == QEvent::MouseButtonPress && !selecting)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);

        if (me->button() == Qt::LeftButton)
        {
            m_isPanning = true;
            m_panStart  = me->globalPosition();
            m_pdfView->viewport()->setCursor(Qt::ClosedHandCursor);
            return true;
        }
    }

    // ───────── MOUSE MOVE (pan) ─────────
    if (event->type() == QEvent::MouseMove && m_isPanning)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);

        QPointF delta = me->globalPosition() - m_panStart;
        m_panStart = me->globalPosition();

        auto *hBar = m_pdfView->horizontalScrollBar();
        auto *vBar = m_pdfView->verticalScrollBar();

        hBar->setValue(hBar->value() - delta.x());
        vBar->setValue(vBar->value() - delta.y());

        return true;
    }

    // ───────── MOUSE RELEASE (stop panning) ─────────
    if (event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);

        if (me->button() == Qt::LeftButton && m_isPanning)
        {
            m_isPanning = false;
            m_pdfView->viewport()->setCursor(Qt::ArrowCursor);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}
