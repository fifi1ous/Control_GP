/**
 * @file displaygp.cpp
 * @brief Implementation of DisplayGP.
 */

#include "displaygp.h"
#include "ui_displaygp.h"

/**
 * @brief Build the DisplayGP UI, populate base-class widget pointers and attach the bounding-box overlay.
 *
 * Uses PdfViewerWindow's protected constructor (no base .ui), wires our
 * own .ui widgets into the base-class member pointers, then calls
 * initViewer() in single-page mode. After m_doc and m_pdfView are ready
 * the BBoxOverlay is constructed and the visibility/edit checkboxes are
 * connected to it.
 */
DisplayGP::DisplayGP(const QString &path, QWidget *parent)
    : PdfViewerWindow(parent)
{
    m_displayUi = new Ui::DisplayGP;
    m_displayUi->setupUi(this);

    // Populate base-class widget pointers from our own UI
    m_pdfView        = m_displayUi->pdfView;
    m_zoomInBtn      = m_displayUi->zoomInBtn;
    m_zoomOutBtn     = m_displayUi->zoomOutBtn;
    m_prevBtn        = m_displayUi->prevBtn;
    m_nextBtn        = m_displayUi->nextBtn;
    m_pageSpin       = m_displayUi->pageSpin;
    m_pageCountLabel = m_displayUi->pageCountLabel;

    initViewer(path, QPdfView::PageMode::SinglePage);

    // m_doc and m_pdfView are now ready
    m_overlay = new BBoxOverlay(m_pdfView, m_doc, this);

    // "Viditelné" checkbox toggles bounding-box visibility
    m_displayUi->visible->setChecked(true);
    connect(m_displayUi->visible, &QCheckBox::toggled,
            m_overlay, &QWidget::setVisible);

    // "Úpravy" checkbox toggles interactive edit mode (move / resize boxes)
    connect(m_displayUi->edit, &QCheckBox::toggled,
            m_overlay, &BBoxOverlay::setEditMode);
}

/**
 * @brief Destructor — frees the .ui object. The overlay is owned by Qt's parent hierarchy.
 */
DisplayGP::~DisplayGP()
{
    delete m_displayUi;
}

/**
 * @brief Reload bounding boxes from disk and repaint the overlay.
 */
void DisplayGP::reloadAnnotations()
{
    if (m_overlay)
        m_overlay->reloadAnnotations();
}

/**
 * @brief Forward the new page index to the overlay after the base class updates navigation state.
 */
void DisplayGP::onCurrentPageChanged(int page0based)
{
    // Let the base class update the page spin / nav as usual
    PdfViewerWindow::onCurrentPageChanged(page0based);

    // Then tell the overlay which page we're on
    if (m_overlay)
        m_overlay->notifyPageChanged(page0based);
}

/**
 * @brief Apply the base-class zoom and ask the overlay to repaint at the new scale.
 */
void DisplayGP::applyZoom(double factor)
{
    // Let the base class do the actual zoom
    PdfViewerWindow::applyZoom(factor);

    // Then tell the overlay to repaint at the new zoom level
    if (m_overlay)
        m_overlay->notifyZoomChanged();
}
