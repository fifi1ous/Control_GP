/**
 * @file displaygnss.cpp
 * @brief Implementation of DisplayGNSS.
 */

#include "displaygnss.h"
#include "ui_displaygnss.h"

/**
 * @brief Build the DisplayGNSS UI, populate base-class widget pointers and load @p path in multi-page mode.
 *
 * Mirrors the DisplayGP constructor but does not attach an overlay.
 */
DisplayGNSS::DisplayGNSS(const QString &path, QWidget *parent)
    : PdfViewerWindow(parent)
{
    m_displayUi = new Ui::DisplayGNSS;
    m_displayUi->setupUi(this);

    // Populate base-class widget pointers from our own UI
    m_pdfView        = m_displayUi->pdfView;
    m_zoomInBtn      = m_displayUi->zoomInBtn;
    m_zoomOutBtn     = m_displayUi->zoomOutBtn;
    m_prevBtn        = m_displayUi->prevBtn;
    m_nextBtn        = m_displayUi->nextBtn;
    m_pageSpin       = m_displayUi->pageSpin;
    m_pageCountLabel = m_displayUi->pageCountLabel;

    initViewer(path, QPdfView::PageMode::MultiPage);
}

/**
 * @brief Destructor — frees the .ui object.
 */
DisplayGNSS::~DisplayGNSS()
{
    delete m_displayUi;
}

/**
 * @brief Forward the page change to the base class. Override hook for future GNSS-specific logic.
 */
void DisplayGNSS::onCurrentPageChanged(int page0based)
{
    // Let the base class update the page spin / nav as usual
    PdfViewerWindow::onCurrentPageChanged(page0based);
}

/**
 * @brief Forward the zoom request to the base class. Override hook for future GNSS-specific logic.
 */
void DisplayGNSS::applyZoom(double factor)
{
    // Let the base class do the actual zoom
    PdfViewerWindow::applyZoom(factor);
}
