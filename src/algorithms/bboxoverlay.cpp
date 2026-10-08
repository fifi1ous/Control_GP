#include "bboxoverlay.h"

#include <QPdfView>
#include <QPdfDocument>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFontMetrics>
#include <QEvent>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QtMath>
#include <QDebug>
#include <QCursor>
#include <qpdfpagenavigator.h>

#include "maps.h"

// ---------------------------------------------------------------------------
//  Constructor
// ---------------------------------------------------------------------------
BBoxOverlay::BBoxOverlay(QPdfView* pdfView, QPdfDocument* doc, QWidget* parent)
    : QWidget(parent)
    , m_pdfView(pdfView)
    , m_doc(doc)
{
    Q_ASSERT(pdfView);
    Q_ASSERT(doc);

    // The QPdfView is a QAbstractScrollArea; its actual drawing surface is
    // the viewport().  We reparent ourselves onto the viewport so we sit
    // exactly on top of the rendered PDF.
    m_viewport = pdfView->viewport();
    setParent(m_viewport);
    setAttribute(Qt::WA_TransparentForMouseEvents); // pass all events through
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(m_viewport->rect());
    show();
    raise();

    // Watch the viewport and the scroll bars for size / scroll changes.
    m_viewport->installEventFilter(this);
    if (auto* sa = qobject_cast<QAbstractScrollArea*>(pdfView)) {
        if (sa->horizontalScrollBar())
            sa->horizontalScrollBar()->installEventFilter(this);
        if (sa->verticalScrollBar())
            sa->verticalScrollBar()->installEventFilter(this);
    }
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void BBoxOverlay::setFillAlpha(int alpha)
{
    m_fillAlpha = qBound(0, alpha, 255);
    update();
}

void BBoxOverlay::setEditMode(bool on)
{
    m_editMode = on;
    m_selectedBox  = -1;
    m_dragging     = false;
    m_activeHandle = HandleNone;

    // When editing, we need mouse events; otherwise let them pass through.
    setAttribute(Qt::WA_TransparentForMouseEvents, !on);
    setMouseTracking(on);

    if (!on)
        unsetCursor();

    update();
}

void BBoxOverlay::notifyPageChanged(int page0based)
{
    m_currentPage = page0based;
    m_selectedBox = -1;
    m_dragging    = false;
    update();
}

void BBoxOverlay::notifyZoomChanged()
{
    update();
}

void BBoxOverlay::reloadAnnotations()
{
    // Forget every cached page so boxesForPage() re-reads the .txt files,
    // then clear any edit selection and repaint with the fresh detections.
    m_cache.clear();
    m_selectedBox = -1;
    m_dragging    = false;
    update();
}

// ---------------------------------------------------------------------------
//  Event filter – keep overlay covering the viewport; repaint on scroll
// ---------------------------------------------------------------------------
bool BBoxOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_viewport) {
        if (event->type() == QEvent::Resize) {
            setGeometry(m_viewport->rect());
        }
    }
    // Repaint whenever scroll position changes or the view is resized/shown.
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

