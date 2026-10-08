#ifndef CONTROLPDF_H
#define CONTROLPDF_H

#include <QString>
#include <vector>

#include "structures.h"

class FilePDF;
class GeometricPlan;

/**
 * @class ControlPDF
 * @brief Runs the standard PDF conformance checks over a whole GeometricPlan.
 *
 * Does not own any data — it binds to an existing GeometricPlan (the single
 * source of truth) and inspects each of its standard PDF slots. For every
 * present file it produces a PdfControlReport describing how the file measures
 * up against the cadastral requirements:
 *
 * - PDF version          — must be ≥ 1.7
 * - PDF/A                 — must be PDF/A with version ≥ 2 (version text shown)
 * - Number of signatures  — 1 for the GP and Žádost, 0 for everything else
 * - Can add signature     — must be allowed
 * - Number of pages       — informational
 * - Page formats          — A4 only, except the GP and Náčrt which may be A1–A4
 *
 * The Ověření (verification) file is intentionally excluded from the checks.
 *
 * Performs no formatting — pass the result to
 * DisplayResults::generatePdfReport() to turn it into printable text.
 */
class ControlPDF
{
public:
    /**
     * @brief Bind the controller to @p plan.
     * @param plan GeometricPlan that owns the file data; must outlive this controller.
     */
    explicit ControlPDF(const GeometricPlan& plan);

    /**
     * @brief Check every standard PDF slot of the bound plan (except Ověření).
     * @return One PdfControlReport per file that is present in the plan.
     */
    std::vector<PdfControlReportFormat> control_PDF() const;

    PdfControlReportFormat control_single_PDF(FilePDF const &file);
    /**
     * @brief Summarise the non-PDF members of the bound plan.
     *
     * Inspects the VFK file, the signature files (AZI1–AZI3), the GNSS
     * measurement, the verification file (Ověření) and any additional
     * files, reporting presence, name and format for each.
     *
     * @return A single NonPdfControlReport describing the whole plan.
     */
    NonPdfControlReport control_nonPDF() const;

private:
    /**
     * @brief Build the report for a single PDF file.
     * @param file               File to inspect.
     * @param categoryKey        NAMESMAP key used to resolve the document label.
     * @param expectedSignatures Required number of signatures for this slot.
     * @param largeFormatAllowed Whether A1–A4 are accepted (true) or only A4 (false).
     */
    PdfControlReportFormat controlFile(const FilePDF& file,
                                 const QString& categoryKey,
                                 int expectedSignatures,
                                 bool largeFormatAllowed) const;

    struct Slot {
        const FilePDF* file;
        QString        key;
        int            expectedSignatures;
        bool           largeFormat;
    };

    const GeometricPlan* m_plan;    ///< Plan whose PDF slots are being checked.
};

#endif // CONTROLPDF_H
