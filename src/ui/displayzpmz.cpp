/**
 * @file displayzpmz.cpp
 * @brief Implementation of DisplayZPMZ.
 */

#include "displayzpmz.h"
#include "ui_displayzpmz.h"

#include <QComboBox>
#include <QSignalBlocker>

#include "maps.h"   // NAMESMAP — category key → Czech label (single source of truth)

namespace {

/**
 * @brief Document classes offered in the reclassification dropdown, in workflow order.
 *
 * Each entry is an internal category key matching FilePDF::what_it_is; the
 * human-readable label is looked up in NAMESMAP. GNSS and VFK are deliberately
 * absent, so opening such a file leaves the dropdown with no selection.
 */
const QStringList& categoryKeys()
{
    static const QStringList keys = {
        QStringLiteral("popispole"), QStringLiteral("nacrt"), QStringLiteral("zap"),
        QStringLiteral("prot"), QStringLiteral("vymery"), QStringLiteral("sezvlast"),
        QStringLiteral("oprav"), QStringLiteral("dsps"), QStringLiteral("vytyc"),
        QStringLiteral("gp"), QStringLiteral("zadost")
    };
    return keys;
}

} // namespace

/**
 * @brief Build the DisplayZPMZ UI, populate base-class widget pointers and load @p path in multi-page mode.
 *
 * Mirrors the DisplayGP constructor but does not attach an overlay.
 */
DisplayZPMZ::DisplayZPMZ(const QString &path, QWidget *parent)
    : PdfViewerWindow(parent)
{
    m_displayUi = new Ui::DisplayZPMZ;
    m_displayUi->setupUi(this);

    // Populate base-class widget pointers from our own UI
    m_pdfView        = m_displayUi->pdfView;
    m_zoomInBtn      = m_displayUi->zoomInBtn;
    m_zoomOutBtn     = m_displayUi->zoomOutBtn;
    m_prevBtn        = m_displayUi->prevBtn;
    m_nextBtn        = m_displayUi->nextBtn;
    m_pageSpin       = m_displayUi->pageSpin;
    m_pageCountLabel = m_displayUi->pageCountLabel;

    // Fill the class dropdown: visible text is the Czech label, the item data
    // carries the internal category key. Start with no selection until the
    // caller sets the current file's category via setCurrentCategory().
    for (const QString& key : categoryKeys())
        m_displayUi->comboBox->addItem(NAMESMAP.value(key, key), key);
    m_displayUi->comboBox->setCurrentIndex(-1);

    // Re-broadcast a user-driven selection as categoryChanged(<key>).
    connect(m_displayUi->comboBox, &QComboBox::currentIndexChanged, this,
            [this](int) { Q_EMIT categoryChanged(m_displayUi->comboBox->currentData().toString()); });

    initViewer(path, QPdfView::PageMode::MultiPage);
}

/**
 * @brief Select the class whose item data equals @p categoryKey (or clear the selection).
 *
 * Signals are blocked while the index is set so that this programmatic update
 * is not mistaken for a user reclassification.
 */
void DisplayZPMZ::setCurrentCategory(const QString& categoryKey)
{
    const int index = m_displayUi->comboBox->findData(categoryKey);
    const QSignalBlocker block(m_displayUi->comboBox);
    m_displayUi->comboBox->setCurrentIndex(index);  // -1 when not found (e.g. GNSS)
}

/**
 * @brief Destructor — frees the .ui object.
 */
DisplayZPMZ::~DisplayZPMZ()
{
    delete m_displayUi;
}

/**
 * @brief Forward the page change to the base class. Override hook for future ZPMZ-specific logic.
 */
void DisplayZPMZ::onCurrentPageChanged(int page0based)
{
    // Let the base class update the page spin / nav as usual
    PdfViewerWindow::onCurrentPageChanged(page0based);
}

/**
 * @brief Forward the zoom request to the base class. Override hook for future ZPMZ-specific logic.
 */
void DisplayZPMZ::applyZoom(double factor)
{
    // Let the base class do the actual zoom
    PdfViewerWindow::applyZoom(factor);
}