// ---------------------------------------------------------------------------
//  Paint
// ---------------------------------------------------------------------------
void BBoxOverlay::paintEvent(QPaintEvent*)
{
    if (!m_doc || m_doc->pageCount() == 0 || m_annotationDir.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QFont labelFont = p.font();
    labelFont.setPointSize(8);
    labelFont.setBold(true);
    p.setFont(labelFont);
    QFontMetrics fm(labelFont);

    // In MultiPage mode QPdfView can show several pages; we draw boxes for
    // every page whose rect intersects the visible area.  For simplicity we
    // iterate over all pages and skip those whose mapped rect is off-screen.
    int pg = m_pdfView->pageNavigator()->currentPage();

    const QVector<BBox>& boxes = boxesForPage(pg);
    for (int i = 0; i < boxes.size(); ++i)
    {
        const BBox& box = boxes[i];
        QRectF r = pdfRectToOverlay(pg, box);

        if (!r.isValid() || !rect().toRectF().intersects(r))
            continue;

        QColor base = colorForClass(box.classId);
        QString label = labelForClass(box.classId);

        QColor fill = base;
        fill.setAlpha(m_fillAlpha);
        p.fillRect(r, fill);

        // Highlight the selected box with a thicker dashed border
        bool isSelected = m_editMode && (i == m_selectedBox);

        QPen pen(base, isSelected ? 3.0 : 2.0);
        if (isSelected) {
            pen.setStyle(Qt::DashLine);
            pen.setColor(Qt::white);
        }
        p.setPen(pen);
        p.drawRect(r);

        // If selected, also draw the normal-color border underneath
        if (isSelected) {
            QPen basePen(base, 2.0);
            p.setPen(basePen);
            p.drawRect(r);
        }

        QString text = label;
        QRect textRect = fm.boundingRect(text).adjusted(-3, -1, 3, 1);
        textRect.moveTopLeft(r.topLeft().toPoint() + QPoint(0, -textRect.height()));

        if (textRect.top() < 0)
            textRect.moveTop(r.top());

        QColor labelBg = base;
        labelBg.setAlpha(200);
        p.fillRect(textRect, labelBg);

        p.setPen(Qt::white);
        p.drawText(textRect, Qt::AlignCenter, text);

        // Draw resize handles for the selected box
        if (isSelected) {
            const int hs = HANDLE_SIZE;
            QColor handleColor(Qt::white);
            QColor handleBorder(base);

            auto drawHandle = [&](double cx, double cy) {
                QRectF hr(cx - hs, cy - hs, hs * 2, hs * 2);
                p.fillRect(hr, handleColor);
                p.setPen(QPen(handleBorder, 1.0));
                p.drawRect(hr);
            };

            // Corners
            drawHandle(r.left(),  r.top());
            drawHandle(r.right(), r.top());
            drawHandle(r.left(),  r.bottom());
            drawHandle(r.right(), r.bottom());
            // Edge midpoints
            drawHandle(r.center().x(), r.top());
            drawHandle(r.center().x(), r.bottom());
            drawHandle(r.left(),  r.center().y());
            drawHandle(r.right(), r.center().y());
        }
    }
}

// ---------------------------------------------------------------------------
//  Coordinate mapping
// ---------------------------------------------------------------------------
QRectF BBoxOverlay::pdfRectToOverlay(int page0based, const BBox& box) const
{
    if (!m_pdfView || !m_doc) return {};
    if (page0based < 0 || page0based >= m_doc->pageCount()) return {};

    double zoom = m_pdfView->zoomFactor();
    if (zoom <= 0) zoom = 1.0;

    double dpiScaleX = logicalDpiX() / 72.0;
    double dpiScaleY = logicalDpiY() / 72.0;

    QSizeF pageSize = m_doc->pagePointSize(page0based);
    if (pageSize.isEmpty()) return {};

    double pw = pageSize.width();
    double ph = pageSize.height();

    double rendW = pw * dpiScaleX * zoom;
    double rendH = ph * dpiScaleY * zoom;

    auto* sa = qobject_cast<QAbstractScrollArea*>(m_pdfView);
    if (!sa) return {};

    int viewportW  = sa->viewport()->width();
    int hScroll    = sa->horizontalScrollBar() ? sa->horizontalScrollBar()->value()   : 0;
    int vScroll    = sa->verticalScrollBar()   ? sa->verticalScrollBar()->value()     : 0;
    int hScrollMax = sa->horizontalScrollBar() ? sa->horizontalScrollBar()->maximum() : 0;

    double contentW  = viewportW + hScrollMax;
    double pageLeft  = (contentW - rendW) / 2.0 - hScroll;

    // Vertical offset: in SinglePage mode the current page is always at the
    // top of the viewport, so we must NOT accumulate previous page heights.
    static constexpr double PAGE_GAP = 4.0;
    double pageTop = -vScroll;
    if (m_pdfView->pageMode() == QPdfView::PageMode::MultiPage) {
        for (int i = 0; i < page0based; ++i) {
            QSizeF ps = m_doc->pagePointSize(i);
            double prevPw = ps.width();
            double prevPh = ps.height();
            if (prevPw < prevPh) std::swap(prevPw, prevPh);
            pageTop += (prevPh * dpiScaleY * zoom) + PAGE_GAP;
        }
    }

    double left   = pageLeft + (box.xMin * rendW);
    double top    = pageTop  + (box.yMin * rendH);
    double right  = pageLeft + (box.xMax * rendW);
    double bottom = pageTop  + (box.yMax * rendH);

    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

// ---------------------------------------------------------------------------
//  Edit-mode: mouse interaction
// ---------------------------------------------------------------------------
BBoxOverlay::HitResult BBoxOverlay::hitTest(const QPointF& pos) const
{
    int pg = m_pdfView->pageNavigator()->currentPage();
    const QVector<BBox>& boxes = m_cache.value(pg);

    const int hs = HANDLE_SIZE;

    // Check selected box first (handles take priority)
    auto testBox = [&](int i) -> HitResult {
        QRectF r = pdfRectToOverlay(pg, boxes[i]);
        if (!r.isValid())
            return {};

        // Check corner handles
        auto nearCorner = [&](double cx, double cy, Handle h) -> bool {
            return QRectF(cx - hs, cy - hs, hs * 2, hs * 2).contains(pos);
        };
        if (nearCorner(r.left(),     r.top(),      HandleTopLeft))     return {i, HandleTopLeft};
        if (nearCorner(r.right(),    r.top(),      HandleTopRight))    return {i, HandleTopRight};
        if (nearCorner(r.left(),     r.bottom(),   HandleBottomLeft))  return {i, HandleBottomLeft};
        if (nearCorner(r.right(),    r.bottom(),   HandleBottomRight)) return {i, HandleBottomRight};

        // Check edge midpoint handles
        if (nearCorner(r.center().x(), r.top(),      HandleTop))    return {i, HandleTop};
        if (nearCorner(r.center().x(), r.bottom(),   HandleBottom)) return {i, HandleBottom};
        if (nearCorner(r.left(),       r.center().y(), HandleLeft))  return {i, HandleLeft};
        if (nearCorner(r.right(),      r.center().y(), HandleRight)) return {i, HandleRight};

        // Check box body
        if (r.contains(pos))
            return {i, HandleMove};

        return {};
    };

    // Prioritize the already-selected box (so its handles are reachable even
    // when boxes overlap).
    if (m_selectedBox >= 0 && m_selectedBox < boxes.size()) {
        HitResult hr = testBox(m_selectedBox);
        if (hr.handle != HandleNone)
            return hr;
    }

    // Then test all other boxes (top-to-bottom draw order, last drawn on top)
    for (int i = boxes.size() - 1; i >= 0; --i) {
        if (i == m_selectedBox) continue;
        HitResult hr = testBox(i);
        if (hr.handle != HandleNone)
            return hr;
    }

    return {};
}

Qt::CursorShape BBoxOverlay::cursorForHandle(Handle h) const
{
    switch (h) {
    case HandleMove:        return Qt::SizeAllCursor;
    case HandleTopLeft:
    case HandleBottomRight: return Qt::SizeFDiagCursor;
    case HandleTopRight:
    case HandleBottomLeft:  return Qt::SizeBDiagCursor;
    case HandleTop:
    case HandleBottom:      return Qt::SizeVerCursor;
    case HandleLeft:
    case HandleRight:       return Qt::SizeHorCursor;
    default:                return Qt::ArrowCursor;
    }
}

QPointF BBoxOverlay::overlayToPdf(int page0based, const QPointF& pos) const
{
    if (!m_pdfView || !m_doc)
        return {};
    if (page0based < 0 || page0based >= m_doc->pageCount())
        return {};

    double zoom = m_pdfView->zoomFactor();
    if (zoom <= 0) zoom = 1.0;

    double dpiScaleX = logicalDpiX() / 72.0;
    double dpiScaleY = logicalDpiY() / 72.0;

    QSizeF pageSize = m_doc->pagePointSize(page0based);
    if (pageSize.isEmpty())
        return {};

    double pw = pageSize.width();
    double ph = pageSize.height();

    double rendW = pw * dpiScaleX * zoom;
    double rendH = ph * dpiScaleY * zoom;

    auto* sa = qobject_cast<QAbstractScrollArea*>(m_pdfView);
    if (!sa) return {};

    int viewportW  = sa->viewport()->width();
    int hScroll    = sa->horizontalScrollBar() ? sa->horizontalScrollBar()->value()   : 0;
    int vScroll    = sa->verticalScrollBar()   ? sa->verticalScrollBar()->value()     : 0;
    int hScrollMax = sa->horizontalScrollBar() ? sa->horizontalScrollBar()->maximum() : 0;

    double contentW = viewportW + hScrollMax;
    double pageLeft = (contentW - rendW) / 2.0 - hScroll;

    static constexpr double PAGE_GAP = 4.0;
    double pageTop = -vScroll;
    if (m_pdfView->pageMode() == QPdfView::PageMode::MultiPage) {
        for (int i = 0; i < page0based; ++i) {
            QSizeF ps = m_doc->pagePointSize(i);
            double prevPw = ps.width();
            double prevPh = ps.height();
            if (prevPw < prevPh) std::swap(prevPw, prevPh);
            pageTop += (prevPh * dpiScaleY * zoom) + PAGE_GAP;
        }
    }

    double normX = (pos.x() - pageLeft) / rendW;
    double normY = (pos.y() - pageTop)  / rendH;

    normX = qBound(0.0, normX, 1.0);
    normY = qBound(0.0, normY, 1.0);

    return QPointF(normX, normY);
}

void BBoxOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!m_editMode || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    HitResult hr = hitTest(event->position());

    if (hr.handle == HandleNone) {
        // Click on empty area — deselect
        m_selectedBox  = -1;
        m_activeHandle = HandleNone;
        update();
        return;
    }

    int pg = m_pdfView->pageNavigator()->currentPage();
    m_selectedBox   = hr.boxIndex;
    m_activeHandle  = hr.handle;
    m_dragging      = true;
    m_dragStartPdf  = overlayToPdf(pg, event->position());
    m_dragOrigBox   = m_cache.value(pg).value(hr.boxIndex);

    update();
}

void BBoxOverlay::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_editMode) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    int pg = m_pdfView->pageNavigator()->currentPage();

    if (m_dragging && m_selectedBox >= 0) {
        QPointF nowPdf = overlayToPdf(pg, event->position());
        double dx = nowPdf.x() - m_dragStartPdf.x();
        double dy = nowPdf.y() - m_dragStartPdf.y();

        QVector<BBox>& boxes = mutableBoxesForPage(pg);
        if (m_selectedBox >= boxes.size()) {
            m_dragging = false;
            return;
        }
        BBox& box = boxes[m_selectedBox];

        switch (m_activeHandle) {
        case HandleMove:
            box.xMin = m_dragOrigBox.xMin + dx;
            box.yMin = m_dragOrigBox.yMin + dy;
            box.xMax = m_dragOrigBox.xMax + dx;
            box.yMax = m_dragOrigBox.yMax + dy;
            break;
        case HandleTopLeft:
            box.xMin = m_dragOrigBox.xMin + dx;
            box.yMin = m_dragOrigBox.yMin + dy;
            break;
        case HandleTop:
            box.yMin = m_dragOrigBox.yMin + dy;
            break;
        case HandleTopRight:
            box.xMax = m_dragOrigBox.xMax + dx;
            box.yMin = m_dragOrigBox.yMin + dy;
            break;
        case HandleRight:
            box.xMax = m_dragOrigBox.xMax + dx;
            break;
        case HandleBottomRight:
            box.xMax = m_dragOrigBox.xMax + dx;
            box.yMax = m_dragOrigBox.yMax + dy;
            break;
        case HandleBottom:
            box.yMax = m_dragOrigBox.yMax + dy;
            break;
        case HandleBottomLeft:
            box.xMin = m_dragOrigBox.xMin + dx;
            box.yMax = m_dragOrigBox.yMax + dy;
            break;
        case HandleLeft:
            box.xMin = m_dragOrigBox.xMin + dx;
            break;
        default:
            break;
        }

        // Ensure min < max (prevent inverted rectangles)
        if (box.xMin > box.xMax) std::swap(box.xMin, box.xMax);
        if (box.yMin > box.yMax) std::swap(box.yMin, box.yMax);

        update();
    } else {
        // Hover: update cursor
        HitResult hr = hitTest(event->position());
        setCursor(cursorForHandle(hr.handle));
    }
}

