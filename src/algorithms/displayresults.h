#ifndef DISPLAYRESULTS_H
#define DISPLAYRESULTS_H

#include <QString>
#include <vector>

#include "structures.h"

class DisplayResults
{
public:
    DisplayResults();

    QString generateReport(const std::vector<QString>& fileNames,
                           const std::vector<QString>& predictions,
                           bool hasGnss);

    QString generateSegmentationReport(const QString& annotationDir);

    /**
     * @brief Render a PdfControlReport produced by ControlPDF into a table.
     *
     * Owns all presentation concerns (column widths, status glyphs, header):
     * the control layer hands over a plain PdfControlReport and this turns it
     * into the same ASCII-table style used by the other reports.
     *
     * @param report Structured control result to format.
     * @return Printable, pre-formatted text ready for the results view.
     */
    QString generatePdfReport(const PdfControlReportFormat& report);

    /**
     * @brief Render every report produced by ControlPDF::control().
     *
     * Formats each file's report with generatePdfReport() and joins them with
     * a blank line, so a whole GeometricPlan can be printed in one call.
     *
     * @param reports Per-file control results.
     * @return Printable, pre-formatted text ready for the results view.
     */
    QString generatePdfReport(const std::vector<PdfControlReportFormat>& reports);

    /**
     * @brief Render the non-PDF summary produced by ControlPDF::control_nonPDF().
     *
     * Reports the VFK file, the signature files (AZI1–AZI3), whether GNSS was
     * used, whether the signatures were verified (Ověření) and any additional
     * files — listing name and format, plus the predicted class for PDFs.
     *
     * @param report Structured non-PDF summary to format.
     * @return Printable, pre-formatted text ready for the results view.
     */
    QString generateNonPdfReport(const NonPdfControlReport& report);

    /**
     * @brief Render the SS coordinate cross-check produced by
     *        ControlGeometricPlan::controlSS().
     *
     * Prints one block per survey point. Each block has a heading with the
     * verdict and a four-row table: one row for each of the three sources —
     * the geometric-plan drawing (náčrt), the VFK file and the protocol —
     * listing every checked field (both coordinate pairs, their quality codes
     * and the description), followed by a "Rozdíl" row showing the numeric
     * difference for the coordinates and whether the remaining fields agree.
     * When the náčrt coordinates came from OCR (Tesseract) instead of a text
     * block, a warning is shown. A summary line with the matched / differing /
     * incomplete counts is appended at the end.
     *
     * @param report Structured cross-check result to format.
     * @return Printable, pre-formatted text ready for the results view.
     */
    QString generateSSReport(const SSControlReport& report);

    /**
     * @brief Render the area-statement cross-check produced by
     *        ControlGeometricPlan::controlVymery().
     *
     * Prints the old (dosavadní) state and then the new (nový) state, one block
     * per parcel. Each block starts with a heading
     * "Parcela <cislovani> <kmenové>/<poddělení>" (the "/poddělení" only when the
     * parcel has one) carrying the verdict, followed by a table with one row per
     * source — labelled from maps.h NAMESMAP — listing the parcel number, the
     * area, the method of area determination (způsob určení) and the land type
     * (druh pozemku). After every parcel a closing section reports whether the
     * old and new totals balance within tolerance.
     *
     * @param report Structured cross-check result to format.
     * @return Printable, pre-formatted text ready for the results view.
     */
    QString generateVymeryReport(const VymeryControlReport& report);
};

#endif // DISPLAYRESULTS_H