void BBoxOverlay::mouseReleaseEvent(QMouseEvent* event)
{
    if (!m_editMode || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (m_dragging) {
        m_dragging = false;
        int pg = m_pdfView->pageNavigator()->currentPage();
        saveAnnotations(pg);
    }
}

void BBoxOverlay::saveAnnotations(int page0based)
{
    if (m_annotationDir.isEmpty())
        return;

    QString filename = QDir(m_annotationDir).filePath(
        QStringLiteral("page_%1.txt").arg(page0based + 1));

    const QVector<BBox>& boxes = m_cache.value(page0based);

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qWarning() << "BBoxOverlay: cannot write annotation file:" << filename;
        return;
    }

    QTextStream out(&file);
    for (const BBox& b : boxes) {
        out << b.classId << ' '
            << QString::number(b.xMin, 'f', 6) << ' '
            << QString::number(b.yMin, 'f', 6) << ' '
            << QString::number(b.xMax, 'f', 6) << ' '
            << QString::number(b.yMax, 'f', 6);
        if (!b.className.isEmpty())
            out << ' ' << b.className;
        out << '\n';
    }

}

// ---------------------------------------------------------------------------
//  Annotation loading / cache
// ---------------------------------------------------------------------------
QVector<BBox>& BBoxOverlay::mutableBoxesForPage(int page0based)
{
    // Ensure the page is loaded into cache first
    boxesForPage(page0based);
    return m_cache[page0based];
}

const QVector<BBox>& BBoxOverlay::boxesForPage(int page0based)
{
    auto it = m_cache.find(page0based);
    if (it != m_cache.end())
        return it.value();

    QVector<BBox>& boxes = m_cache[page0based];

    if (m_annotationDir.isEmpty())
        return boxes;

    // Convention: page_1.txt for page index 0, etc.
    QString filename = QDir(m_annotationDir).filePath(
        QStringLiteral("page_%1.txt").arg(page0based + 1));


    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // No annotation file for this page – that is fine.
        return boxes;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        QStringList parts = line.split(QRegularExpression(R"(\s+)"),
                                       Qt::SkipEmptyParts);
        if (parts.size() < 5) {
            qWarning() << "BBoxOverlay: skipping malformed line:" << line;
            continue;
        }

        BBox b;
        bool ok1, ok2, ok3, ok4, ok5;
        b.classId = parts[0].toInt(&ok1);
        b.xMin    = parts[1].toDouble(&ok2);
        b.yMin    = parts[2].toDouble(&ok3);
        b.xMax    = parts[3].toDouble(&ok4);
        b.yMax    = parts[4].toDouble(&ok5);

        if (!(ok1 && ok2 && ok3 && ok4 && ok5)) {
            qWarning() << "BBoxOverlay: non-numeric value in line:" << line;
            continue;
        }

        // Optional 6th column: human-readable class name
        if (parts.size() >= 6)
            b.className = parts[5];

        boxes.append(b);
    }

    return boxes;
}

// ---------------------------------------------------------------------------
//  Color / label helpers
// ---------------------------------------------------------------------------
QColor BBoxOverlay::paletteColor(int index)
{
    // Evenly spread hues around the colour wheel, high saturation & value.
    static const int HUE_STEP = 37;   // prime-ish, gives good spread
    int hue = (index * HUE_STEP) % 360;
    return QColor::fromHsv(hue, 220, 230);
}

QColor BBoxOverlay::colorForClass(int classId) const
{
    // Use a stable positive index so negative class IDs still work.
    return paletteColor(qAbs(classId));
}

QString BBoxOverlay::labelForClass(int classId) const
{
    const auto& names = BB_NAMES();
    auto it = names.constFind(classId);

    // Check if the key exists and if the name member has content
    if (it != names.constEnd() && !it.value().name.isEmpty()) {
        return it.value().name;
    }

    // Fallback if ID is not found or name is empty
    return QStringLiteral("cls %1").arg(classId);
}
